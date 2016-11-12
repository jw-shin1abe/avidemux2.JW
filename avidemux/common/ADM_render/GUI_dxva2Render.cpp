/**
    \author mean fixounet@free.fr 2010
*/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "config.h"
#include "ADM_default.h"
#include "DIA_coreToolkit.h"
#include "GUI_render.h"
#include "GUI_renderInternal.h"
#include "GUI_accelRender.h"
#include "ADM_threads.h"

#include "ADM_coreD3D.h"
#include <dxva2api.h>
#include <d3d9types.h>
#include "ADM_coreDxva2.h"
#include "../../qt4/ADM_userInterfaces/ADM_gui/T_preview.h"

/**
    \class sdlRender
*/
class dxvaRender: public VideoRenderBase,public ADM_QvideoDrawer
{
  protected:
              bool     useYV12;
  public:
                             dxvaRender( void ) ;
              virtual       ~dxvaRender();
              virtual	  bool init( GUI_WindowInfo *  window, uint32_t w, uint32_t h,renderZoom zoom);
              virtual	  bool stop(void);
              virtual   bool displayImage(ADMImage *pic);
              virtual   bool changeZoom(renderZoom newZoom);
              virtual   bool usingUIRedraw(void) {return false;};
              virtual   bool refresh(void) ;
                        const char *getName() {return "DXVA2";}
                        bool draw(QWidget *widget, QPaintEvent *ev);
  protected:
                        admMutex        lock;
                        GUI_WindowInfo  info;
                        IDirect3DSurface9 *mySurface;
                        uint8_t           *videoBuffer;
                        D3DDISPLAYMODE     displayMode;
                        IDirect3DDevice9  *d3dDevice;
                        IDirect3D9        *d3dHandle;
                        D3DLOCKED_RECT     d3dLock;
                        RECT               panScan;
                        RECT               targetRect;
                        ADM_Qvideo        *videoWidget;

protected:
};


/**
        \fn RenderSpawnDxva2
*/
VideoRenderBase *RenderSpawnDxva2()
{
    return new dxvaRender();
}

/**
    \fn dxvaRender
*/
dxvaRender::dxvaRender()
{
    ADM_info("creating Dxva2 render.\n");
    useYV12=false;
    mySurface=NULL;
    videoBuffer=NULL;
    d3dHandle=admD3D::getHandle();

}
/**
    \fn dxvaRender
*/
dxvaRender::~dxvaRender()
{
    ADM_info("Deleting surface\n");
    if(mySurface)
    {
                IDirect3DSurface9_Release(mySurface);
                mySurface=NULL;
    }
    ADM_info("dxva2 cleaned up\n");
}

/**
    \fn stop
*/
bool dxvaRender::stop(void)
{
    ADM_info("stopping dxva2 render.\n");
    return true;
}
/**
    \fn changeZoom
*/
bool dxvaRender::changeZoom(renderZoom newZoom)
{
        ADM_info("changing zoom, dxva render.\n");
        calcDisplayFromZoom(newZoom);
        currentZoom=newZoom;
        return false;
}


