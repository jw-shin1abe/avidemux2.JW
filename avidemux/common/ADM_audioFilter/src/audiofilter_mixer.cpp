/***************************************************************************
       \file audiofilter_mixer.cpp
       \brief Change channels configuration (down/up channels)

    (C) Mihail Zenkov <kreator@tut.by> & mean
 ***************************************************************************/
 
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ADM_default.h"

#include "ADM_audioFilter.h"
#include "audiofilter_mixer.h"
#include "audiofilter_dolby.h"
#include <math.h>

AUDMAudioFilterMixer::AUDMAudioFilterMixer(AUDMAudioFilter *instream,CHANNEL_CONF out):AUDMAudioFilter (instream)
{
    _output=out;
    _previous->rewind();     // rewind
    ADM_assert(_output<CHANNEL_LAST);
    memset(inputChannelMapping,0,sizeof(CHANNEL_TYPE)*MAX_CHANNELS);
    memset(outputChannelMapping,0,sizeof(CHANNEL_TYPE)*MAX_CHANNELS);
    CHANNEL_TYPE *map=_previous->getChannelMapping();
    bypass=false;
    input_channels = _wavHeader.channels;
    if(map && input_channels)
        memcpy(inputChannelMapping,map,sizeof(CHANNEL_TYPE)*_wavHeader.channels);
    else
        bypass=true;
    double d;               // Update duration
    d=_wavHeader.byterate;
    d/=_wavHeader.channels;

	switch (_output) {
		case CHANNEL_MONO:
			_wavHeader.channels = 1;
			outputChannelMapping[0] = ADM_CH_MONO;
		break;
		case CHANNEL_STEREO:
		case CHANNEL_STEREO_HEADPHONES:
                case CHANNEL_SURROUND_HEADPHONES:
			_wavHeader.channels = 2;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
		break;
		case CHANNEL_2F_1R:
			_wavHeader.channels = 3;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
			outputChannelMapping[2] = ADM_CH_REAR_CENTER;
		break;
		case CHANNEL_3F:
			_wavHeader.channels = 3;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
			outputChannelMapping[2] = ADM_CH_FRONT_CENTER;
		break;
		case CHANNEL_3F_1R:
			_wavHeader.channels = 4;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
			outputChannelMapping[2] = ADM_CH_REAR_CENTER;
			outputChannelMapping[3] = ADM_CH_FRONT_CENTER;
		break;
		case CHANNEL_2F_2R:
			_wavHeader.channels = 4;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
			outputChannelMapping[2] = ADM_CH_REAR_LEFT;
			outputChannelMapping[3] = ADM_CH_REAR_RIGHT;
		break;
		case CHANNEL_3F_2R:
			_wavHeader.channels = 5;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
			outputChannelMapping[2] = ADM_CH_REAR_LEFT;
			outputChannelMapping[3] = ADM_CH_REAR_RIGHT;
			outputChannelMapping[4] = ADM_CH_FRONT_CENTER;
		break;
		case CHANNEL_3F_2R_LFE:
			_wavHeader.channels = 6;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
			outputChannelMapping[2] = ADM_CH_REAR_LEFT;
			outputChannelMapping[3] = ADM_CH_REAR_RIGHT;
			outputChannelMapping[4] = ADM_CH_FRONT_CENTER;
			outputChannelMapping[5] = ADM_CH_LFE;
		break;
		case CHANNEL_DOLBY_PROLOGIC:
		case CHANNEL_DOLBY_PROLOGIC2:
			_wavHeader.channels = 2;
			outputChannelMapping[0] = ADM_CH_FRONT_LEFT;
			outputChannelMapping[1] = ADM_CH_FRONT_RIGHT;
//			DolbyInit();
		break;
        default:
                break;
	}

    d*=_wavHeader.channels;
    _wavHeader.byterate = (uint32_t)ceil(d);

    for (int k=0; k<8; k++)
    {
        sHphDelay[k] = new float [1024];
        for (int i=0; i<1024; i++)
        {
            sHphDelay[k][i] = 0.0;
        }
        sHphPos[k] = 0;
    }
//    printf("[mixer]Input channels : %u : %u \n",_previous->getInfo()->channels,input_channels);
//    printf("[mixer]Out   channels : %u : %u \n",_wavHeader.channels,ADM_CH_annel_mixer[_output]);

};
/**
 * 	\fn getChannelMapping
 *  \brief That filter changes the channel mapping, output its own
 */
