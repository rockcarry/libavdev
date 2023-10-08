#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <d3d9.h>
#include "vdev.h"

#define ENABLE_WAIT_D3D_VSYNC  TRUE
#define AV_PIX_FMT_YUYV422     1
#define AV_PIX_FMT_UYVY422     17
#define AV_PIX_FMT_RGB32       30
typedef LPDIRECT3D9 (WINAPI *PFNDirect3DCreate9)(UINT);

typedef struct {
    HMODULE               hDll ;
    LPDIRECT3D9           pD3D9;
    LPDIRECT3DDEVICE9     pD3DDev;
    LPDIRECT3DSURFACE9    surface;   // offset screen surfaces
    LPDIRECT3DSURFACE9    bkbuf;     // back buffer surface
    D3DPRESENT_PARAMETERS d3dpp;
    D3DFORMAT             d3dfmt;
    BMP                   tbmp;

    #define FLAG_UPDATE   (1 << 6)
    uint32_t              flags;
    pthread_mutex_t       lock;
} VDEV;

static void d3d_release_and_create(VDEV *c, int release, int create)
{
    if (release) {
        if (c->surface) { IDirect3DSurface9_Release(c->surface); c->bkbuf  = NULL; }
        if (c->bkbuf  ) { IDirect3DSurface9_Release(c->bkbuf  ); c->bkbuf  = NULL; }
        if (c->pD3DDev) { IDirect3DDevice9_Release (c->pD3DDev); c->pD3DDev= NULL; }
    }
    if (create) {
        if (!c->pD3DDev) { // try create d3d device
            c->d3dpp.BackBufferWidth  = c->tbmp.width;
            c->d3dpp.BackBufferHeight = c->tbmp.height;
            IDirect3D9_CreateDevice(c->pD3D9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, c->d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &c->d3dpp, &c->pD3DDev);
            if (!c->pD3DDev) return;
        }
        if (!c->bkbuf) IDirect3DDevice9_GetBackBuffer(c->pD3DDev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &c->bkbuf); // try get back buffer
        if (!c->d3dfmt) { // try pixel format
            if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, c->tbmp.width, c->tbmp.height, D3DFMT_YUY2, D3DPOOL_DEFAULT, &c->surface, NULL))) {
                c->d3dfmt      = D3DFMT_YUY2;
                c->tbmp.pixfmt = AV_PIX_FMT_YUYV422;
                c->tbmp.cdepth = 16;
            } else if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, c->tbmp.width, c->tbmp.height, D3DFMT_UYVY, D3DPOOL_DEFAULT, &c->surface, NULL))) {
                c->d3dfmt      = D3DFMT_UYVY;
                c->tbmp.pixfmt = AV_PIX_FMT_UYVY422;
                c->tbmp.cdepth = 16;
            } else if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(c->pD3DDev, c->tbmp.width, c->tbmp.height, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &c->surface, NULL))) {
                c->d3dfmt      = D3DFMT_X8R8G8B8;
                c->tbmp.pixfmt = AV_PIX_FMT_RGB32;
                c->tbmp.cdepth = 32;
            }
        }
    }
}

void* vdev_init(int w, int h, char *params, PFN_VDEV_MSG_CB callback, void *cbctx)
{
    PFNDirect3DCreate9 create  = NULL;
    D3DDISPLAYMODE     d3dmode = {};

    VDEV *dev = calloc(1, sizeof(VDEV));
    if (!dev) return NULL;
    pthread_mutex_init(&dev->lock, NULL);
    if (!dev->lock) goto failed;
    dev->tbmp.width  = w;
    dev->tbmp.height = h;

    dev->hDll  = LoadLibrary(TEXT("d3d9.dll"));
    create     = (PFNDirect3DCreate9)GetProcAddress(dev->hDll, "Direct3DCreate9");
    if (!dev->hDll || !create) goto failed;
    dev->pD3D9 = create(D3D_SDK_VERSION);

    IDirect3D9_GetAdapterDisplayMode(dev->pD3D9, D3DADAPTER_DEFAULT, &d3dmode);
    dev->d3dpp.BackBufferFormat      = D3DFMT_UNKNOWN;
    dev->d3dpp.BackBufferCount       = 1;
    dev->d3dpp.MultiSampleType       = D3DMULTISAMPLE_NONE;
    dev->d3dpp.MultiSampleQuality    = 0;
    dev->d3dpp.SwapEffect            = D3DSWAPEFFECT_DISCARD;
    dev->d3dpp.hDeviceWindow         = (HWND)cbctx;
    dev->d3dpp.Windowed              = TRUE;
    dev->d3dpp.EnableAutoDepthStencil= FALSE;
    dev->d3dpp.PresentationInterval  = ENABLE_WAIT_D3D_VSYNC == FALSE ? D3DPRESENT_INTERVAL_IMMEDIATE : d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
    return dev;

failed:
    if (dev->hDll) FreeLibrary(dev->hDll);
    free(dev);
    return NULL;
}

void vdev_exit(void *ctx, int close)
{
    VDEV *dev = (VDEV*)ctx;
    if (!dev) return;
    d3d_release_and_create(dev, 1, 0);
    if (dev->hDll) FreeLibrary(dev->hDll);
    if (dev->lock) pthread_mutex_destroy(&dev->lock);
    free(dev);
}

BMP* vdev_lock(void *ctx)
{
    VDEV *dev = (VDEV*)ctx;
    if (!dev) return NULL;
    if (!dev->surface || (dev->flags & FLAG_UPDATE)) {
        dev->flags &= ~FLAG_UPDATE;
        d3d_release_and_create(dev, 1, 1);
    }
    if (!dev->surface) return NULL;
    D3DLOCKED_RECT rect;
    IDirect3DSurface9_LockRect(dev->surface, &rect, NULL, D3DLOCK_DISCARD);
    dev->tbmp.pdata  = rect.pBits;
    dev->tbmp.stride = rect.Pitch;
    return &dev->tbmp;
}
void vdev_unlock(void *ctx)
{
    VDEV *dev = (VDEV*)ctx;
    if (!dev) return;
    if (dev->surface && dev->bkbuf) {
        IDirect3DSurface9_UnlockRect(dev->surface);
        IDirect3DDevice9_StretchRect(dev->pD3DDev, dev->surface, NULL, dev->bkbuf, NULL, D3DTEXF_LINEAR);
        IDirect3DDevice9_Present(dev->pD3DDev, NULL, NULL, NULL, NULL);
    }
}

void  vdev_set(void *ctx, char *name, void *data) {}
void* vdev_get(void *ctx, char *name, void *data) { return NULL; }
