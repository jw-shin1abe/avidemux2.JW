/***************************************************************************
                          CubicLUT filter
    Algorithm:
        Copyright 2021 szlldm
    Ported to Avidemux:
        Copyright 2021 szlldm
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#pragma once

#include "ADM_byteBuffer.h"
#include "ADM_image.h"
#include "ADM_threads.h"

/**
    \class ADMVideoCubicLUT
*/
class  ADMVideoCubicLUT:public ADM_coreVideoFilter
{

  protected:
    cubicLUT              _param;
    uint8_t *             _lut;
    
    typedef struct {
        int         vstart, vincr;
        int n;
        float * sparse;
        uint8_t * lut;
    } worker_thread_arg;

    bool reloadLUT(void);
    static void * rgb2lut_worker(void * ptr);
    static void SparseRGBLUTtoLUT(float * sparse, int n, uint8_t * lut);
  public:
    ADMVideoCubicLUT(ADM_coreVideoFilter *in,CONFcouple *couples);
    ~ADMVideoCubicLUT();

    virtual const char    *getConfiguration(void);          /// Return  current configuration as a human readable string
    virtual bool          getNextFrame(uint32_t *fn,ADMImage *image);    /// Return the next image
    virtual bool          getCoupledConf(CONFcouple **couples) ;   /// Return the current filter configuration
    virtual void          setCoupledConf(CONFcouple *couples);
    virtual bool          configure(void) ;                 /// Start graphical user interface        

    static void CubicLUTProcess_C(ADMImage *img, int w, int h, uint8_t * lut);
    static const char * FileToLUT(const char *filename, bool hald, uint8_t * lut);  // return error message
};