CHANNEL_TYPE    *AUDMAudioFilterMixer::getChannelMapping(void ) 
{
		return this->outputChannelMapping;
}
AUDMAudioFilterMixer::~AUDMAudioFilterMixer()
{
    for (int k=0; k<8; k++)
    {
        delete [] sHphDelay[k];
    }    
};

static int MCOPY(float *in,float *out,uint32_t nbSample,uint32_t chan)
{
    memcpy(out,in,nbSample*chan*sizeof(float));
    return nbSample*chan;
    
}

static int MNto1(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
float sum;
int den=(chan+1)&0xfe;
    for(int i=0;i<nbSample;i++)
    {
        sum=0;
        for(int j=0;j<chan;j++)
          sum+=in[j];
        out[0]=sum/(float)den;
        out++;
        in+=chan;
    }
    return nbSample;
    
}

static int MStereo(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 2);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
				case ADM_CH_REAR_CENTER:
				case ADM_CH_LFE:
					out[0]  += *in * 0.707;
					out[1]  += *in * 0.707;
				break;
				case ADM_CH_FRONT_LEFT:
				case ADM_CH_REAR_LEFT:
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
				case ADM_CH_REAR_RIGHT:
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 2;
	}

	return nbSample*2;
}

static int MStereoHeadphones(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 2);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
				case ADM_CH_REAR_CENTER:
				case ADM_CH_LFE:
					out[0]  += *in * 0.707;
					out[1]  += *in * 0.707;
				break;
				case ADM_CH_FRONT_LEFT:
				case ADM_CH_REAR_LEFT:
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in;
					out[1]  += *in*0.1;
				break;
				case ADM_CH_FRONT_RIGHT:
				case ADM_CH_REAR_RIGHT:
				case ADM_CH_SIDE_RIGHT:
					out[0]  += *in*0.1;
					out[1]  += *in;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 2;
	}

	return nbSample*2;
}


static int M2F1R(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 3);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
					out[0]  += *in * 0.707;
					out[1]  += *in * 0.707;
				break;
				case ADM_CH_FRONT_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
					out[1]  += *in;
				break;
				case ADM_CH_REAR_LEFT:
				case ADM_CH_REAR_RIGHT:
				case ADM_CH_REAR_CENTER:
					out[2]  += *in;
				break;
				case ADM_CH_LFE:
					out[0]  += *in * 0.595;
					out[1]  += *in * 0.595;
					out[2]  += *in * 0.595;
				break;
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 3;
	}

	return nbSample * 3;
}

static int M3F(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 3);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
				case ADM_CH_REAR_CENTER:
					out[2]  += *in;
				break;
				case ADM_CH_FRONT_LEFT:
				case ADM_CH_REAR_LEFT:
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
				case ADM_CH_REAR_RIGHT:
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in;
				break;
				case ADM_CH_LFE:
					out[0]  += *in * 0.595;
					out[1]  += *in * 0.595;
					out[2]  += *in * 0.595;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 3;
	}

	return nbSample * 3;
}

static int M3F1R(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 4);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
					out[3]  += *in;
				break;
				case ADM_CH_REAR_CENTER:
				case ADM_CH_REAR_LEFT:
				case ADM_CH_REAR_RIGHT:
					out[2]  += *in;
				break;
				case ADM_CH_FRONT_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
					out[1]  += *in;
				break;
				case ADM_CH_LFE:
					out[0]  += *in * 0.5;
					out[1]  += *in * 0.5;
					out[2]  += *in * 0.5;
					out[3]  += *in * 0.5;
				break;
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 4;
	}

	return nbSample * 4;
}

static int M2F2R(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 4);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
					out[0]  += *in * 0.707;
					out[1]  += *in * 0.707;
				break;
				case ADM_CH_FRONT_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
					out[1]  += *in;
				break;
				case ADM_CH_REAR_LEFT:
					out[2]  += *in;
				break;
				case ADM_CH_REAR_RIGHT:
					out[3]  += *in;
				break;
				case ADM_CH_REAR_CENTER:
					out[2]  += *in * 0.707;
					out[3]  += *in * 0.707;
				break;
				case ADM_CH_LFE:
					out[0]  += *in * 0.5;
					out[1]  += *in * 0.5;
					out[2]  += *in * 0.5;
					out[3]  += *in * 0.5;
				break;
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in * 0.707;
					out[3]  += *in * 0.707;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 4;
	}

	return nbSample * 4;
}

