/***************************************************************************
    copyright            : (C) 2007 by mean
    email                : fixounet@free.fr
    

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <cmath>
#include "ADM_default.h"
#include "ADM_toneMapper.h" 
#include "prefs.h"
#include "ADM_image.h"
#include "ADM_colorspace.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
}

unsigned int ADMToneMapperConfig::method;
float ADMToneMapperConfig::saturation;
float ADMToneMapperConfig::boost;
bool ADMToneMapperConfig::adaptive;
unsigned int ADMToneMapperConfig::gamut;

ADMToneMapperConfig::ADMToneMapperConfig(bool init)
{
    changed = false;
    if (!prefs->get(HDR_TARGET_LUMINANCE,&luminance))
        luminance = DEFAULT_TARGET_LUMINANCE_HDR;
    if (!init) return;

    if (!prefs->get(HDR_TONEMAPPING,&method))
        method = 1;
    saturation = 1;
    boost = 1;
    adaptive = true;
    gamut = 0;
}

void ADMToneMapperConfig::getConfig(uint32_t * toneMappingMethod, float * saturationAdjust, float * boostAdjust, bool * adaptiveRGB, uint32_t * gamutMethod, float * targetLuminance)
{
    if (toneMappingMethod)
        *toneMappingMethod = method;
    if (saturationAdjust)
        *saturationAdjust = saturation;
    if (boostAdjust)
        *boostAdjust = boost;
    if (adaptiveRGB)
        *adaptiveRGB = adaptive;
    if (gamutMethod)
        *gamutMethod = gamut;
    if (!targetLuminance)
        return;

    if(changed)
    {
        if(!prefs->get(HDR_TARGET_LUMINANCE,&luminance))
            luminance = DEFAULT_TARGET_LUMINANCE_HDR;
        changed = false;
    }
    *targetLuminance = luminance;
}

void ADMToneMapperConfig::setConfig(uint32_t toneMappingMethod, float saturationAdjust, float boostAdjust, bool adaptiveRGB, uint32_t gamutMethod)
{
    method = toneMappingMethod;
    saturation = saturationAdjust;
    boost = boostAdjust;
    adaptive = adaptiveRGB;
    changed = true;
    gamut = gamutMethod;
}



#define CONTEXTYUV (SwsContext *)contextYUV
#define CONTEXTRGB1 (SwsContext *)contextRGB1
#define CONTEXTRGB2 (SwsContext *)contextRGB2

/**
    \fn  ADMToneMapper
    \brief Ctor
*/
ADMToneMapper::ADMToneMapper(int sws_flag,
            int sw, int sh,
            int dw, int dh,
            ADM_pixelFormat from,ADM_pixelFormat to)
{
    config = new ADMToneMapperConfig();
    contextYUV=NULL;
    contextRGB1=NULL;
    contextRGB2=NULL;
    hdrLumaLUT = NULL;
    hdrRGBLUT = NULL;
    hdrGammaLUT = NULL;
    linearizeLUT = NULL;
    for (int i=0; i<256; i++)
    {
        hdrChromaBLUT[i] = NULL;
        hdrChromaRLUT[i] = NULL;
        hdrLumaCrLUT[i] = NULL;
    }
    hdrTMsrcLum = hdrTMtrgtLum = hdrTMsat = hdrTMboost = -1.0;	// invalidate
    hdrTMmethod = 0;
    hdrYUV=NULL;
    for (int i=0; i<3; i++)
    {
        hdrYCbCr[i]=NULL;
    }
    sdrRGB=NULL;
    
    this->sws_flag=sws_flag;

    srcWidth=sw;
    srcHeight=sh;

    dstWidth=dw;
    dstHeight=dh;

    ADM_assert(to==ADM_PIXFRMT_YV12);
    fromPixFrmt=from;
    toPixFrmt=to;

    AVPixelFormat lavFrom=ADMPixFrmt2LAVPixFmt(fromPixFrmt );
    AVPixelFormat lavTo=ADMPixFrmt2LAVPixFmt(toPixFrmt );
    
    contextYUV=(void *)sws_getContext(
                      srcWidth,srcHeight,
                      lavFrom ,
                      dstWidth,dstHeight,
                      AV_PIX_FMT_YUV420P16LE,
                      sws_flag, NULL, NULL,NULL);

    contextRGB1=(void *)sws_getContext(
                      srcWidth,srcHeight,
                      lavFrom ,
                      srcWidth,srcHeight,
                      AV_PIX_FMT_YUV444P16LE,
                      SWS_POINT, NULL, NULL,NULL);		// use fast n.n. scaling, as resolution will not change in the first step
    contextRGB2=(void *)sws_getContext(
                      srcWidth,srcHeight,
                      AV_PIX_FMT_BGR24 ,
                      dstWidth,dstHeight,
                      lavTo,
                      sws_flag, NULL, NULL,NULL);
    const int *coeffsrc = sws_getCoefficients(SWS_CS_BT2020);
    const int *coeffdst = sws_getCoefficients(SWS_CS_BT2020);
    sws_setColorspaceDetails(CONTEXTRGB1, coeffsrc, 0, coeffdst, 0, 0, (1 << 16), (1 << 16));
    coeffsrc = sws_getCoefficients(SWS_CS_ITU709);
    coeffdst = sws_getCoefficients(SWS_CS_ITU709);
    sws_setColorspaceDetails(CONTEXTRGB2, coeffsrc, 0, coeffdst, 0, 0, (1 << 16), (1 << 16));
    
    threadCount = ADM_cpu_num_processors();
    if (threadCount < 1)
        threadCount = 1;
    if (threadCount > 64)
        threadCount = 64;
    if (threadCount > 4)
        threadCount = (threadCount-4)/2 + 4;
    worker_threads = new pthread_t [threadCount];
    fastYUV_worker_thread_args = new fastYUV_worker_thread_arg [threadCount];
    RGB_worker_thread_args = new RGB_worker_thread_arg [threadCount];
    
    adaptLLAvg = -1.0;
    adaptLLMax = -1.0;
    adaptHistoPrev = NULL;
    adaptHistoCurr = NULL;
}


/**
    \fn  ~ADMToneMapper
    \brief Destructor
*/
ADMToneMapper::~ADMToneMapper()
{
    delete config;
    if(contextYUV)
    {
        sws_freeContext(CONTEXTYUV);
        contextYUV=NULL;
    }
    if(contextRGB1)
    {
        sws_freeContext(CONTEXTRGB1);
        contextRGB1=NULL;
    }
    if(contextRGB2)
    {
        sws_freeContext(CONTEXTRGB2);
        contextRGB2=NULL;
    }

    delete [] hdrLumaLUT;
    delete [] hdrRGBLUT;
    delete [] hdrGammaLUT;
    delete [] linearizeLUT;
    for (int i=0; i<256; i++)
    {
        delete [] hdrChromaBLUT[i];
        delete [] hdrChromaRLUT[i];
        delete [] hdrLumaCrLUT[i];
    }
    if (hdrYUV)
    {
        delete [] hdrYUV;
        hdrYUV = NULL;
    }
    for (int i=0; i<3; i++)
    {
        delete [] hdrYCbCr[i];
    }
    delete [] sdrRGB;
    delete [] worker_threads;
    delete [] fastYUV_worker_thread_args;
    delete [] RGB_worker_thread_args;
    delete [] adaptHistoPrev;
    delete [] adaptHistoCurr;
}