/**
    \fn changeZoom
*/
bool dxvaRender::init( GUI_WindowInfo *  window, uint32_t w, uint32_t h,renderZoom zoom)
{
    ADM_info("Initializing D3D render\n");
    info=*window;
    baseInit(w,h,zoom);

    if(!d3dHandle)
    {
        ADM_warning("No D3DHandle\n");
        return false;
    }

    if (FAILED(IDirect3D9_GetAdapterDisplayMode(d3dHandle,
                                                D3DADAPTER_DEFAULT,
                                                &displayMode)))
    {
        ADM_warning("Dxv2Render: Cannot get display mode\n");
        return 0;
    }

      // Check if we support YV12
    D3DFORMAT fmt=displayMode.Format;
    D3DFORMAT yv12=(D3DFORMAT)MAKEFOURCC('Y','V','1','2');
    if (FAILED(IDirect3D9_CheckDeviceFormatConversion(   d3dHandle, // adapter
                                                         D3DADAPTER_DEFAULT, // device type
                                                         D3DDEVTYPE_HAL, // adapter format
                                                         yv12, // render target format
                                                         fmt)))  // depth stencil format
    {
        useYV12=false;
        ADM_info("YV12 not supported\n");
    }
    else
    {
        useYV12=true;
        ADM_info("YV12 is supported\n");
    }



   D3DPRESENT_PARAMETERS presentationParameters;
   memset(&presentationParameters, 0, sizeof(presentationParameters));
   presentationParameters.Windowed               = TRUE;
   presentationParameters.SwapEffect             = D3DSWAPEFFECT_DISCARD;
   presentationParameters.Flags                  = D3DPRESENTFLAG_VIDEO;
   presentationParameters.hDeviceWindow          = (HWND)window->systemWindowId;
   presentationParameters.BackBufferWidth        = imageWidth;
   presentationParameters.BackBufferHeight       = imageHeight;
   presentationParameters.MultiSampleType        = D3DMULTISAMPLE_NONE;
   presentationParameters.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
   presentationParameters.BackBufferFormat       = fmt;
   presentationParameters.BackBufferCount        = 1;
   presentationParameters.EnableAutoDepthStencil = FALSE;


   if(FAILED(IDirect3D9_CreateDevice(  d3dHandle,
                                       D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL,  presentationParameters.hDeviceWindow,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                       &presentationParameters, &d3dDevice)))
   {
      ADM_warning("Failed to create D3D device\n");
      return false;
  }


    //
    D3DVIEWPORT9 viewPort = {0, 0, displayWidth, displayHeight, 0, 1};


    //
     if( FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(
               d3dDevice, w,h,
               yv12, D3DPOOL_DEFAULT, &mySurface, NULL)))
     {
                ADM_warning("Cannot create surface\n");
                return false;
     }
     if(FAILED(IDirect3DDevice9_ColorFill(d3dDevice, mySurface, NULL, D3DCOLOR_ARGB(0xFF, 0xff, 0, 0))))
     {
       ADM_warning("Color fill failed\n");
     }

    // put some defaults
    IDirect3DDevice9_SetRenderState(d3dDevice, D3DRS_SRCBLEND, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(d3dDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(d3dDevice, D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    IDirect3DDevice9_SetRenderState(d3dDevice, D3DRS_ALPHAREF, (DWORD)0x0);
    IDirect3DDevice9_SetRenderState(d3dDevice, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetSamplerState(d3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(d3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    //

    if(FAILED(IDirect3DDevice9_SetViewport(d3dDevice, &viewPort)))
    {
        ADM_warning("Cannot set D3D viewport\n");
        return false;
    }

   scaler=new ADMColorScalerFull(ADM_CS_BICUBIC,imageWidth,imageHeight,displayWidth,displayHeight,
            ADM_COLOR_YV12,
            ADM_COLOR_RGB32A
        );
    videoBuffer=new uint8_t[displayWidth*displayHeight*4];
    panScan.left  =0;
    panScan.right =imageWidth-1;
    panScan.top   =0;
    panScan.bottom=imageHeight-1;

    targetRect.left  =0;
    targetRect.right =displayWidth-1;
    targetRect.top   =0;
    targetRect.bottom=displayHeight-1;

    videoWidget=(ADM_Qvideo *)info.widget;
    videoWidget->useExternalRedraw(false);
    videoWidget->setDrawer(this);

    ADM_info("Dxva (D3D) init successful, dxva render. w=%d, h=%d,zoom=%d, displayWidth=%d, displayHeight=%d\n",(int)w,(int)h,(int)zoom,(int)displayWidth,(int)displayHeight);
    return true;
}
 /**
 \fn refresh
 */
 bool dxvaRender::refresh(void)
 {
   RECT rect = {0, 0, displayWidth, displayHeight};
   if(FAILED(IDirect3DDevice9_Present(d3dDevice, &rect, 0, 0, 0)))
   {
     ADM_warning("Refresh failed\n");
   }
   return true;
 }

bool dxvaRender::draw(QWidget *widget, QPaintEvent *ev)
{
  ADM_info("D3D : Draw!\n");
  refresh();
  return true;
}


/**
    \fn displayImage
*/
bool dxvaRender::displayImage(ADMImage *pic)
{
  IDirect3DSurface9 *bBuffer;
  //--
  if( FAILED(IDirect3DDevice9_GetBackBuffer(d3dDevice, 0, 0,
                                            D3DBACKBUFFER_TYPE_MONO,
                                            &bBuffer)))
  {
        ADM_warning("Cannot create backBuffer\n");
        return false;
  }
  if (FAILED(IDirect3DSurface9_LockRect(bBuffer,&d3dLock, NULL, 0)))
  {
      ADM_warning("Cannot lock surface\n");
      return false;
  }
  if(useYV12)
  {
    // copy
    uint8_t *y=pic->GetReadPtr(PLANAR_Y);
    int pitch=pic->GetPitch(PLANAR_Y);

    uint8_t *dst=(uint8_t *)d3dLock.pBits;
    int  dStride=d3dLock.Pitch;
  //bool BitBlit(uint8_t *dst, uint32_t pitchDst,uint8_t *src,uint32_t pitchSrc,uint32_t width, uint32_t height)
    BitBlit(dst,dStride,
            y,pitch,
            pic->GetWidth(PLANAR_Y),pic->GetHeight(PLANAR_Y));
  }
  if (FAILED(IDirect3DSurface9_UnlockRect(bBuffer)))
  {
      ADM_warning("Cannot unlock surface\n");
      return false;
  }
  if( FAILED(IDirect3DDevice9_Present(d3dDevice, &targetRect, 0, 0, 0)))
  {
    ADM_warning("Present failed\n");
  }
  return true;
//++
#if 0

    ADM_info("DXVARender :display image\n");

    if (FAILED(IDirect3DSurface9_LockRect(mySurface,&d3dLock, NULL, 0)))
    {
        ADM_warning("Cannot lock surface\n");
        return false;
    }
    if(useYV12)
    {
      // copy
      uint8_t *y=pic->GetReadPtr(PLANAR_Y);
      int pitch=pic->GetPitch(PLANAR_Y);

      uint8_t *dst=(uint8_t *)d3dLock.pBits;
      int  dStride=d3dLock.Pitch;
  //bool BitBlit(uint8_t *dst, uint32_t pitchDst,uint8_t *src,uint32_t pitchSrc,uint32_t width, uint32_t height)
      BitBlit(dst,dStride,y,pitch,pic->GetWidth(PLANAR_Y),pic->GetHeight(PLANAR_Y));
    } // Else
    {

    }

    if (FAILED(IDirect3DSurface9_UnlockRect(mySurface)))
    {
        ADM_warning("Cannot unlock surface\n");
        return false;
    }
    IDirect3DDevice9_BeginScene(d3dDevice);
    if (FAILED(IDirect3DDevice9_StretchRect(d3dDevice,
                                            mySurface,
                                            NULL,
                                            backBuffer,
                                            NULL,
                                            D3DTEXF_LINEAR)))
    {
        ADM_warning("Strech failed\n");
    }

    IDirect3DDevice9_EndScene(d3dDevice);
    /*if( FAILED(IDirect3DDevice9_Present(d3dDevice, &targetRect, 0, 0, 0)))
    {
      ADM_warning("Present failed\n");
    }*/

    return true;
#endif
}


// EOF