static int M3F2R(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 5);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
					out[4]  += *in;
				break;
				case ADM_CH_FRONT_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
					out[1]  += *in;
				break;
				case ADM_CH_REAR_LEFT:
					out[2]  += *in;
				break;
				case ADM_CH_REAR_RIGHT:
					out[3]  += *in;
				break;
				case ADM_CH_REAR_CENTER:
					out[2]  += *in * 0.707;
					out[3]  += *in * 0.707;
				break;
				case ADM_CH_LFE:
					out[0]  += *in * 0.459;
					out[1]  += *in * 0.459;
					out[2]  += *in * 0.459;
					out[3]  += *in * 0.459;
					out[4]  += *in * 0.459;
				break;
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in * 0.707;
					out[3]  += *in * 0.707;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 5;
	}

	return nbSample * 5;
}

static int M3F2RLFE(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
	memset(out, 0, sizeof(float) * nbSample * 6);

	for (int i = 0; i < nbSample; i++) {
		for (int c = 0; c < chan; c++) {
			switch (chanMap[c]) {
				case ADM_CH_MONO:
				case ADM_CH_FRONT_CENTER:
					out[4]  += *in;
				break;
				case ADM_CH_FRONT_LEFT:
					out[0]  += *in;
				break;
				case ADM_CH_FRONT_RIGHT:
					out[1]  += *in;
				break;
				case ADM_CH_REAR_LEFT:
					out[2]  += *in;
				break;
				case ADM_CH_REAR_RIGHT:
					out[3]  += *in;
				break;
				case ADM_CH_REAR_CENTER:
					out[2]  += *in * 0.707;
					out[3]  += *in * 0.707;
				break;
				case ADM_CH_LFE:
					out[5]  += *in;
				break;
				case ADM_CH_SIDE_LEFT:
					out[0]  += *in * 0.707;
					out[2]  += *in * 0.707;
				break;
				case ADM_CH_SIDE_RIGHT:
					out[1]  += *in * 0.707;
					out[3]  += *in * 0.707;
				break;
                default:
                    break;
			}
			in++;
		}
		out += 6;
	}

	return nbSample * 6;
}

static int MDolbyProLogic(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
    memset(out, 0, sizeof(float) * nbSample * 2);
    float ds;

    for (int i = 0; i < nbSample; i++) {
        for (int c = 0; c < chan; c++) {
            switch (chanMap[c]) {
                case ADM_CH_MONO:
                case ADM_CH_FRONT_CENTER:
                case ADM_CH_LFE:
                    out[0]  += *in * 0.707;
                    out[1]  += *in * 0.707;
                    break;
                case ADM_CH_FRONT_LEFT:
                    out[0]  += *in;
                    break;
                case ADM_CH_FRONT_RIGHT:
                    out[1]  += *in;
                    break;
                case ADM_CH_REAR_CENTER:
                    ds = me->dolby.DolbyShift90(0, *in);
                    out[0]  += ds        * 0.707;
                    out[1]  += ds * -1.0 * 0.707;
                    break;
                case ADM_CH_REAR_LEFT:
                    ds = me->dolby.DolbyShift90(1, *in);
                    out[0]  += ds        * 0.707;
                    out[1]  += ds * -1.0 * 0.707;
                    break;
                case ADM_CH_REAR_RIGHT:
                    ds = me->dolby.DolbyShift90(2, *in);
                    out[0]  += ds        * 0.707;
                    out[1]  += ds * -1.0 * 0.707;
                    break;
                case ADM_CH_SIDE_LEFT:
                    ds = me->dolby.DolbyShift90(3, *in);
                    out[0]  += *in * 0.707;
                    out[0]  += ds        * 0.707 * 0.707;
                    out[1]  += ds * -1.0 * 0.707 * 0.707;
                    break;
                case ADM_CH_SIDE_RIGHT:
                    ds = me->dolby.DolbyShift90(4, *in);
                    out[1]  += *in * 0.5;
                    out[0]  += ds        * 0.707 * 0.707;
                    out[1]  += ds * -1.0 * 0.707 * 0.707;
                    break;
                default:
                    break;
            }
            in++;
        }
        out += 2;
    }

    return nbSample*2;
}

