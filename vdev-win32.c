#include <unistd.h>
#include <windows.h>
#include <pthread.h>
#include "vdev.h"
#include "utils.h"

//++ directdraw types and defines
#define DDSCL_FULLSCREEN       0x00000001
#define DDSCL_EXCLUSIVE        0x00000010
#define DDSD_CAPS              0x00000001
#define DDSCAPS_PRIMARYSURFACE 0x00000200
#define DDLOCK_WAIT            0x00000001

typedef struct {
    DWORD dwColorSpaceLowValue;  /* low boundary of color space that is to
                                  * be treated as Color Key, inclusive */
    DWORD dwColorSpaceHighValue; /* high boundary of color space that is
                                  * to be treated as Color Key, inclusive */
} DDCOLORKEY,*LPDDCOLORKEY;

typedef struct {
    DWORD        dwSize;                 /* 0: size of structure */
    DWORD        dwFlags;                /* 4: pixel format flags */
    DWORD        dwFourCC;               /* 8: (FOURCC code) */
    union {
        DWORD    dwRGBBitCount;          /* C: how many bits per pixel */
        DWORD    dwYUVBitCount;          /* C: how many bits per pixel */
        DWORD    dwZBufferBitDepth;      /* C: how many bits for z buffers */
        DWORD    dwAlphaBitDepth;        /* C: how many bits for alpha channels*/
        DWORD    dwLuminanceBitCount;
        DWORD    dwBumpBitCount;
        DWORD    dwPrivateFormatBitCount;
    } DUMMYUNIONNAME1;
    union {
        DWORD    dwRBitMask;             /* 10: mask for red bit*/
        DWORD    dwYBitMask;             /* 10: mask for Y bits*/
        DWORD    dwStencilBitDepth;
        DWORD    dwLuminanceBitMask;
        DWORD    dwBumpDuBitMask;
        DWORD    dwOperations;
    } DUMMYUNIONNAME2;
    union {
        DWORD    dwGBitMask;             /* 14: mask for green bits*/
        DWORD    dwUBitMask;             /* 14: mask for U bits*/
        DWORD    dwZBitMask;
        DWORD    dwBumpDvBitMask;
        struct {
            WORD wFlipMSTypes;
            WORD wBltMSTypes;
        } MultiSampleCaps;
    } DUMMYUNIONNAME3;
    union {
        DWORD    dwBBitMask;             /* 18: mask for blue bits*/
        DWORD    dwVBitMask;             /* 18: mask for V bits*/
        DWORD    dwStencilBitMask;
        DWORD    dwBumpLuminanceBitMask;
    } DUMMYUNIONNAME4;
    union {
        DWORD    dwRGBAlphaBitMask;      /* 1C: mask for alpha channel */
        DWORD    dwYUVAlphaBitMask;      /* 1C: mask for alpha channel */
        DWORD    dwLuminanceAlphaBitMask;
        DWORD    dwRGBZBitMask;          /* 1C: mask for Z channel */
        DWORD    dwYUVZBitMask;          /* 1C: mask for Z channel */
    } DUMMYUNIONNAME5;
                                         /* 20: next structure */
} DDPIXELFORMAT,*LPDDPIXELFORMAT;

typedef struct {
    DWORD   dwCaps;    /* capabilities of surface wanted */
} DDSCAPS, *LPDDSCAPS;

typedef struct {
    DWORD           dwSize;             /* 0:  size of the DDSURFACEDESC structure */
    DWORD           dwFlags;            /* 4:  determines what fields are valid */
    DWORD           dwHeight;           /* 8:  height of surface to be created */
    DWORD           dwWidth;            /* C:  width of input surface */
    LONG            lPitch;             /* 10: distance to start of next line (return value only) */
    DWORD           dwBackBufferCount;  /* 14: number of back buffers requested */
    union {
        DWORD       dwMipMapCount;      /* 18: number of mip-map levels requested */
        DWORD       dwZBufferBitDepth;  /* 18: depth of Z buffer requested */
        DWORD       dwRefreshRate;      /* 18: refresh rate (used when display mode is described) */
    } DUMMYUNIONNAME2;
    DWORD           dwAlphaBitDepth;    /* 1C: depth of alpha buffer requested */
    DWORD           dwReserved;         /* 20: reserved */
    LPVOID          lpSurface;          /* 24: pointer to the associated surface memory */
    DDCOLORKEY      ddckCKDestOverlay;  /* 28: CK for dest overlay use */
    DDCOLORKEY      ddckCKDestBlt;      /* 30: CK for destination blt use */
    DDCOLORKEY      ddckCKSrcOverlay;   /* 38: CK for source overlay use */
    DDCOLORKEY      ddckCKSrcBlt;       /* 40: CK for source blt use */
    DDPIXELFORMAT   ddpfPixelFormat;    /* 48: pixel format description of the surface */
    DDSCAPS         ddsCaps;            /* 68: direct draw surface caps */
} DDSURFACEDESC,*LPDDSURFACEDESC;

