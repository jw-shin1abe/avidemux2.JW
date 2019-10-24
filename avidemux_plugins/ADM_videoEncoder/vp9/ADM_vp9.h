/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef ADM_VP9_H
#define ADM_VP9_H
#include "ADM_coreVideoEncoder.h"
#include "ADM_encoderConf.h"

extern "C"
{
#include "vpx/vpx_encoder.h"
};

enum vp9DeadlinePreset
{
    REALTIME=0,
    GOOD_QUALITY=1,
    BEST_QUALITY=2
};

#define VP9_DEFAULT_CONF \
{ \
     2, /* nbThreads */ \
    13, /* speed = VP8E_SET_CPUUSED + 9 */ \
     1, /* deadline = 1s */ \
    20, /* qMin */ \
    30, /* qMax */ \
     0  /* fullrange */ \
}

/**
    \class vp9Encoder
    \brief libvpx VP9 encoder
*/
class vp9Encoder : public ADM_coreVideoEncoder
{
protected:
                vpx_codec_ctx_t     context;
                vpx_codec_enc_cfg_t param;
                vpx_codec_iface_t   *iface;
                vpx_image_t         *pic;
                std::vector <const vpx_codec_cx_pkt *> packetQueue;
                int                 plane;
                uint32_t            ticks;
                uint32_t            dline;
                bool                flush;
                bool                postAmble(ADMBitstream *out);
public:
                                    vp9Encoder(ADM_coreVideoFilter *src, bool globalHeader);
    virtual                         ~vp9Encoder();
    virtual    bool                 setup(void);
    virtual    bool                 encode(ADMBitstream *out);
    virtual const char              *getFourcc(void) {return "VP9 ";}
    virtual    bool                 isDualPass(void);
};

#endif