static int MDolbyProLogic2(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
    memset(out, 0, sizeof(float) * nbSample * 2);
    float ds;

    for (int i = 0; i < nbSample; i++) {
        for (int c = 0; c < chan; c++) {
            switch (chanMap[c]) {
                case ADM_CH_MONO:
                case ADM_CH_FRONT_CENTER:
                case ADM_CH_LFE:
                    out[0]  += *in * 0.707;
                    out[1]  += *in * 0.707;
                    break;
                case ADM_CH_FRONT_LEFT:
                    out[0]  += *in;
                    break;
                case ADM_CH_FRONT_RIGHT:
                    out[1]  += *in;
                    break;
                case ADM_CH_REAR_CENTER:
                    ds = me->dolby.DolbyShift90(0, *in);
                    out[0]  += ds        * 0.707;
                    out[1]  += ds * -1.0 * 0.707;
                    break;
                case ADM_CH_REAR_LEFT:
                    ds = me->dolby.DolbyShift90(1, *in);
                    out[0]  += ds        * 0.866;
                    out[1]  += ds * -1.0 * 0.5;
                    break;
                case ADM_CH_REAR_RIGHT:
                    ds = me->dolby.DolbyShift90(2, *in);
                    out[0]  += ds        * 0.5;
                    out[1]  += ds * -1.0 * 0.866;
                    break;
                case ADM_CH_SIDE_LEFT:
                    ds = me->dolby.DolbyShift90(3, *in);
                    out[0]  += *in * 0.707;
                    out[0]  += ds        * 0.8165 * 0.707;
                    out[1]  += ds * -1.0 * 0.5774 * 0.707;
                    break;
                case ADM_CH_SIDE_RIGHT:
                    ds = me->dolby.DolbyShift90(4, *in);
                    out[1]  += *in * 0.707;
                    out[0]  += ds        * 0.5774 * 0.707;
                    out[1]  += ds * -1.0 * 0.8165 * 0.707;
                    break;
                default:
                    break;
            }
            in++;
        }
        out += 2;
    }

    return nbSample*2;
}


float AUDMAudioFilterMixer::getSHphSample(int sel, float in, float alpha, int delay)
{
    float * dl = sHphDelay[sel];
    int p = sHphPos[sel] & 1023;
    int pNext = (p+1) & 1023;
    int pPast = (pNext - delay)&1023;
    dl[pNext] = (alpha * in) + ((1.0-alpha) * dl[p])*0.9999;
    
    sHphPos[sel] = pNext;
    return dl[pPast];
}

static int MSurroundHeadphones(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)
{
    memset(out, 0, sizeof(float) * nbSample * 2);
    WAVHeader * info = me->getInfo();
    float dt = 1.0/((float)info->frequency);
    float cutoff = 700.0;
    float alpha = 6.2831853*dt*cutoff/(6.2831853*dt*cutoff + 1.0);
    int delayFront = 0.0003/dt;
    int delaySide = 0.0006/dt;
    int delayRear = 0.0004/dt;
    
    float ds;

    for (int i = 0; i < nbSample; i++) {
        for (int c = 0; c < chan; c++) {
            switch (chanMap[c]) {
                case ADM_CH_MONO:
                case ADM_CH_FRONT_CENTER:
                case ADM_CH_LFE:
                    out[0]  += *in * 0.707;
                    out[1]  += *in * 0.707;
                    break;
                case ADM_CH_REAR_CENTER:
                    ds = me->getSHphSample(0,*in,alpha,0) + *in*0.05;
                    out[0]  += ds        * 0.707;
                    out[1]  += ds * -1.0 * 0.707;
                    break;
                case ADM_CH_FRONT_LEFT:
                    ds = me->getSHphSample(1,*in,1.0,delayFront) + *in*0.05;
                    out[0]  += *in * 0.866;
                    out[1]  += ds  * 0.5;
                    break;
                case ADM_CH_FRONT_RIGHT:
                    ds =  me->getSHphSample(2,*in,1.0,delayFront) + *in*0.05;
                    out[0]  += ds  * 0.5;
                    out[1]  += *in * 0.866;
                    break;
                case ADM_CH_SIDE_LEFT:
                    ds = me->getSHphSample(3,*in,alpha,delaySide) + *in*0.05;
                    out[0]  += *in * 0.866;
                    out[1]  += ds  * 0.5;
                    break;
                case ADM_CH_SIDE_RIGHT:
                    ds = me->getSHphSample(4,*in,alpha,delaySide) + *in*0.05;
                    out[1]  += *in * 0.866;
                    out[0]  += ds  * 0.5;
                    break;
                case ADM_CH_REAR_LEFT:
                    ds = me->getSHphSample(5,*in,alpha,delayRear) + *in*0.05;
                    out[0]  += *in * 0.707;
                    out[1]  += ds  * -0.5;
                    break;
                case ADM_CH_REAR_RIGHT:
                    ds = me->getSHphSample(6,*in,alpha,delayRear) + *in*0.05;
                    out[0]  += ds  * -0.5;
                    out[1]  += *in * 0.707;
                    break;
                default:
                    break;
            }
            in++;
        }
        out += 2;
    }

    return nbSample*2;
}