typedef struct {
    HRESULT (WINAPI *QueryInterface)(void *this, IID *riid, void** ppvObject);
    ULONG   (WINAPI *AddRef )(void *this);
    ULONG   (WINAPI *Release)(void *this);

    DWORD    dwReserved1[22];
    HRESULT (WINAPI *Lock)(void *this, LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);
    DWORD    dwReserved2[6];
    HRESULT (WINAPI *Unlock)(void *this, LPVOID lpSurfaceData);
} IDirectDrawSurfaceVtbl;

typedef struct {
    IDirectDrawSurfaceVtbl *pVtbl;
} IDirectDrawSurface, *LPDIRECTDRAWSURFACE;

typedef struct {
    HRESULT (WINAPI *QueryInterface)(void *this, IID *riid, void** ppvObject);
    ULONG   (WINAPI *AddRef )(void *this);
    ULONG   (WINAPI *Release)(void *this);

    HRESULT (WINAPI *Compact               )(void *this);
    HRESULT (WINAPI *CreateClipper         )(void *this, DWORD dwFlags, void *lplpDDClipper, IUnknown *pUnkOuter);
    HRESULT (WINAPI *CreatePalette         )(void *this, DWORD dwFlags, void *lpColorTable, void *lplpDDPalette, IUnknown *pUnkOuter);
    HRESULT (WINAPI *CreateSurface         )(void *this, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter);
    HRESULT (WINAPI *DuplicateSurface      )(void *this, LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE *lplpDupDDSurface);
    HRESULT (WINAPI *EnumDisplayModes      )(void *this, DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, void *lpEnumModesCallback);
    HRESULT (WINAPI *EnumSurfaces          )(void *this, DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, void *lpEnumSurfacesCallback);
    HRESULT (WINAPI *FlipToGDISurface      )(void *this);
    DWORD    dwReserved1;
    HRESULT (WINAPI *GetDisplayMode        )(void *this, LPDDSURFACEDESC lpDDSurfaceDesc);
    HRESULT (WINAPI *GetFourCCCodes        )(void *this, LPDWORD lpNumCodes, LPDWORD lpCodes);
    HRESULT (WINAPI *GetGDISurface         )(void *this, LPDIRECTDRAWSURFACE *lplpGDIDDSurface);
    HRESULT (WINAPI *GetMonitorFrequency   )(void *this, LPDWORD lpdwFrequency);
    HRESULT (WINAPI *GetScanLine           )(void *this, LPDWORD lpdwScanLine );
    HRESULT (WINAPI *GetVerticalBlankStatus)(void *this, BOOL *lpbIsInVB);
    HRESULT (WINAPI *Initialize            )(void *this, GUID *lpGUID);
    HRESULT (WINAPI *RestoreDisplayMode    )(void *this);
    HRESULT (WINAPI *SetCooperativeLevel   )(void *this, HWND hWnd, DWORD dwFlags);
    HRESULT (WINAPI *SetDisplayMode        )(void *this, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP);
    HRESULT (WINAPI *WaitForVerticalBlank  )(void *this, DWORD dwFlags, HANDLE hEvent);
} IDirectDrawVtbl;

typedef struct {
    IDirectDrawVtbl *pVtbl;
} IDirectDraw;

typedef void*(WINAPI *PFN_DirectDrawCreate)(void *guid, void **lpddraw, void *iunknown);
//-- directdraw types and defines

