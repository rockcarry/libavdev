#include <unistd.h>
#include <windows.h>
#include <pthread.h>
#include "texture.h"
#include "display.h"
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
    TEXTURE texture;
    HWND    hwnd;
    HDC     hdc;
    HBITMAP hbmp;

    HMODULE             hDDrawDll;
    IDirectDraw        *lpDirectDraw;
    IDirectDrawSurface *lpDDSPrimary;

    #define FLAG_DDRAW  (1 << 0)
    #define FLAG_INITED (1 << 1)
    #define FLAG_CLOSED (1 << 2)
    #define FLAG_NOSHOW (1 << 3)
    uint32_t  flags;
    pthread_t hthread;

    PFN_DISP_MSG_CB callback;
    void           *cbctx;
} DISP;

#define TINYGL_WND_CLASS TEXT("TinyGLWndClass")
#define TINYGL_WND_NAME  TEXT("TinyGLWindow"  )

static int init_free_for_ddraw_gdi(DISP *disp, int init)
{
    if (disp->flags & FLAG_DDRAW) {
        if (init) {
            PFN_DirectDrawCreate create = NULL;
            DDSURFACEDESC        ddsd   = {0};
            disp->hDDrawDll = LoadLibrary(TEXT("ddraw.dll"));
            if (!disp->hDDrawDll) return -1;
            create = (PFN_DirectDrawCreate)GetProcAddress(disp->hDDrawDll, "DirectDrawCreate");
            if (!create) return -1;
            create(NULL, (void**)&disp->lpDirectDraw, NULL);
            if (disp->lpDirectDraw == NULL) return -1;
            disp->lpDirectDraw->pVtbl->SetCooperativeLevel(disp->lpDirectDraw, disp->hwnd, DDSCL_FULLSCREEN|DDSCL_EXCLUSIVE);
            disp->lpDirectDraw->pVtbl->SetDisplayMode(disp->lpDirectDraw, disp->texture.w, disp->texture.h, 32);

            ddsd.dwSize  = sizeof(ddsd);
            ddsd.dwFlags = DDSD_CAPS;
            ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
            disp->lpDirectDraw->pVtbl->CreateSurface(disp->lpDirectDraw, &ddsd, &disp->lpDDSPrimary, NULL);
            if (disp->lpDDSPrimary == NULL) return -1;
            disp->texture.w = ddsd.dwWidth;
            disp->texture.h = ddsd.dwHeight;
        } else {
            if (disp->lpDDSPrimary) { disp->lpDDSPrimary->pVtbl->Release(disp->lpDDSPrimary); disp->lpDDSPrimary = NULL; }
            if (disp->lpDirectDraw) { disp->lpDirectDraw->pVtbl->Release(disp->lpDirectDraw); disp->lpDirectDraw = NULL; }
            if (disp->hDDrawDll   ) { FreeLibrary(disp->hDDrawDll); disp->hDDrawDll = NULL; }
        }
    } else {
        if (init) {
            BITMAPINFO bmpinfo = {};
            BITMAP     bitmap  = {};

            bmpinfo.bmiHeader.biSize        =  sizeof(bmpinfo);
            bmpinfo.bmiHeader.biWidth       =  disp->texture.w;
            bmpinfo.bmiHeader.biHeight      = -disp->texture.h;
            bmpinfo.bmiHeader.biPlanes      =  1;
            bmpinfo.bmiHeader.biBitCount    =  32;
            bmpinfo.bmiHeader.biCompression =  BI_RGB;

            disp->hdc  = CreateCompatibleDC(NULL);
            disp->hbmp = CreateDIBSection(disp->hdc, &bmpinfo, DIB_RGB_COLORS, (void**)&(disp->texture.data), NULL, 0);
            if (!disp->hdc || !disp->hbmp || !disp->texture.data) return -1;

            GetObject(disp->hbmp, sizeof(BITMAP), &bitmap);
            SelectObject(disp->hdc, disp->hbmp);
        } else {
            if (disp->hdc ) DeleteDC    (disp->hdc );
            if (disp->hbmp) DeleteObject(disp->hbmp);
        }
    }

    disp->flags |= FLAG_INITED;
    return 0;
}

static void disp_texture_lock(TEXTURE *t)
{
    DISP *disp = container_of(t, DISP, texture);
    if (disp->flags & FLAG_DDRAW) {
        DDSURFACEDESC ddsd = { sizeof(ddsd) };
        disp->lpDDSPrimary->pVtbl->Lock(disp->lpDDSPrimary, NULL, &ddsd, DDLOCK_WAIT, NULL);
        t->w      = ddsd.dwWidth;
        t->h      = ddsd.dwHeight;
        t->data   = ddsd.lpSurface;
    }
}

static void disp_texture_unlock(TEXTURE *t)
{
    DISP *disp = container_of(t, DISP, texture);
    if (disp->flags & FLAG_CLOSED) return;
    if (disp->flags & FLAG_DDRAW) {
        disp->lpDDSPrimary->pVtbl->Unlock(disp->lpDDSPrimary, NULL);
        t->data = NULL;
    } else {
#if 1
        HDC hdc = GetDC(disp->hwnd);
        BitBlt(hdc, 0, 0, disp->texture.w, disp->texture.h, disp->hdc, 0, 0, SRCCOPY);
        ReleaseDC(disp->hwnd, hdc);
#else
        InvalidateRect(disp->hwnd, NULL, FALSE);
#endif
    }
}