/**
    \fn toneMap
*/
bool ADMToneMapper::toneMap(ADMImage *sourceImage, ADMImage *destImage)
{
    uint32_t toneMappingMethod, gamutMethod;
    float targetLuminance;
    float saturationAdjust;
    float boostAdjust;
    bool adaptiveRGB;
    
    config->getConfig(&toneMappingMethod, &saturationAdjust, &boostAdjust, &adaptiveRGB, &gamutMethod, &targetLuminance);
    
    if (hdrTMmethod != toneMappingMethod)
    {
        hdrTMmethod = toneMappingMethod;
        hdrTMsrcLum = hdrTMtrgtLum = hdrTMsat = hdrTMboost = -1.0;	// invalidate
    }

    switch (toneMappingMethod)
    {
        case 1:	// fastYUV
                return toneMap_fastYUV(sourceImage, destImage, targetLuminance, saturationAdjust, boostAdjust);
            break;
        case 2:
        case 3:
        case 4:
                return toneMap_RGB(sourceImage, destImage, toneMappingMethod, targetLuminance, saturationAdjust, boostAdjust, adaptiveRGB, gamutMethod);
        default:
            return false;
    }
}



/**
    \fn toneMap_fastYUV_worker
*/
void * ADMToneMapper::toneMap_fastYUV_worker(void *argptr)
{
    fastYUV_worker_thread_arg * arg = (fastYUV_worker_thread_arg*)argptr;

    int ystride = ADM_IMAGE_ALIGN(arg->dstWidth);
    int uvstride = ADM_IMAGE_ALIGN(arg->dstWidth/2);
    uint8_t * ptr, * ptrNext, * ptrU, * ptrV;
    uint16_t * hptr, * hptrNext, * hptrU, * hptrV;
    uint8_t ysdr[4];
    int usdr,vsdr,urot,vrot,oormask;
    oormask = 0xFF;
    oormask = ~oormask;

    for (int y=arg->ystart; y<(arg->dstHeight/2); y+=arg->yincr)
    {
        ptr = arg->dstData[0] + y*2*ystride;
        ptrNext = ptr + ystride;
        hptr = (uint16_t *)(arg->gbrData[0]) + y*2*ystride;
        hptrNext = hptr+ystride;
        ptrU = arg->dstData[1] + y*uvstride;
        ptrV = arg->dstData[2] + y*uvstride;
        hptrU = (uint16_t *)(arg->gbrData[1]) + y*uvstride;
        hptrV = (uint16_t *)(arg->gbrData[2]) + y*uvstride;
        for (int x=0; x<(arg->dstWidth/2); x++)
        {
            ysdr[0] = arg->hdrLumaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(*hptr>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            hptr++;
            ysdr[1] = arg->hdrLumaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(*hptr>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            hptr++;
            ysdr[2] = arg->hdrLumaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(*hptrNext>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            hptrNext++;
            ysdr[3] = arg->hdrLumaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(*hptrNext>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            hptrNext++;

            int luma = 0;
            luma += ysdr[0];
            luma += ysdr[1];
            luma += ysdr[2];
            luma += ysdr[3];
            luma /= 4;
            luma &= 0xFF;
            usdr = arg->hdrChromaBLUT[luma][(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(*hptrU>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            vsdr = arg->hdrChromaRLUT[luma][(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(*hptrV>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            hptrU++; hptrV++;

            *ptr = arg->hdrLumaCrLUT[vsdr][ysdr[0]];
            ptr++;
            *ptr = arg->hdrLumaCrLUT[vsdr][ysdr[1]];
            ptr++;
            *ptrNext = arg->hdrLumaCrLUT[vsdr][ysdr[2]];
            ptrNext++;
            *ptrNext = arg->hdrLumaCrLUT[vsdr][ysdr[3]];
            ptrNext++;

            if (arg->p3_primaries)	// -8 deg hue shift
            {
                usdr -= 128;
                vsdr -= 128;
                urot = 507*usdr + 71*vsdr;
                vrot = 507*vsdr - 71*usdr;
                usdr = (urot >> 9) + 128;
                vsdr = (vrot >> 9) + 128;
                if (usdr & oormask)
                {
                    usdr = (usdr < 0) ? 0:255;
                }
                if (vsdr & oormask)
                {
                    vsdr = (vsdr < 0) ? 0:255;
                }
            }

            *ptrU = usdr;
            *ptrV = vsdr;
            ptrU++; ptrV++;
        }
    }


    pthread_exit(NULL);
    return NULL;
}

/**
    \fn toneMap_fastYUV
*/
bool ADMToneMapper::toneMap_fastYUV(ADMImage *sourceImage, ADMImage *destImage, double targetLuminance, double saturationAdjust, double boostAdjust)
{
    // Check if tone mapping is needed & can do 
    if (!((sourceImage->_colorTrc == ADM_COL_TRC_SMPTE2084) || (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_NCL) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_CL)))
        return false;
    if ((sourceImage->_colorTrc == ADM_COL_TRC_BT2020_10) || (sourceImage->_colorTrc == ADM_COL_TRC_BT2020_12))	// excluding trc, not hdr
        return false;
    if (!std::isnan(sourceImage->_hdrInfo.colorSaturationWeight))
        if (sourceImage->_hdrInfo.colorSaturationWeight > 0)
            saturationAdjust *= sourceImage->_hdrInfo.colorSaturationWeight;
    
    // Determine max luminance
    double maxLuminance = 10000.0;
    if (!std::isnan(sourceImage->_hdrInfo.maxLuminance))
        if (sourceImage->_hdrInfo.maxLuminance > 0)
            if (maxLuminance > sourceImage->_hdrInfo.maxLuminance)
                maxLuminance = sourceImage->_hdrInfo.maxLuminance;
    if (!std::isnan(sourceImage->_hdrInfo.targetMaxLuminance))
        if (sourceImage->_hdrInfo.targetMaxLuminance > 0)
            if (maxLuminance > sourceImage->_hdrInfo.targetMaxLuminance)
                maxLuminance = sourceImage->_hdrInfo.targetMaxLuminance;
    if (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67)
        if (maxLuminance == 10000.0)
            maxLuminance=1000.0;
    double boost = 1;
    if ((!std::isnan(sourceImage->_hdrInfo.maxCLL)) && (!std::isnan(sourceImage->_hdrInfo.maxFALL)))
        if ((sourceImage->_hdrInfo.maxCLL > 0) && (sourceImage->_hdrInfo.maxFALL > 0))
            boost = sourceImage->_hdrInfo.maxCLL / sourceImage->_hdrInfo.maxFALL;
    boost *= boostAdjust*boostAdjust;

    // P3 primaries
    bool p3_primaries = false;
    if ((sourceImage->_colorPrim == ADM_COL_PRI_SMPTE431) || (sourceImage->_colorPrim == ADM_COL_PRI_SMPTE432))
        p3_primaries = true;
    if ((fabs(sourceImage->_hdrInfo.primaries[0][0] - 0.680) <= 0.001) && (fabs(sourceImage->_hdrInfo.primaries[0][1] - 0.320) <= 0.001) &&
        (fabs(sourceImage->_hdrInfo.primaries[1][0] - 0.265) <= 0.001) && (fabs(sourceImage->_hdrInfo.primaries[1][1] - 0.690) <= 0.001) &&
        (fabs(sourceImage->_hdrInfo.primaries[2][0] - 0.150) <= 0.001) && (fabs(sourceImage->_hdrInfo.primaries[2][1] - 0.060) <= 0.001) )
        p3_primaries = true;

    // Allocate if not done yet
    if (hdrLumaLUT == NULL)
    {
        hdrLumaLUT = new uint8_t[ADM_COLORSPACE_HDR_LUT_SIZE];
    }
    for (int i=0; i<256; i++)
    {
        if (hdrChromaBLUT[i] == NULL)
        {
            hdrChromaBLUT[i] = new uint8_t[ADM_COLORSPACE_HDR_LUT_SIZE];
        }
        if (hdrChromaRLUT[i] == NULL)
        {
            hdrChromaRLUT[i] = new uint8_t[ADM_COLORSPACE_HDR_LUT_SIZE];
        }
        if (hdrLumaCrLUT[i] == NULL)
        {
            hdrLumaCrLUT[i] = new uint8_t[256];
        }
    }
    if (hdrYUV == NULL)
    {
        hdrYUV = new uint16_t[ADM_IMAGE_ALIGN(dstWidth)*dstHeight*2];
    }
    
    // Populate LUTs if parameters have changed
    bool extended_range = false;
    if ((maxLuminance != hdrTMsrcLum) || (targetLuminance != hdrTMtrgtLum) || (saturationAdjust != hdrTMsat) || (boost != hdrTMboost))
    {
        hdrTMsrcLum = maxLuminance;
        hdrTMtrgtLum = targetLuminance;
        hdrTMsat = saturationAdjust;
        hdrTMboost = boost;
        
        int prevYSDRint=-1;
        
        for (int l=(ADM_COLORSPACE_HDR_LUT_SIZE-1); l>=0; l-=1)
        {
            double LumHDR = maxLuminance;	// peak mastering display luminance [nit]
            double LumSDR = targetLuminance;		// peak target display luminance [nit]

            double Y = l;
            // normalize:
            Y /= ADM_COLORSPACE_HDR_LUT_SIZE;
            Y -= 16.0/256.0;	// deal with limited range
            Y *= 256.0/220.0;
            if (Y < 0)
                Y = 0.0;
            if (Y > 1)
                Y = 1.0;

            // WTF? step 0
            Y = std::pow(Y, 2.4);
            Y *= std::sqrt(boost);

            // Tone mapping step 1
            double rhoHDR = 1 + 32*std::pow(LumHDR/10000.0, 1/2.4);
            double Yp = std::log(1 + (rhoHDR-1)*Y) / std::log(rhoHDR);

            // Tone mapping step 2
            double Yc;
            if (Yp < 0)
                Yc = 0.0;
            else if ((0 <= Yp) && (Yp <= 0.7399))
                Yc = 1.0770*Yp;
            else if ((0.7399 < Yp) && (Yp <  0.9909))
                Yc = -1.1510*Yp*Yp + 2.7811*Yp - 0.6302;
            else if (( 0.9909 <= Yp) && (Yp <= 1.0))
                Yc = 0.5*Yp + 0.5;
            else // Yp > 1
                Yc = 1.0;

            // Tone mapping step 3
            double rhoSDR = 1 + 32*std::pow(LumSDR/10000.0, 1/2.4);
            double YSDR = (std::pow(rhoSDR, Yc) - 1) / (rhoSDR - 1);

            // WTF? step 4
            YSDR *= std::sqrt(2);
            if (YSDR < 0)
                YSDR = 0;
            if (YSDR > 1)
                YSDR = 1.0;

            int YSDRint;
            if (extended_range)
                YSDRint = std::round(255.0*YSDR);
            else
                YSDRint = std::round(219.0*YSDR) + 16;
            hdrLumaLUT[l] = YSDRint;
            
            if (prevYSDRint != YSDRint)
            {
                prevYSDRint = YSDRint;
                for (int l=0; l<ADM_COLORSPACE_HDR_LUT_SIZE; l+=1)
                {
                    double C = l;
                    C /= ADM_COLORSPACE_HDR_LUT_SIZE;
                    C -= 16.0/256.0;	// deal with limited range
                    C *= 256.0/224.0;
                    if (C < 0)
                        C = 0.0;
                    if (C > 1)
                        C = 1.0;
                    C -= 0.5;

                    double fYSDR = ((Y==0)||(YSDR==0)) ? 1.0 : (YSDR / (1.1*Y));
                    C *= fYSDR;
                    // WTF? step 5 prevent shadow glow
                    C *= std::pow(YSDR+0.001,1/2.4)*saturationAdjust*std::sqrt(std::sqrt(boost))*std::sqrt(2);
                    if (C < -0.5)
                        C = -0.5;
                    if (C > 0.5)
                        C = 0.5;
                    C += 0.5;
                    if (extended_range)
                    {
                        hdrChromaBLUT[YSDRint][l] = std::round(255.0*C);
                        hdrChromaRLUT[YSDRint][l] = std::round(255.0*C);
                    }
                    else
                    {
                        hdrChromaBLUT[YSDRint][l] = std::round(224.0*C) + 16;
                        hdrChromaRLUT[YSDRint][l] = std::round(224.0*C) + 16;
                    }
                }
            }
        }

        for (int cr=0; cr<256; cr++)
        {
            for (int y=0; y<256; y++)
            {
                double yn, crn;
                if (extended_range)
                {
                    crn = cr/255.0;
                    yn = y/255.0;
                }
                else
                {
                    crn = cr;
                    crn -= 16.0;
                    crn /= 224.0;
                    if (crn < 0) crn = 0;
                    if (crn > 1) crn = 1;
                    yn = y;
                    yn -= 16.0;
                    yn /= 219.0;
                    if (yn < 0) yn = 0;
                    if (yn > 1) yn = 1;
                }
                crn -= 0.5;
                if (crn > 0)
                    yn -= 0.1*crn;
                if (yn < 0) yn = 0;
                int YSDRint;
                if (extended_range)
                    YSDRint = std::round(255.0*yn);
                else
                    YSDRint = std::round(219.0*yn) + 16;
                hdrLumaCrLUT[cr][y] = YSDRint;
            }
        }
    }
    
    // Do tone mapping
    uint8_t * srcData[3];
    int srcStride[3];
    uint8_t * dstData[3];
    int dstStride[3];
    
    sourceImage->GetPitches(srcStride);
    destImage->GetPitches(dstStride);
    sourceImage->GetReadPlanes(srcData);
    destImage->GetWritePlanes(dstData);
    
    // ADM_PIXFRMT_YV12 swapped UV
    uint8_t *p=dstData[1];
    dstData[1]=dstData[2];
    dstData[2]=p;
    
    uint8_t *gbrData[3];
    int gbrStride[3];
    gbrData[0] = (uint8_t*)hdrYUV;
    gbrStride[0] = ADM_IMAGE_ALIGN(dstWidth)*2;
    gbrStride[1] = gbrStride[2]= ADM_IMAGE_ALIGN(dstWidth/2)*2;
    gbrData[1] = gbrData[0] + gbrStride[0]*(dstHeight);
    gbrData[2] = gbrData[1] + gbrStride[1]*(dstHeight/2);

    sws_scale(CONTEXTYUV,srcData,srcStride,0,srcHeight,gbrData,gbrStride);

    for (int tr=0; tr<threadCount; tr++)
    {
        fastYUV_worker_thread_args[tr].dstWidth = dstWidth;
        fastYUV_worker_thread_args[tr].dstHeight = dstHeight;
        fastYUV_worker_thread_args[tr].ystart = tr;
        fastYUV_worker_thread_args[tr].yincr = threadCount;
        for (int i=0; i<3; i++)
        {
            fastYUV_worker_thread_args[tr].gbrData[i] = gbrData[i];
            fastYUV_worker_thread_args[tr].dstData[i] = dstData[i];
        }
        fastYUV_worker_thread_args[tr].p3_primaries = p3_primaries;
        fastYUV_worker_thread_args[tr].hdrLumaLUT = hdrLumaLUT;
        for (int i=0; i<256; i++)
        {
            fastYUV_worker_thread_args[tr].hdrChromaBLUT[i] = hdrChromaBLUT[i];
            fastYUV_worker_thread_args[tr].hdrChromaRLUT[i] = hdrChromaRLUT[i];
            fastYUV_worker_thread_args[tr].hdrLumaCrLUT[i] = hdrLumaCrLUT[i];
        }
    }
    for (int tr=0; tr<threadCount; tr++)
    {
        pthread_create( &worker_threads[tr], NULL, toneMap_fastYUV_worker, (void*) &fastYUV_worker_thread_args[tr]);
    }
    // work in thread workers...
    for (int tr=0; tr<threadCount; tr++)
    {
        pthread_join( worker_threads[tr], NULL);
    }

    // Reset output color properties
    destImage->_pixfrmt=ADM_PIXFRMT_YV12;
    destImage->_range=ADM_COL_RANGE_MPEG;
    destImage->_colorPrim=ADM_COL_PRI_BT709;
    destImage->_colorTrc=ADM_COL_TRC_BT709;
    destImage->_colorSpace=ADM_COL_SPC_BT709;
    
    return true;
}

/**
    \fn toneMap_RGB_ColorMatrix
*/
void ADMToneMapper::toneMap_RGB_ColorMatrix(int32_t * matrix, ADM_colorPrimaries colorPrim, ADM_colorSpace colorSpace, double * primaries, double * whitePoint)
{
    double pri[3][2];
    double wp[2];

    // defaults to BT2020:
    pri[0][0] = 0.708; pri[0][1] = 0.292;
    pri[1][0] = 0.170; pri[1][1] = 0.797;
    pri[2][0] = 0.131; pri[2][1] = 0.046;
    wp[0]=0.3127; wp[1]=0.3290;

    if (colorPrim == ADM_COL_PRI_SMPTE431)	//DCI P3
    {
        pri[0][0] = 0.680; pri[0][1] = 0.320;
        pri[1][0] = 0.265; pri[1][1] = 0.690;
        pri[2][0] = 0.150; pri[2][1] = 0.060;
        wp[0]=0.314; wp[1]=0.351;
    }

    if (colorPrim == ADM_COL_PRI_SMPTE432)	//Display P3
    {
        pri[0][0] = 0.680; pri[0][1] = 0.320;
        pri[1][0] = 0.265; pri[1][1] = 0.690;
        pri[2][0] = 0.150; pri[2][1] = 0.060;
        wp[0]=0.3127; wp[1]=0.3290;
    }

    if (primaries)
    {
        bool all = true;
        for (int j=0; j<3; j++)
            for (int k=0;k<2; k++)
                if (std::isnan(primaries[j*2+k]) || (fabs(primaries[j*2+k])<0.001))
                {
                    all = false;
                    break;
                }
        if (all)
            for (int j=0; j<3; j++)
                for (int k=0;k<2; k++)
                    pri[j][k] = primaries[j*2+k];
    }

    if (whitePoint)
    {
        if (!(std::isnan(whitePoint[0]) || std::isnan(whitePoint[1])) && (whitePoint[0] != 0) && (whitePoint[1] != 0))
        {
            wp[0] = whitePoint[0];
            wp[1] = whitePoint[1];
        }
    }

    double bt709mx[3][3] = {
            { 3.2410, -1.5374, -0.4986},
            {-0.9692,  1.8759,  0.0416},
            { 0.0556, -0.2040,  1.0571},
        };

    if ((fabs(wp[0] - 0.3127) > 0.001) || (fabs(wp[1] - 0.3290) > 0.001))	// need white point adaptation
    {
        float bradford[3][3] = {
            { 0.8951,  0.2664, -0.1614},
            {-0.7502,  1.7135,  0.0367},
            { 0.0389, -0.0685,  1.0296},
        };
        float bradfordInv[3][3] = {
            { 0.9870, -0.1471,  0.1600},
            { 0.4323,  0.5184,  0.0493},
            {-0.0085,  0.0400,  0.9685},
        };

        float dps[3][3] = {{0}};
        for (int i=0; i<3; i++)
        {
            dps[i][i] = (bradford[i][0]*0.9505 + bradford[i][1] + bradford[i][2]*1.0891) / (bradford[i][0]*(wp[0]/wp[1]) + bradford[i][1] + bradford[i][2]*((1.0-wp[0]-wp[1])/wp[1]));
        }
        float dpsb[3][3] = {{0}};
        for (int j=0; j<3; j++)
            for (int k=0;k<3; k++)
                for (int l=0;l<3; l++)
                    dpsb[j][k] += dps[j][l] * bradford[l][k];
        float tmpbt709mx[3][3] = {{0}};
        for (int j=0; j<3; j++)
            for (int k=0;k<3; k++)
                for (int l=0;l<3; l++)
                    tmpbt709mx[j][k] += bt709mx[j][l] * bradfordInv[l][k];
        for (int j=0; j<3; j++)
            for (int k=0;k<3; k++)
                bt709mx[j][k] = 0;
        for (int j=0; j<3; j++)
            for (int k=0;k<3; k++)
                for (int l=0;l<3; l++)
                    bt709mx[j][k] += tmpbt709mx[j][l] * dpsb[l][k];
    }
    
    double X[3], Z[3], S[3];
    for (int i=0; i<3; i++)
    {
        X[i] = pri[i][0] / pri[i][1];
        Z[i] = (1.0 - pri[i][0] - pri[i][1]) / pri[i][1];
    }
    double smx[3][3];
    for (int i=0; i<3; i++)
    {
        smx[0][i] = X[i];
        smx[1][i] = 1.0;
        smx[2][i] = Z[i];
    }
    // invert smx
    double smxInv[3][3];
    smxInv[0][0] =  (smx[1][1]*smx[2][2] - smx[2][1]*smx[1][2]);
    smxInv[0][1] = -(smx[0][1]*smx[2][2] - smx[2][1]*smx[0][2]);
    smxInv[0][2] =  (smx[0][1]*smx[1][2] - smx[1][1]*smx[0][2]);
    smxInv[1][0] = -(smx[1][0]*smx[2][2] - smx[2][0]*smx[1][2]);
    smxInv[1][1] =  (smx[0][0]*smx[2][2] - smx[2][0]*smx[0][2]);
    smxInv[1][2] = -(smx[0][0]*smx[1][2] - smx[1][0]*smx[0][2]);
    smxInv[2][0] =  (smx[1][0]*smx[2][1] - smx[2][0]*smx[1][1]);
    smxInv[2][1] = -(smx[0][0]*smx[2][1] - smx[2][0]*smx[0][1]);
    smxInv[2][2] =  (smx[0][0]*smx[1][1] - smx[1][0]*smx[0][1]);
    double determinant = smx[0][0] * smxInv[0][0] + smx[1][0] * smxInv[0][1] + smx[2][0] * smxInv[0][2];
    if (determinant != 0.0)
        for (int j=0; j<3; j++)
            for (int k=0;k<3; k++)
                smxInv[j][k] /= determinant;

    for (int i=0; i<3; i++)
        S[i] = smxInv[i][0]*(wp[0]/wp[1]) + smxInv[i][1] + smxInv[i][2]*((1.0-wp[0]-wp[1])/wp[1]);
    for (int i=0; i<3; i++)
    {
        smx[0][i] = S[i]*X[i];
        smx[1][i] = S[i];
        smx[2][i] = S[i]*Z[i];
    }
    
    double fmx[3][3] = {{0}};
    for (int j=0; j<3; j++)
        for (int k=0;k<3; k++)
            for (int l=0;l<3; l++)
                fmx[j][k] += bt709mx[j][l] * smx[l][k];

    for (int j=0; j<3; j++)
        for (int k=0;k<3; k++)
            matrix[j*3+k] = std::round(fmx[j][k]*4096);
}



/**
    \fn toneMap_RGB_worker
*/
void * ADMToneMapper::toneMap_RGB_worker(void *argptr)
{
    RGB_worker_thread_arg * arg = (RGB_worker_thread_arg*)argptr;

    int stride = ADM_IMAGE_ALIGN(arg->srcWidth);
    uint8_t * sdrBGR, * sdrG, * sdrB;
    int32_t hdrR, hdrG, hdrB, hY, hU, hV;
    uint16_t * hdrY, * hdrU, * hdrV;
    int32_t linR,linG,linB,linccR,linccG,linccB;
    int32_t oormask;
    oormask = 0xFFFF;
    oormask = ~oormask;

    for (int y=arg->ystart; y<(arg->srcHeight); y+=arg->yincr)
    {
        sdrBGR = arg->sdrRGB + y*ADM_IMAGE_ALIGN(arg->srcWidth*3);
        hdrY = arg->hdrYCbCr[0] + y*stride;
        hdrU = arg->hdrYCbCr[1] + y*stride;
        hdrV = arg->hdrYCbCr[2] + y*stride;

        for (int x=0; x<(arg->srcWidth); x++)
        {
            // do YUV->RGB conversion here (multithreaded)
            hY = *hdrY;
            hU = *hdrU;
            hV = *hdrV;
            hdrY++; hdrU++; hdrV++;

            hU -= 32768;
            hV -= 32768;
            
            // YUV is limited range
            // Y: 4096 .. 60416     -> substract 4096, then multiply by 256/220
            // UV: 4096 .. 61440    -> multiply by 256/224
            hY -= 4096;
            
            // [RGB] = [BT.2020-NCL] * [Y'CbCr]
            //  1       +0                  +1.4746
            //  1       -0.16455312684366   -0.57135312684366
            //  1       +1.8814             +0
            hdrR = hY*9533            + hV*13806;
            hdrG = hY*9533 - hU*1541  - hV*5349 ;
            hdrB = hY*9533 + hU*17614           ;
            hdrR /= 8192;
            hdrG /= 8192;
            hdrB /= 8192;
            if ((hdrR&oormask) || (hdrG&oormask) || (hdrB&oormask))
            {
                hdrR = (hdrR<0) ? 0 : ((hdrR > 65535) ? 65535 : hdrR);
                hdrG = (hdrG<0) ? 0 : ((hdrG > 65535) ? 65535 : hdrG);
                hdrB = (hdrB<0) ? 0 : ((hdrB > 65535) ? 65535 : hdrB);
            }
            
            linR = arg->hdrRGBLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(hdrR>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            linG = arg->hdrRGBLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(hdrG>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            linB = arg->hdrRGBLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(hdrB>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];

            
            linccR = arg->ccmx[0]*linR + arg->ccmx[1]*linG + arg->ccmx[2]*linB;
            linccG = arg->ccmx[3]*linR + arg->ccmx[4]*linG + arg->ccmx[5]*linB;
            linccB = arg->ccmx[6]*linR + arg->ccmx[7]*linG + arg->ccmx[8]*linB;
            linccR >>= 12;
            linccG >>= 12;
            linccB >>= 12;
            if ((linccR&oormask) || (linccG&oormask) || (linccB&oormask))
            {
                if (arg->gamutMethod == 1)
                {
                    int32_t min = (linccR < linccG) ? linccR : linccG;
                    min = (min < linccB) ? min : linccB;
                    if (min < 0)
                    {
                        int32_t luma = 54*linccR+183*linccG+18*linccB;
                        luma >>= 8;
                        int32_t coeff = ((min-luma)==0) ? 256: min*256/(min-luma);
                        int32_t icoeff = 256-coeff;
                        int32_t lc = luma*coeff;
                        linccR = icoeff*linccR + lc;
                        linccR >>= 8;
                        linccG = icoeff*linccG + lc;
                        linccG >>= 8;
                        linccB = icoeff*linccB + lc;
                        linccB >>= 8;
                    }
                    int32_t max = (linccR > linccG) ? linccR : linccG;
                    max = (max > linccB) ? max : linccB;
                    if (max > 65535)
                    {
                        int32_t coeff = (4096*65536)/max;
                        linccR *= coeff;
                        linccR >>= 12;
                        linccG *= coeff;
                        linccG >>= 12;
                        linccB *= coeff;
                        linccB >>= 12;
                    }
                }
                linccR = (linccR < 0) ? 0 : ((linccR > 65535) ? 65535 : linccR);
                linccG = (linccG < 0) ? 0 : ((linccG > 65535) ? 65535 : linccG);
                linccB = (linccB < 0) ? 0 : ((linccB > 65535) ? 65535 : linccB);
            }
            *sdrBGR = arg->hdrGammaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(linccB>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            sdrBGR++;
            *sdrBGR = arg->hdrGammaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(linccG>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            sdrBGR++;
            *sdrBGR = arg->hdrGammaLUT[(ADM_COLORSPACE_HDR_LUT_SIZE-1)&(linccR>>(16-ADM_COLORSPACE_HDR_LUT_WIDTH))];
            sdrBGR++;
        }
    }

    pthread_exit(NULL);
    return NULL;
}

/**
    \fn toneMap_RGB
*/
bool ADMToneMapper::toneMap_RGB(ADMImage *sourceImage, ADMImage *destImage, unsigned int method, double targetLuminance, double saturationAdjust, double boostAdjust, bool adaptive, unsigned int gamutMethod)
{
    // Check if tone mapping is needed & can do 
    if (!((sourceImage->_colorTrc == ADM_COL_TRC_SMPTE2084) || (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_NCL) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_CL)))
        return false;
    if ((sourceImage->_colorTrc == ADM_COL_TRC_BT2020_10) || (sourceImage->_colorTrc == ADM_COL_TRC_BT2020_12))	// excluding trc, not hdr
        return false;
    if (!std::isnan(sourceImage->_hdrInfo.colorSaturationWeight))
        if (sourceImage->_hdrInfo.colorSaturationWeight > 0)
            saturationAdjust *= sourceImage->_hdrInfo.colorSaturationWeight;
    
    // Determine max luminance
    double maxLuminance = 10000.0;
    if (!std::isnan(sourceImage->_hdrInfo.maxLuminance))
        if (sourceImage->_hdrInfo.maxLuminance > 0)
            if (maxLuminance > sourceImage->_hdrInfo.maxLuminance)
                maxLuminance = sourceImage->_hdrInfo.maxLuminance;
    if (!std::isnan(sourceImage->_hdrInfo.targetMaxLuminance))
        if (sourceImage->_hdrInfo.targetMaxLuminance > 0)
            if (maxLuminance > sourceImage->_hdrInfo.targetMaxLuminance)
                maxLuminance = sourceImage->_hdrInfo.targetMaxLuminance;
    if (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67)
        if (maxLuminance == 10000.0)
            maxLuminance=1000.0;
    double peakLuminance = maxLuminance;
    if (!std::isnan(sourceImage->_hdrInfo.maxCLL))
        if (sourceImage->_hdrInfo.maxCLL > 0)
            if (peakLuminance > sourceImage->_hdrInfo.maxCLL)
                peakLuminance = sourceImage->_hdrInfo.maxCLL;
    double boost = 1;
    if ((!std::isnan(sourceImage->_hdrInfo.maxCLL)) && (!std::isnan(sourceImage->_hdrInfo.maxFALL)))
        if ((sourceImage->_hdrInfo.maxCLL > 0) && (sourceImage->_hdrInfo.maxFALL > 0))
            boost = sourceImage->_hdrInfo.maxCLL / sourceImage->_hdrInfo.maxFALL;
    boost *= boostAdjust*boostAdjust;

    // Allocate if not done yet
    if (hdrRGBLUT == NULL)
    {
        hdrRGBLUT = new uint16_t[ADM_COLORSPACE_HDR_LUT_SIZE];
    }
    if (hdrGammaLUT == NULL)
    {
        hdrGammaLUT = new uint8_t[ADM_COLORSPACE_HDR_LUT_SIZE];
    }
    for (int i=0; i<3; i++)
    {
        if (hdrYCbCr[i] == NULL)
        {
            hdrYCbCr[i] = new uint16_t[ADM_IMAGE_ALIGN(srcWidth)*srcHeight];
        }
    }
    if (sdrRGB == NULL)
    {
        sdrRGB = new uint8_t[ADM_IMAGE_ALIGN(srcWidth*3)*srcHeight];
    }

    
    // Create HDR YUV //RGB
    uint8_t * srcData[3];
    int srcStride[3];
    uint8_t * dstData[3];
    int dstStride[3];
    
    sourceImage->GetPitches(srcStride);
    destImage->GetPitches(dstStride);
    sourceImage->GetReadPlanes(srcData);
    destImage->GetWritePlanes(dstData);
    
    // ADM_PIXFRMT_YV12 swapped UV
    uint8_t *p=dstData[1];
    dstData[1]=dstData[2];
    dstData[2]=p;
    
    uint8_t *gbrData[3];
    int gbrStride[3];

    for (int p=0; p<3; p++)
    {
        gbrData[p] = (uint8_t*)hdrYCbCr[p];   // convert to YUV444
        gbrStride[p] = ADM_IMAGE_ALIGN(srcWidth)*2;
    }
    sws_scale(CONTEXTRGB1,srcData,srcStride,0,srcHeight,gbrData,gbrStride);
    
    
    
    if (adaptive)
    {
        // Adaptive RGB tonemapper constant. Defined here -> scope of the defines restricted to this file.
        #define HDR_AVG_LOWER_LIMIT     (0.0004)        // lower value == brighter dark scenes
        #define SDR_AVG                 (0.25)          // average is 50% (like a ramp from black to white) == 0.5 -> in linear intensity ~= 0.25
        #define RC_FILTER_CONST         (0.1)           // filtering constant
        
        boost = boostAdjust*boostAdjust;    // don't use _hdrInfo, only user enforced parameter
        
        if (linearizeLUT == NULL)
        {
            linearizeLUT = new uint16_t[ADM_ADAPTIVE_HDR_LIN_LUT_SIZE];
            for (int l=0; l<ADM_ADAPTIVE_HDR_LIN_LUT_SIZE; l+=1)
            {
                double Y = l;
                // normalize:
                Y /= ADM_ADAPTIVE_HDR_LIN_LUT_SIZE;
                Y -= 16.0/256.0;	// deal with limited range
                Y *= 256.0/220.0;
                if (Y < 0)
                    Y = 0.0;
                if (Y > 1)
                    Y = 1.0;

                double Ylin;
                // linearize
                if (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67)	//HLG
                {
                    if (Y <= 0.5)
                        Ylin = Y*Y/3.0;
                    else
                        Ylin = (std::exp((Y-0.55991073)/0.17883277) + 0.28466892)/12;
                } else
                if ((sourceImage->_colorTrc == ADM_COL_TRC_SMPTE2084) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_NCL) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_CL))	//PQ
                {
                    Ylin = 0;
                    if ((std::pow(Y, 1/78.84375) - 0.8359375) > 0)
                        Ylin = std::pow((std::pow(Y, 1/78.84375) - 0.8359375) / (18.8515625 - 18.6875*std::pow(Y, 1/78.84375)) , 1/0.1593017578125);
                } else {
                    Ylin = std::pow(Y, 2.6);
                }
                
                linearizeLUT[l] = std::round(65535.0*Ylin);
            }
        }
        
        if (adaptHistoPrev == NULL)
        {
            adaptHistoPrev = new int32_t[64];
        }
        if (adaptHistoCurr == NULL)
        {
            adaptHistoCurr = new int32_t[64];
        }
        
        // measure peak and avg luminance of the current frame
        uint64_t linlumaMax = 0;
        uint64_t linLumaAvg = 0;
        uint16_t linLum;
        memset(adaptHistoCurr,0,64*sizeof(int32_t));
        for (int y=0; y<srcHeight; y++)
        {
            uint16_t * hdrY = hdrYCbCr[0] + y*ADM_IMAGE_ALIGN(srcWidth);

            for (int x=0; x<srcWidth; x++)
            {
                linLum = linearizeLUT[(ADM_ADAPTIVE_HDR_LIN_LUT_SIZE-1)&(hdrY[x]>>(16-ADM_ADAPTIVE_HDR_LIN_LUT_WIDTH))];
                linLumaAvg += linLum;
                if (linlumaMax < linLum)
                    linlumaMax = linLum;
            }
        }
        double lumaMax,lumaAvg;
        lumaAvg = linLumaAvg;
        lumaAvg /= (srcWidth*srcHeight);
        lumaAvg /= 65535.0;
        lumaMax = linlumaMax;
        lumaMax /= 65535.0;
        lumaAvg = (1.0-HDR_AVG_LOWER_LIMIT)*lumaAvg + HDR_AVG_LOWER_LIMIT;    // soft limit (differentiable knee)

        // undersample chroma as it would be 4:2:0. its fine for scene detection histogram
        for (int y=0; y<srcHeight; y+=2)
        {
            uint16_t * hdrU = hdrYCbCr[1] + y*ADM_IMAGE_ALIGN(srcWidth);
            uint16_t * hdrV = hdrYCbCr[2] + y*ADM_IMAGE_ALIGN(srcWidth);

            for (int x=0; x<srcWidth; x+=2)
            {
                adaptHistoCurr[(hdrU[x]>>11)&0x1F +  0] += 1;
                adaptHistoCurr[(hdrV[x]>>11)&0x1F + 32] += 1;
            }
        }
        double scd = 0;
        for (int b=0; b<64; b++)
        {
            scd += abs(adaptHistoPrev[b]-adaptHistoCurr[b]);
        }
        scd /= (srcWidth*srcHeight)/4;
        memcpy(adaptHistoPrev,adaptHistoCurr,64*sizeof(int32_t));
        //printf("scd = %.06f\n",scd);

        // low-pass filtering the measured avg and peak, to prevent flickering
        double filterConst = RC_FILTER_CONST;
        if (scd > filterConst)  // if scene changed, adapt quickly
            filterConst = scd;
        if (filterConst > 1.0)  // valid range: 0.0 .. 1.0
            filterConst = 1.0;
        if (adaptLLAvg < 0)     // if this is the first frame after the tonemapper kicks in, use the first measurement as is
            adaptLLAvg = lumaAvg;
        else                    // else do filtering
            adaptLLAvg = (1.0-filterConst)*adaptLLAvg + filterConst*lumaAvg;
        if (adaptLLMax < 0)
            adaptLLMax = lumaMax;
        else
            adaptLLMax = (1.0-filterConst)*adaptLLMax + filterConst*lumaMax;
        
                    
        if (adaptLLAvg < HDR_AVG_LOWER_LIMIT)
            adaptLLAvg = HDR_AVG_LOWER_LIMIT;
        
        peakLuminance = maxLuminance*adaptLLMax*(SDR_AVG/adaptLLAvg);
        if (peakLuminance < targetLuminance)
            peakLuminance = targetLuminance;

        for (int l=0; l<ADM_COLORSPACE_HDR_LUT_SIZE; l+=1)
        {
            double Y = l;
            // normalize:
            Y /= ADM_COLORSPACE_HDR_LUT_SIZE;
            
            double Ylin;
            // linearize
            if (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67)	//HLG
            {
                if (Y <= 0.5)
                    Ylin = Y*Y/3.0;
                else
                    Ylin = (std::exp((Y-0.55991073)/0.17883277) + 0.28466892)/12;
            } else
            if ((sourceImage->_colorTrc == ADM_COL_TRC_SMPTE2084) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_NCL) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_CL))	//PQ
            {
                Ylin = 0;
                if ((std::pow(Y, 1/78.84375) - 0.8359375) > 0)
                    Ylin = std::pow((std::pow(Y, 1/78.84375) - 0.8359375) / (18.8515625 - 18.6875*std::pow(Y, 1/78.84375)) , 1/0.1593017578125);
            } else {
                Ylin = std::pow(Y, 2.6);
            }
            
            Ylin *= SDR_AVG/adaptLLAvg;
            double npl = peakLuminance/targetLuminance;
            
            double Ytm = Ylin;
            // tonemap
            switch (method)
            {
                default:
                case 2:	// clip
                        Ytm *= std::sqrt(boost);
                        if (Ytm > 1.0)
                            Ytm = 1.0;
                    break;
                case 3:	// reinhard
                        Ytm *= std::sqrt(boost)*1.4;    // multiplier const: try to match perceived brightness to clipping & hable
                        Ytm = Ytm/(1.0+Ytm);
                        Ytm *= (npl+1)/npl;
                    break;
                case 4:	// hable
                        Ytm *= boost*4.5;    // multiplier const: try to match perceived brightness to clipping & reinhard
                        Ytm = (Ytm * (Ytm * 0.15 + 0.50 * 0.10) + 0.20 * 0.02) / (Ytm * (Ytm * 0.15 + 0.50) + 0.20 * 0.30) - 0.02 / 0.30;
                        Ytm /= (npl * (npl * 0.15 + 0.50 * 0.10) + 0.20 * 0.02) / (npl * (npl * 0.15 + 0.50) + 0.20 * 0.30) - 0.02 / 0.30;
                    break;
            }
            
            if (Ytm < 0)
                Ytm = 0;
            if (Ytm > 1)
                Ytm = 1;
            hdrRGBLUT[l] = std::round(65535.0*Ytm);
            
            // gamma
            double Ygamma;
            Ygamma = ((Y > 0.0031308) ? (( 1.055 * std::pow(Y, (1.0 / 2.4)) ) - 0.055) : (Y * 12.92));
            hdrGammaLUT[l] = std::round(255.0*Ygamma);
        }
        
        for (int l=0; l<256; l+=1)
        {
            double C = l;
            C -= 128;
            C *= saturationAdjust;
            C += 128;
            if (C < 0)
                C = 0;
            if (C > 255)
                C = 255;
            sdrRGBSat[l] = std::round(C);
        }

    }
    else
    if ((maxLuminance != hdrTMsrcLum) || (targetLuminance != hdrTMtrgtLum) || (saturationAdjust != hdrTMsat) || (boost != hdrTMboost))    // Populate LUTs if parameters have changed
    {
        hdrTMsrcLum = maxLuminance;
        hdrTMtrgtLum = targetLuminance;
        hdrTMsat = saturationAdjust;
        hdrTMboost = boost;
        
        for (int l=0; l<ADM_COLORSPACE_HDR_LUT_SIZE; l+=1)
        {
            double LumHDR = maxLuminance;	// peak mastering display luminance [nit]
            double LumSDR = targetLuminance;		// peak target display luminance [nit]

            double Y = l;
            // normalize:
            Y /= ADM_COLORSPACE_HDR_LUT_SIZE;
            
            double Ylin;
            // linearize
            if (sourceImage->_colorTrc == ADM_COL_TRC_ARIB_STD_B67)	//HLG
            {
                if (Y <= 0.5)
                    Ylin = Y*Y/3.0;
                else
                    Ylin = (std::exp((Y-0.55991073)/0.17883277) + 0.28466892)/12;
            } else
            if ((sourceImage->_colorTrc == ADM_COL_TRC_SMPTE2084) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_NCL) || (sourceImage->_colorSpace == ADM_COL_SPC_BT2020_CL))	//PQ
            {
                Ylin = 0;
                if ((std::pow(Y, 1/78.84375) - 0.8359375) > 0)
                    Ylin = std::pow((std::pow(Y, 1/78.84375) - 0.8359375) / (18.8515625 - 18.6875*std::pow(Y, 1/78.84375)) , 1/0.1593017578125);
            } else {
                Ylin = std::pow(Y, 2.6);
            }
            
            Ylin *= LumHDR/LumSDR;
            double npl = peakLuminance/LumSDR;
            
            double Ytm = Ylin;
            // tonemap
            switch (method)
            {
                default:
                case 2:	// clip
                        Ytm *= std::sqrt(boost);
                        if (Ytm > 1.0)
                            Ytm = 1.0;
                    break;
                case 3:	// reinhard
                        Ytm *= std::sqrt(boost);
                        Ytm = Ytm/(1.0+Ytm);
                        Ytm *= (npl+1)/npl;
                    break;
                case 4:	// hable
                        Ytm *= boost;
                        Ytm = (Ytm * (Ytm * 0.15 + 0.50 * 0.10) + 0.20 * 0.02) / (Ytm * (Ytm * 0.15 + 0.50) + 0.20 * 0.30) - 0.02 / 0.30;
                        Ytm /= (npl * (npl * 0.15 + 0.50 * 0.10) + 0.20 * 0.02) / (npl * (npl * 0.15 + 0.50) + 0.20 * 0.30) - 0.02 / 0.30;
                    break;
            }
            
            if (Ytm < 0)
                Ytm = 0;
            if (Ytm > 1)
                Ytm = 1;
            hdrRGBLUT[l] = std::round(65535.0*Ytm);
            
            // gamma
            double Ygamma;
            Ygamma = ((Y > 0.0031308) ? (( 1.055 * std::pow(Y, (1.0 / 2.4)) ) - 0.055) : (Y * 12.92));
            hdrGammaLUT[l] = std::round(255.0*Ygamma);
        }
        
        for (int l=0; l<256; l+=1)
        {
            double C = l;
            C -= 128;
            C *= saturationAdjust;
            C += 128;
            if (C < 0)
                C = 0;
            if (C > 255)
                C = 255;
            sdrRGBSat[l] = std::round(C);
        }
    }

    // Do tone mapping

    int32_t ccmx[9];
    toneMap_RGB_ColorMatrix(ccmx, sourceImage->_colorPrim, sourceImage->_colorSpace, &(sourceImage->_hdrInfo.primaries[0][0]), sourceImage->_hdrInfo.whitePoint);

    for (int tr=0; tr<threadCount; tr++)
    {
        RGB_worker_thread_args[tr].srcWidth = srcWidth;
        RGB_worker_thread_args[tr].srcHeight = srcHeight;
        RGB_worker_thread_args[tr].ystart = tr;
        RGB_worker_thread_args[tr].yincr = threadCount;
        for (int i=0; i<3; i++)
        {
            RGB_worker_thread_args[tr].hdrYCbCr[i] = hdrYCbCr[i];
        }
        RGB_worker_thread_args[tr].sdrRGB = sdrRGB;
        RGB_worker_thread_args[tr].hdrRGBLUT = hdrRGBLUT;
        RGB_worker_thread_args[tr].ccmx = ccmx;
        RGB_worker_thread_args[tr].hdrGammaLUT = hdrGammaLUT;
        RGB_worker_thread_args[tr].gamutMethod = gamutMethod;
    }
    for (int tr=0; tr<threadCount; tr++)
    {
        pthread_create( &worker_threads[tr], NULL, toneMap_RGB_worker, (void*) &RGB_worker_thread_args[tr]);
    }
    // work in thread workers...
    for (int tr=0; tr<threadCount; tr++)
    {
        pthread_join( worker_threads[tr], NULL);
    }

    gbrData[0] = sdrRGB;
    gbrStride[0] = ADM_IMAGE_ALIGN(srcWidth*3);
    gbrData[1] = NULL;
    gbrStride[1] = 0;
    gbrData[2] = NULL;
    gbrStride[2] = 0;
    sws_scale(CONTEXTRGB2,gbrData,gbrStride,0,srcHeight,dstData,dstStride);

    // dst is YUV420
    for (int p=1; p<3; p++)
    {
        uint8_t * ptr = dstData[p];
        for (int q=0; q<(dstStride[p] * dstHeight/2); q++)
        {
            *ptr = sdrRGBSat[*ptr];
            ptr++;
        }
    }

    // Reset output color properties
    destImage->_pixfrmt=ADM_PIXFRMT_YV12;
    destImage->_range=ADM_COL_RANGE_MPEG;
    destImage->_colorPrim=ADM_COL_PRI_BT709;
    destImage->_colorTrc=ADM_COL_TRC_BT709;
    destImage->_colorSpace=ADM_COL_SPC_BT709;
    
    return true;

}
//EOF