typedef struct {
    HWND     hwnd;
    HDC      hdc;
    HBITMAP  hbmp;
    BMP      tbmp;
    uint32_t keybits[8];
    int32_t  mousex;
    int32_t  mousey;
    int32_t  mousebtns;

    HMODULE             hDDrawDll;
    IDirectDraw        *lpDirectDraw;
    IDirectDrawSurface *lpDDSPrimary;

    #define FLAG_DDRAW  (1 << 0)
    #define FLAG_INITED (1 << 1)
    #define FLAG_CLOSED (1 << 2)
    #define FLAG_NOSHOW (1 << 3)
    #define FLAG_LOCKED (1 << 4)
    uint32_t  flags;
    pthread_mutex_t lock;
    pthread_t    hthread;

    PFN_VDEV_MSG_CB callback;
    void           *cbctx;
} VDEV;

#define VDEV_WND_CLASS TEXT("VDevWndClass")
#define VDEV_WND_NAME  TEXT("VDevWindow"  )

static int init_free_for_ddraw_gdi(VDEV *vdev, int init)
{
    if (vdev->flags & FLAG_DDRAW) {
        if (init) {
            PFN_DirectDrawCreate create = NULL;
            DDSURFACEDESC        ddsd   = {0};
            vdev->hDDrawDll = LoadLibrary(TEXT("ddraw.dll"));
            if (!vdev->hDDrawDll) return -1;
            create = (PFN_DirectDrawCreate)GetProcAddress(vdev->hDDrawDll, "DirectDrawCreate");
            if (!create) return -1;
            create(NULL, (void**)&vdev->lpDirectDraw, NULL);
            if (vdev->lpDirectDraw == NULL) return -1;
            vdev->lpDirectDraw->pVtbl->SetCooperativeLevel(vdev->lpDirectDraw, vdev->hwnd, DDSCL_FULLSCREEN|DDSCL_EXCLUSIVE);
            vdev->lpDirectDraw->pVtbl->SetDisplayMode(vdev->lpDirectDraw, vdev->tbmp.width, vdev->tbmp.height, 32);

            ddsd.dwSize  = sizeof(ddsd);
            ddsd.dwFlags = DDSD_CAPS;
            ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
            vdev->lpDirectDraw->pVtbl->CreateSurface(vdev->lpDirectDraw, &ddsd, &vdev->lpDDSPrimary, NULL);
            if (vdev->lpDDSPrimary == NULL) return -1;
            vdev->tbmp.width  = ddsd.dwWidth ;
            vdev->tbmp.height = ddsd.dwHeight;
        } else {
            if (vdev->lpDDSPrimary) { vdev->lpDDSPrimary->pVtbl->Release(vdev->lpDDSPrimary); vdev->lpDDSPrimary = NULL; }
            if (vdev->lpDirectDraw) { vdev->lpDirectDraw->pVtbl->Release(vdev->lpDirectDraw); vdev->lpDirectDraw = NULL; }
            if (vdev->hDDrawDll   ) { FreeLibrary(vdev->hDDrawDll); vdev->hDDrawDll = NULL; }
        }
    } else {
        if (init) {
            BITMAPINFO bmpinfo = {};
            BITMAP     bitmap  = {};

            bmpinfo.bmiHeader.biSize        =  sizeof(bmpinfo);
            bmpinfo.bmiHeader.biWidth       =  vdev->tbmp.width ;
            bmpinfo.bmiHeader.biHeight      = -vdev->tbmp.height;
            bmpinfo.bmiHeader.biPlanes      =  1;
            bmpinfo.bmiHeader.biBitCount    =  32;
            bmpinfo.bmiHeader.biCompression =  BI_RGB;

            vdev->hdc  = CreateCompatibleDC(NULL);
            vdev->hbmp = CreateDIBSection(vdev->hdc, &bmpinfo, DIB_RGB_COLORS, (void**)&(vdev->tbmp.pdata), NULL, 0);
            if (!vdev->hdc || !vdev->hbmp || !vdev->tbmp.pdata) return -1;

            GetObject(vdev->hbmp, sizeof(BITMAP), &bitmap);
            SelectObject(vdev->hdc, vdev->hbmp);
        } else {
            if (vdev->hdc ) DeleteDC    (vdev->hdc );
            if (vdev->hbmp) DeleteObject(vdev->hbmp);
        }
    }

    pthread_mutex_lock(&vdev->lock);
    vdev->flags |= FLAG_INITED;
    pthread_mutex_unlock(&vdev->lock);
    return 0;
}

