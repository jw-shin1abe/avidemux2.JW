/**
    \brief vaapi filters
    \author mean (C) 2016
 *  Use vaapi transform API (i.e. resize/colorspace)
 * (c) mean 2016
 * 
*/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ADM_default.h"
#include "ADM_coreLibVA.h"
#include "ADM_coreVideoFilter.h"
#include "ADM_videoFilterCache.h"
#include "DIA_factory.h"
#include "vaapiFilter.h"
#include "vaapiFilter_desc.cpp"




#include "ADM_coreLibVA.h"
//
#define ADM_INVALID_FRAME_NUM 0x80000000
#define ADM_NB_SURFACES 3

/**
    \class vaapiVideoFilter
*/
class vaapiVideoFilter : public  ADM_coreVideoFilterCached
{
protected:
                    ADMColorScalerSimple *scaler;
                    ADM_vaSurface        *sourceSurface,*destSurface;
                    VAConfigID           configID;
                    VAContextID          contextID;
                    bool                 passThrough;
                    bool                 setupVaapi(void);
                    bool                 cleanupVaapi(void);
                    vaapiFilter          configuration;                    
                    bool                 updateInfo(bool status);

public:
        
                             vaapiVideoFilter(ADM_coreVideoFilter *previous,CONFcouple *conf);
                             ~vaapiVideoFilter();

        virtual const char   *getConfiguration(void);                 /// Return  current configuration as a human readable string
        virtual bool         getNextFrame(uint32_t *fn,ADMImage *image);           /// Return the next image
        virtual bool         getCoupledConf(CONFcouple **couples) ;   /// Return the current filter configuration
		virtual void setCoupledConf(CONFcouple *couples);
        virtual bool         configure(void) ;                        /// Start graphical user interface
};

// Add the hook to make it valid plugin
DECLARE_VIDEO_FILTER(   vaapiVideoFilter,   // Class
                        1,0,0,              // Version
                        ADM_UI_GTK+ADM_UI_QT4+ADM_FEATURE_VDPAU,     // We need a display for VDPAU; so no cli...
                        VF_TRANSFORM,            // Category
                        "vaapiResize",            // internal name (must be uniq!)
                        QT_TRANSLATE_NOOP("vaapiResize","vaapi: Resize"),            // Display name
                        QT_TRANSLATE_NOOP("vaapiResize","vaapi: Resize image using vaapi.") // Description
)
       
/**
    \fn setupVaapi
*/
bool vaapiVideoFilter::setupVaapi(void)
{
    FilterInfo *prevInfo=previousFilter->getInfo();
    FilterInfo *currentInfo=getInfo();

    scaler=NULL;
    info.width=configuration.targetWidth;
    info.height=configuration.targetHeight;
    
    if(configuration.targetWidth==prevInfo->width && configuration.targetHeight==prevInfo->height)
    {
        ADM_info("Passthrough\n");
        return false;
    }
    
    configID=admLibVA::createFilterConfig();
    if(configID==VA_INVALID)
    {
        ADM_warning("Cannot create context\n");
        return false;
    }
    // context
    //contextID=admLibVA::createFilterContext(configID,info.width, info.height,VA_PROGRESSIVE,)
    
    // Allocate source and target surface
    sourceSurface=ADM_vaSurface::allocateWithSurface(prevInfo->width,prevInfo->height);
    destSurface=ADM_vaSurface::allocateWithSurface(currentInfo->width,currentInfo->height);
    if(!sourceSurface || !destSurface )
    {
        ADM_warning("Cannot allocate surface\n");
        cleanupVaapi();
        return false;
    }

#if 0
    scaler=new ADMColorScalerSimple( info.width,info.height, ADM_COLOR_RGB32A,ADM_COLOR_YV12);
    ADM_info("VDPAU setup ok\n");
#endif
    return true;    
}
/**
    \fn cleanupVaapi
*/
bool vaapiVideoFilter::cleanupVaapi(void)
{
    if(sourceSurface)
    {
        delete sourceSurface;
        sourceSurface=NULL;
    }
    if(destSurface)
    {
        delete destSurface;
        destSurface=NULL;
    }
    if(configID!=VA_INVALID)
    {
        admLibVA::destroyFilterConfig(configID);
        configID=VA_INVALID;
    }
    if(contextID!=VA_INVALID)
    {
        admLibVA::destroyFilterContext(contextID);
        contextID=VA_INVALID;
    }
    
    return true;
}
/**
 * \fn updateInfo
 * @param status
 * @return 
 */