typedef int MIXER(float *in,float *out,uint32_t nbSample,uint32_t chan,CHANNEL_TYPE *chanMap,AUDMAudioFilterMixer *me)  ;

static MIXER *matrixCall[CHANNEL_LAST] = {
NULL, MNto1, MStereo, MStereoHeadphones, M2F1R, M3F, M3F1R, M2F2R, M3F2R, M3F2RLFE, MDolbyProLogic, MDolbyProLogic2, MSurroundHeadphones
};
//_____________________________________________
uint32_t AUDMAudioFilterMixer::fill(uint32_t max,float *output,AUD_Status *status)
{
    if(bypass)
    {
        *status=AUD_END_OF_STREAM; // not recoverable for now
        return 0;
    }

    uint32_t rd = 0;
    int nbSampleMax=max/_wavHeader.channels;
    if(!nbSampleMax) nbSampleMax=1;

// Fill incoming buffer
    shrink();
    fillIncomingBuffer(status);
    // Block not filled ?
    if((_tail-_head)<input_channels)
    {
      if(*status==AUD_END_OF_STREAM && _head)
      {
        memset(_incomingBuffer.at(_head),0,sizeof(float) * input_channels);
        _tail=_head+input_channels;
        printf("[Mixer] Warning asked %u symbols\n",max);
      }
      else
      {
        return 0;
      }
    }
    // How many ?

    // Let's go
    int available=0;
    if(!nbSampleMax)
    {
      printf("[Mixer] Warning max %u, channels %u\n",max,input_channels);
    }
    available=(_tail-_head)/input_channels; // nb Sample
    ADM_assert(available);
    if(available > nbSampleMax) available=nbSampleMax;
    
    ADM_assert(available);
    

    // Now do the downsampling
	if (_output == CHANNEL_INVALID || true==ADM_audioCompareChannelMapping(&_wavHeader, _previous->getInfo(),
			inputChannelMapping,outputChannelMapping))
	{
		
		rd= (uint32_t)MCOPY(_incomingBuffer.at(_head),output,available,input_channels);
	} else 
	{
		MIXER *call=matrixCall[_output];
		rd= (uint32_t)call(_incomingBuffer.at(_head),output,available,input_channels,inputChannelMapping,this);
	}

    _head+=available*input_channels;
    return rd;
    
}
/**
    \fn AudioMixerIdToString
    \brief convert channel conf to plain string
*/
const char *AudioMixerIdToString(CHANNEL_CONF  cnf)
{
    for(int i=0;i<NB_MIXER_DESC;i++)
        if(cnf==mixerStringDescriptor[i].conf) return mixerStringDescriptor[i].desc;
    return NULL;
}
/**
    \fn AudioMuxerStringToId
    \brief convert channel conf from plain string
*/
CHANNEL_CONF AudioMuxerStringToId(const char *st)
{
  for(int i=0;i<NB_MIXER_DESC;i++)
  {
        const AudioChannelDesc *s=&(mixerStringDescriptor[i]);
        if(!strcasecmp(st,s->desc)) return s->conf;
  }
  return CHANNEL_INVALID;
}