static LRESULT CALLBACK VDEV_WNDPROC(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps = {0};
    HDC        hdc = NULL;
#ifdef _WIN64
    VDEV *vdev = (VDEV*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
#else
    VDEV *vdev = (VDEV*)GetWindowLong(hwnd, GWL_USERDATA);
#endif
    int  ret = -1;
    switch (uMsg) {
    case WM_KEYUP: case WM_KEYDOWN: case WM_SYSKEYUP: case WM_SYSKEYDOWN:
        if (vdev->callback) ret = vdev->callback(vdev->cbctx, VDEV_MSG_KEY_EVENT, uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN, wParam, NULL);
        if (ret != 0 && uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) PostQuitMessage(0);
        int idx = ((BYTE)wParam) / 32;
        int bit = ((BYTE)wParam) % 32;
        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) vdev->keybits[idx] |= (1 << bit);
        else vdev->keybits[idx] &= ~(1 << bit);
        return 0;
    case WM_MOUSEMOVE:
        vdev->mousex = (int32_t)((lParam >> 0) & 0xFFFF);
        vdev->mousey = (int32_t)((lParam >>16) & 0xFFFF);
        return 0;
    case WM_MOUSEWHEEL:
        return 0;
    case WM_LBUTTONUP  : vdev->mousebtns &= ~(1 << 0); return 0;
    case WM_LBUTTONDOWN: vdev->mousebtns |=  (1 << 0); return 0;
    case WM_MBUTTONUP  : vdev->mousebtns &= ~(1 << 1); return 0;
    case WM_MBUTTONDOWN: vdev->mousebtns |=  (1 << 1); return 0;
    case WM_RBUTTONUP  : vdev->mousebtns &= ~(1 << 2); return 0;
    case WM_RBUTTONDOWN: vdev->mousebtns |=  (1 << 2); return 0;
    case WM_PAINT:
        if (vdev->flags & FLAG_DDRAW) break;
        hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
            ps.rcPaint.right  - ps.rcPaint.left,
            ps.rcPaint.bottom - ps.rcPaint.top,
            vdev->hdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static void* vdev_thread_proc(void *param)
{
    VDEV    *vdev = (VDEV*)param;
    WNDCLASS wc   = {0};
    RECT     rect = {0};
    MSG      msg  = {0};
    int      x, y, w, h;

    wc.lpfnWndProc   = VDEV_WNDPROC;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon  (NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = VDEV_WND_CLASS;
    if (!RegisterClass(&wc)) return NULL;

    vdev->hwnd = CreateWindow(VDEV_WND_CLASS, VDEV_WND_NAME, (vdev->flags & FLAG_DDRAW) ? WS_POPUP : (WS_SYSMENU|WS_MINIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, vdev->tbmp.width, vdev->tbmp.height,
        NULL, NULL, wc.hInstance, NULL);
    if (!vdev->hwnd) goto done;

#ifdef _WIN64
    SetWindowLongPtr(vdev->hwnd, GWLP_USERDATA, (LONG_PTR)vdev);
#else
    SetWindowLong(vdev->hwnd, GWL_USERDATA, (LONG)vdev);
#endif
    GetClientRect(vdev->hwnd, &rect);
    w = vdev->tbmp.width  + (vdev->tbmp.width  - rect.right );
    h = vdev->tbmp.height + (vdev->tbmp.height - rect.bottom);
    x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    x = x > 0 ? x : 0;
    y = y > 0 ? y : 0;

    MoveWindow(vdev->hwnd, x, y, w, h, FALSE);
    if (init_free_for_ddraw_gdi(vdev, 1) == -1) goto done;
    if (!(vdev->flags & FLAG_NOSHOW)) {
        ShowWindow(vdev->hwnd, SW_SHOW);
        UpdateWindow(vdev->hwnd);
    }

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage (&msg);
    }

done:
    pthread_mutex_lock(&vdev->lock);
    vdev->flags |= FLAG_CLOSED;
    pthread_mutex_unlock(&vdev->lock);
    while (vdev->flags & FLAG_LOCKED) usleep(10 * 1000);
    init_free_for_ddraw_gdi(vdev, 0);
    if (vdev->hwnd) { DestroyWindow(vdev->hwnd); vdev->hwnd = NULL; }
    return NULL;
}

void* vdev_init(int w, int h, char *params, PFN_VDEV_MSG_CB callback, void *cbctx)
{
    VDEV *vdev = calloc(1, sizeof(VDEV));
    if (!vdev) return NULL;

    if (params) {
        if      (strcmp(params, "fullscreen") == 0) vdev->flags |= FLAG_DDRAW;
        else if (strcmp(params, "inithidden") == 0) vdev->flags |= FLAG_NOSHOW;
    }
    vdev->callback       = callback;
    vdev->cbctx          = cbctx;
    vdev->tbmp.width     = w;
    vdev->tbmp.height    = h;
    vdev->tbmp.stride    = w * sizeof(uint32_t);
    vdev->tbmp.cdepth    = 32;
    pthread_mutex_init(&vdev->lock, NULL);
    pthread_create(&vdev->hthread, NULL, vdev_thread_proc, vdev);
    while (!(vdev->flags & (FLAG_INITED | FLAG_CLOSED))) usleep(100 * 1000);
    if (vdev->flags & FLAG_CLOSED) {
        vdev_exit(vdev, 0);
        vdev = NULL;
    }
    return vdev;
}

void vdev_exit(void *ctx, int close)
{
    VDEV *vdev = (VDEV*)ctx;
    if (!vdev) return;
    if (close && vdev->hwnd) PostMessage(vdev->hwnd, WM_CLOSE, 0, 0);
    if (vdev->hthread) pthread_join(vdev->hthread, NULL);
    if (vdev->lock) pthread_mutex_destroy(&vdev->lock);
    free(vdev);
}

BMP* vdev_lock(void *ctx)
{
    if (!ctx) return NULL;
    VDEV *vdev = ctx;
    if (vdev->flags & FLAG_CLOSED) return NULL;
    pthread_mutex_lock(&vdev->lock);
    vdev->flags |= FLAG_LOCKED;
    pthread_mutex_unlock(&vdev->lock);
    if (vdev->flags & FLAG_DDRAW) {
        DDSURFACEDESC ddsd = { sizeof(ddsd) };
        vdev->lpDDSPrimary->pVtbl->Lock(vdev->lpDDSPrimary, NULL, &ddsd, DDLOCK_WAIT, NULL);
        vdev->tbmp.pdata   = ddsd.lpSurface;
    }
    return &vdev->tbmp;
}

void vdev_unlock(void *ctx)
{
    if (!ctx) return;
    VDEV *vdev = ctx;
    if (vdev->flags & FLAG_DDRAW) {
        vdev->lpDDSPrimary->pVtbl->Unlock(vdev->lpDDSPrimary, NULL);
    } else {
#if 1
        HDC hdc = GetDC(vdev->hwnd);
        BitBlt(hdc, 0, 0, vdev->tbmp.width, vdev->tbmp.height, vdev->hdc, 0, 0, SRCCOPY);
        ReleaseDC(vdev->hwnd, hdc);
#else
        InvalidateRect(vdev->hwnd, NULL, FALSE);
#endif
    }
    pthread_mutex_lock(&vdev->lock);
    vdev->flags &= ~FLAG_LOCKED;
    pthread_mutex_unlock(&vdev->lock);
}

void vdev_set(void *ctx, char *name, void *data)
{
    VDEV *vdev = (VDEV*)ctx;
    if (!ctx || !name) return;
    if (strcmp(name, "show") == 0) {
        ShowWindow(vdev->hwnd, SW_SHOW);
        UpdateWindow(vdev->hwnd);
    }
}

long vdev_get(void *ctx, char *name, void *data)
{
    VDEV *vdev = (VDEV*)ctx;
    if (!ctx || !name) return 0;
    if (strcmp(name, "state") == 0   ) return (long)((vdev->flags & FLAG_CLOSED) ? "closed" : "running");
    if (strstr(name, "key_" ) == name) return !!(vdev->keybits[(unsigned)name[4] / 32] & (1 << ((unsigned)name[4] % 32)));
    if (strcmp(name, "mouse") == 0   ) return (long)&vdev->mousex;
    return 0;
}