static LRESULT CALLBACK DISP_WNDPROC(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps = {0};
    HDC        hdc = NULL;
#ifdef _WIN64
    DISP *disp = (DISP*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
#else
    DISP *disp = (DISP*)GetWindowLong(hwnd, GWL_USERDATA);
#endif
    int  ret = -1;
    switch (uMsg) {
    case WM_KEYUP: case WM_KEYDOWN: case WM_SYSKEYUP: case WM_SYSKEYDOWN:
        if (disp->callback) ret = disp->callback(disp->cbctx, DISP_MSG_KEY_EVENT, uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN, wParam, NULL);
        if (ret != 0 && uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) PostQuitMessage(0);
        return 0;
    case WM_MOUSEMOVE: case WM_MOUSEWHEEL:
    case WM_LBUTTONUP: case WM_LBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDOWN:
        return 0;
    case WM_PAINT:
        if (disp->flags & FLAG_DDRAW) return DefWindowProc(hwnd, uMsg, wParam, lParam);
        hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
            ps.rcPaint.right  - ps.rcPaint.left,
            ps.rcPaint.bottom - ps.rcPaint.top,
            disp->hdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

static void* disp_thread_proc(void *param)
{
    DISP    *disp = (DISP*)param;
    WNDCLASS wc   = {0};
    RECT     rect = {0};
    MSG      msg  = {0};
    int      x, y, w, h;

    wc.lpfnWndProc   = DISP_WNDPROC;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon  (NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = TINYGL_WND_CLASS;
    if (!RegisterClass(&wc)) return NULL;

    disp->hwnd = CreateWindow(TINYGL_WND_CLASS, TINYGL_WND_NAME, (disp->flags & FLAG_DDRAW) ? WS_POPUP : (WS_SYSMENU|WS_MINIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, disp->texture.w, disp->texture.h,
        NULL, NULL, wc.hInstance, NULL);
    if (!disp->hwnd) goto done;

#ifdef _WIN64
    SetWindowLongPtr(disp->hwnd, GWLP_USERDATA, (LONG_PTR)disp);
#else
    SetWindowLong(disp->hwnd, GWL_USERDATA, (LONG)disp);
#endif
    GetClientRect(disp->hwnd, &rect);
    w = disp->texture.w + (disp->texture.w - rect.right );
    h = disp->texture.h + (disp->texture.h - rect.bottom);
    x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    x = x > 0 ? x : 0;
    y = y > 0 ? y : 0;

    MoveWindow(disp->hwnd, x, y, w, h, FALSE);
    if (init_free_for_ddraw_gdi(disp, 1) == -1) goto done;
    if (!(disp->flags & FLAG_NOSHOW)) {
        ShowWindow(disp->hwnd, SW_SHOW);
        UpdateWindow(disp->hwnd);
    }

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage (&msg);
    }

done:
    init_free_for_ddraw_gdi(disp, 0);
    if (disp->hwnd) { DestroyWindow(disp->hwnd); disp->hwnd = NULL; }
    disp->flags |= FLAG_CLOSED;
    return NULL;
}

void* display_init(int w, int h, char *params, PFN_DISP_MSG_CB callback, void *cbctx)
{
    DISP *disp = calloc(1, sizeof(DISP));
    if (!disp) return NULL;

    if (params) {
        if      (strcmp(params, "fullscreen") == 0) disp->flags |= FLAG_DDRAW;
        else if (strcmp(params, "inithidden") == 0) disp->flags |= FLAG_NOSHOW;
    }
    disp->callback       = callback;
    disp->cbctx          = cbctx;
    disp->texture.w      = w;
    disp->texture.h      = h;
    disp->texture.lock   = disp_texture_lock;
    disp->texture.unlock = disp_texture_unlock;
    pthread_create(&disp->hthread, NULL, disp_thread_proc, disp);
    while (!(disp->flags & (FLAG_INITED | FLAG_CLOSED))) usleep(100 * 1000);
    if (disp->flags & FLAG_CLOSED) {
        display_free(disp, 0);
        disp = NULL;
    }
    return disp;
}

void display_free(void *ctx, int close)
{
    DISP *disp = (DISP*)ctx;
    if (!disp) return;
    if (close && disp->hwnd) PostMessage(disp->hwnd, WM_CLOSE, 0, 0);
    if (disp->hthread) pthread_join(disp->hthread, NULL);
    free(disp);
}

void display_set(void *ctx, char *name, void *data)
{
    DISP *disp = (DISP*)ctx;
    if (!ctx || !name) return;
    if (strcmp(name, "show") == 0) {
        ShowWindow(disp->hwnd, SW_SHOW);
        UpdateWindow(disp->hwnd);
    }
}

void* display_get(void *ctx, char *name)
{
    DISP *disp = (DISP*)ctx;
    if (!ctx || !name) return NULL;
    if (strcmp(name, "texture") == 0) return &disp->texture;
    if (strcmp(name, "state"  ) == 0) return (disp->flags & FLAG_CLOSED) ? "closed" : "running";
    return NULL;
}