bool vaapiVideoFilter::updateInfo(bool status)
{
    passThrough=!status;
    if(passThrough)
    {
        FilterInfo *prevInfo=previousFilter->getInfo();
        info.width=prevInfo->width;
        info.height=prevInfo->height;
    }else
    {
        info.width=configuration.targetWidth;
        info.height=configuration.targetHeight;
    }
    return true;
}
/**
    \fn constructor
*/
vaapiVideoFilter::vaapiVideoFilter(ADM_coreVideoFilter *in, CONFcouple *setup)
        : ADM_coreVideoFilterCached(5,in,setup)
{
    configID=VA_INVALID;
    contextID=VA_INVALID;
    sourceSurface=NULL;
    destSurface=NULL;
    if(!setup || !ADM_paramLoad(setup,vaapiFilter_param,&configuration))
    {
        // Default value
        configuration.targetWidth=info.width;
        configuration.targetHeight=info.height;
    }

    myName="vaapi";
    bool status=setupVaapi();
    updateInfo(status);
    
}
/**
    \fn destructor
*/
vaapiVideoFilter::~vaapiVideoFilter()
{
        cleanupVaapi();
}
/**
    \fn updateInfo
*/
bool vaapiVideoFilter::configure( void) 
{
     diaElemUInteger  tWidth(&(configuration.targetWidth),QT_TRANSLATE_NOOP("vaapiResize","Width :"),16,2048);
     diaElemUInteger  tHeight(&(configuration.targetHeight),QT_TRANSLATE_NOOP("vaapiResize","Height :"),16,2048);
     
     diaElem *elems[]={&tWidth,&tHeight};
     
     if(diaFactoryRun(QT_TRANSLATE_NOOP("vaapiResize","vaapi"),sizeof(elems)/sizeof(diaElem *),elems))
     {
                ADM_info("New dimension : %d x %d\n",info.width,info.height);
                cleanupVaapi();
                bool status=setupVaapi();
                updateInfo(status);
                return true;
     }
     return false;
}
/**
    \fn getCoupledConf
    \brief Return our current configuration as couple name=value
*/
bool         vaapiVideoFilter::getCoupledConf(CONFcouple **couples)
{
    return ADM_paramSave(couples, vaapiFilter_param,&configuration);
}

void vaapiVideoFilter::setCoupledConf(CONFcouple *couples)
{
    ADM_paramLoad(couples, vaapiFilter_param, &configuration);
}

/**
    \fn getConfiguration
    \brief Return current setting as a string
*/
const char *vaapiVideoFilter::getConfiguration(void)
{

    static char conf[80];    
    char conf2[80];
    conf2[0]=0;
    sprintf(conf,"vaapi:");
    if(1) //configuration.resizeToggle)
    {
        sprintf(conf2,"%d x %d -> %d x %d",
                        previousFilter->getInfo()->width, 
                        previousFilter->getInfo()->height,
                        configuration.targetWidth,configuration.targetHeight);
        strcat(conf,conf2);
    }
    conf[79]=0;
    return conf;
}
/**
    \fn getConfiguration
    \brief Return current setting as a string
*/
bool vaapiVideoFilter::getNextFrame(uint32_t *fn,ADMImage *image)
{
     bool r=false;
     if(passThrough) 
     {
         ADM_info("VaapiFilter : passthrough\n");
         return previousFilter->getNextFrame(fn,image);
     }
    // regular image, in fact we get the next image here    
    ADMImage *next= vidCache->getImageAs(ADM_HW_LIBVA,nextFrame);
    ADM_vaSurface *source=NULL;
    if(!next)
    {
        ADM_warning("vaapiResize : cannot get image\n");
        return false;
    }
    image->Pts=next->Pts;
    if(next->refType==ADM_HW_LIBVA)
    {
          source=(ADM_vaSurface *)next->refDescriptor.refHwImage;
          printf("image is already vaapi %d\n",(int)source->surface);
    }else
    {

        printf("Uploading image to vaapi\n");
        if(!admLibVA::admImageToSurface(next,sourceSurface))        
        {
            ADM_warning("Cannot upload to sourceSurface");
            vidCache->unlockAll();
            return false;
        }
        source=sourceSurface;
    }    
    
    //-- Perform --
    VAProcPipelineParameterBuffer params;
    VABufferID paramId;
    VAStatus status;

    memset(&params,0,sizeof(params));
    
    params.surface=source->surface;
    params.surface_region=NULL;
    params.surface_color_standard=VAProcColorStandardBT601 ; // FIXME

    params.output_region      =NULL;
    params.output_background_color=0xff000000;
    params.output_color_standard=VAProcColorStandardBT601; // FIXME
            
    params.pipeline_flags=0;
    params.filter_flags=VA_FILTER_SCALING_HQ;
    // Go
#define CHECK(x) {status=x;if(status!=VA_STATUS_SUCCESS)    {ADM_warning( #x "Failed with error %d/%s\n",status,vaErrorStr(status));goto failed;}}
#define CHECK2(x) {status=x;if(status!=VA_STATUS_SUCCESS)    {ADM_warning( #x "Failed with error %d/%s\n",status,vaErrorStr(status));goto failed2;}}
    CHECK(vaBeginPicture(admLibVA::getDisplay(),  configID, destSurface->surface));
    CHECK(vaCreateBuffer(admLibVA::getDisplay(),  configID,VAProcPipelineParameterBufferType,sizeof(params),1,&params,&paramId));
    CHECK2(vaRenderPicture(admLibVA::getDisplay(),  configID,&paramId, 1));
    CHECK2(vaEndPicture(admLibVA::getDisplay(),  configID));

    vaDestroyBuffer(admLibVA::getDisplay(), paramId);
 
    // Download result to regular ADMImage
    r=destSurface->toAdmImage(image);
    printf("Result is %d\n",r);
failed2:    
    vaDestroyBuffer(admLibVA::getDisplay(), paramId);
failed:    
    nextFrame++;
    vidCache->unlockAll();
    
    return r;
    
}
//****************
// EOF
