#include <windows.h>
#include <d3d9.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include "idev.h"
#include "vdev.h"

typedef struct {
    #define MOD_THREAD_FLAG_STOP      (1 << 0)
    #define MOD_THREAD_FLAG_PAUSE_REQ (1 << 1)
    #define MOD_THREAD_FLAG_PAUSE_OK  (1 << 2)
    uint32_t    flags;
    void     *hthread;
    void*      (*proc)(void *arg);
    void         *arg;
} MOD_THREAD;

static int module_thread_get_state(MOD_THREAD *mt)
{
    if (!mt->hthread) return 0;
    else if ((mt->flags & (MOD_THREAD_FLAG_PAUSE_REQ|MOD_THREAD_FLAG_PAUSE_OK)) == (MOD_THREAD_FLAG_PAUSE_REQ|MOD_THREAD_FLAG_PAUSE_OK)) return 2;
    else return 1;
}

static void module_thread_set_state(MOD_THREAD *mt, int state)
{
    switch (state) {
    case 0: // stop
        if (mt->hthread) {
            mt->flags |= MOD_THREAD_FLAG_STOP;
            pthread_join((pthread_t)mt->hthread, NULL);
            mt->hthread = NULL;
        }
        break;
    case 1: // start
        mt->flags &= ~(MOD_THREAD_FLAG_STOP|MOD_THREAD_FLAG_PAUSE_REQ|MOD_THREAD_FLAG_PAUSE_OK);
        if (!mt->hthread) pthread_create((pthread_t*)&mt->hthread, NULL, mt->proc, mt->arg);
        break;
    case 2: // pause async
    case 3: // pause sync
        mt->flags = (mt->flags & ~MOD_THREAD_FLAG_PAUSE_OK) | MOD_THREAD_FLAG_PAUSE_REQ;
        if (state == 3) { while (mt->hthread && !(mt->flags & MOD_THREAD_FLAG_PAUSE_OK)) usleep(100 * 1000); }
        break;
    case 4: // restart
        module_thread_set_state(mt, 0);
        module_thread_set_state(mt, 1);
        break;
    }
}

static void module_thread_dump(MOD_THREAD *mt, char *str, int len)
{
    snprintf(str, len, "state: %d, flags: 0x%X, hthread: %p, proc: %p, arg: %p", module_thread_get_state(mt), mt->flags, mt->hthread, mt->proc, mt->arg);
}

typedef LPDIRECT3D9 (WINAPI *PFNDirect3DCreate9)(UINT);

enum {
    SURFACE_PIXFMT_NONE    =-1,
    SURFACE_PIXFMT_ARGB    = 0,
    SURFACE_PIXFMT_YUYV    = 1,
    SURFACE_PIXFMT_UYVY    = 2,
    SURFACE_PIXFMT_NV12    = 3,
    SURFACE_PIXFMT_NV21    = 4,
    SURFACE_PIXFMT_YUV420P = 5,
};

typedef struct {
    int pixfmt; // see above enum
    int parent; // parent surface id, -1 means root surface
    float w, h; // w, h: 0.0 ~ 1.0, pw = parent.pw * w, ph = parent.ph * h
                // w, h, (0, 0) means surface disabled
                // w <= 20 && h <= 20, means keep w / h ratio and fill parent surface
                // w > 20 && h > 20, means surface width and height in pixel
    float x, y; // x, y: 0.0 ~ 1.0, means surface position in parent surface from left top
                //       dx = parent.dx + parent.dw * x, dy = parent.dy + parent.dh * y
                // x, y: -1.0 ~ -0.0, means surface position in parent surface from right bottom
                //       dx = parent.dx + parent.dw * (1.0 + x), dy = parent.dy + parent.dh * (1.0 + y)
                // x, y: > 1.0, mean surface position in parent surface in pixel
                // x, y: -65536, means h/v center in parent surface
    int   letterbox; // if w > 20 && h > 20, 1: letterbox, 0: stretch
    int   rotate;    // rotate angle, 0 - 360
    int   pw, ph;    // surface width & height in pixel
    int   dx, dy, dw, dh; // surface bitblt to parent dest rect in pixel
    LPDIRECT3DSURFACE9 surface; // direct3d offscreen surface in system memory
    LPDIRECT3DTEXTURE9 texture; // direct3d texture
    LPDIRECT3DVERTEXBUFFER9 vb; // direct3d vertex buffer
    BMP                 bitmap; // bitmap for surface
} SURFACE;

typedef struct {
    float    x, y, z;
    float    rhw;
    float    tu, tv;
} CUSTOMVERTEX;
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX1)

typedef struct {
    HWND hwnd;
    int  window_width, window_height;
    char title[64];

    #define FLAG_FULLSCREEN (1 << 0)
    #define FLAG_INITED     (1 << 1)
    #define FLAG_CLOSED     (1 << 2)
    #define FLAG_NOSHOW     (1 << 3)
    #define FLAG_RESIZE     (1 << 4)
    #define FLAG_WMSIZE     (1 << 5)
    #define FLAG_REINITSURF (1 << 6)
    #define FLAG_DEVLOST    (1 << 7)
    uint32_t flags;
    pthread_mutex_t lock;

    MOD_THREAD  mthread;
    PFN_VDEV_CB callback;
    void       *cbctx;
    IDEV        idev;

    HMODULE               hDll;
    PFNDirect3DCreate9    pfnDirect3DCreate9;
    LPDIRECT3D9           pD3D9;
    LPDIRECT3DDEVICE9     pD3DDev;
    LPDIRECT3DSURFACE9    bkbuf;
    D3DPRESENT_PARAMETERS d3dpp;

    int surface_count;
    SURFACE surfaces[0];
} VDEV;

#define VDEV_WND_CLASS TEXT("VDevWndClass")
#define VDEV_WND_NAME  TEXT("VDevWindow"  )

static int init_free_for_d3d(VDEV *vdev, int init) // init: 1: init, 0: free, -1: reset
{
    if (init == 1) {
        if (!vdev->pD3D9) vdev->pD3D9 = vdev->pfnDirect3DCreate9(D3D_SDK_VERSION);
        if (!vdev->pD3D9) return -1;
        vdev->d3dpp.BackBufferWidth        = GetSystemMetrics(SM_CXSCREEN);
        vdev->d3dpp.BackBufferHeight       = GetSystemMetrics(SM_CYSCREEN);
        vdev->d3dpp.BackBufferFormat       = D3DFMT_A8R8G8B8;
        vdev->d3dpp.BackBufferCount        = 1;
        vdev->d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
        vdev->d3dpp.MultiSampleQuality     = 0;
        vdev->d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
        vdev->d3dpp.hDeviceWindow          = vdev->hwnd;
        vdev->d3dpp.Windowed               = !(vdev->flags & FLAG_FULLSCREEN);
        vdev->d3dpp.EnableAutoDepthStencil = FALSE;
        IDirect3D9_CreateDevice(vdev->pD3D9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, vdev->hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &vdev->d3dpp, &vdev->pD3DDev);
        if (!vdev->pD3DDev) return -1;
        IDirect3DDevice9_GetBackBuffer(vdev->pD3DDev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &vdev->bkbuf);
        if (!vdev->bkbuf  ) return -1;
        vdev->flags |= FLAG_INITED|FLAG_REINITSURF;
    } else if (init == 0 || init == -1) {
        for (int i = 0; i < vdev->surface_count; i++) {
            SURFACE *s = &vdev->surfaces[i];
            if (s->surface) { IDirect3DSurface9_Release(s->surface); s->surface = NULL; }
            if (s->texture) { IDirect3DTexture9_Release(s->texture); s->texture = NULL; }
            if (s->vb     ) { IDirect3DVertexBuffer9_Release(s->vb); s->vb      = NULL; }
        }
        if (vdev->bkbuf) { IDirect3DSurface9_Release(vdev->bkbuf); vdev->bkbuf = NULL; }

        if (init == 0) {
            if (vdev->pD3DDev) { IDirect3DDevice9_Release(vdev->pD3DDev); vdev->pD3DDev = NULL; }
            if (vdev->pD3D9  ) { IDirect3D9_Release      (vdev->pD3D9  ); vdev->pD3D9   = NULL; }
            vdev->flags &= ~(FLAG_INITED|FLAG_REINITSURF);
            vdev->flags |=  FLAG_CLOSED;
        } else {
            // reset device
            vdev->d3dpp.BackBufferWidth  = GetSystemMetrics(SM_CXSCREEN);
            vdev->d3dpp.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
            HRESULT hr = IDirect3DDevice9_Reset(vdev->pD3DDev, &vdev->d3dpp);
            if (FAILED(hr)) return -1;
            vdev->flags |= FLAG_REINITSURF;
        }
    }
    return 0;
}

static void setup_vertex_buffer(LPDIRECT3DVERTEXBUFFER9 vb, int dstx, int dsty, int dstw, int dsth, int srcw, int srch, int angle)
{
    CUSTOMVERTEX *pv = NULL;
    if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(vb, 0, 4 * sizeof(CUSTOMVERTEX), (void**)&pv, 0))) {
        pv[0].rhw = pv[1].rhw = pv[2].rhw = pv[3].rhw = 1.0f;
        pv[0].z   = pv[1].z   = pv[2].z   = pv[3].z   = 0.0f;
        pv[0].tu  = 0.0f; pv[0].tv  = 0.0f;
        pv[1].tu  = 1.0f; pv[1].tv  = 0.0f;
        pv[2].tu  = 1.0f; pv[2].tv  = 1.0f;
        pv[3].tu  = 0.0f; pv[3].tv  = 1.0f;
        if (angle) {
            float radian   = (float)(angle % 360) * M_PI / 180.0f;
            float cos_a    = cosf(radian);
            float sin_a    = sinf(radian);
            float abs_cos  = fabsf(cos_a);
            float abs_sin  = fabsf(sin_a);
            // bounding box of a w*h rect rotated by angle: bw = w*|cos|+h*|sin|, bh = w*|sin|+h*|cos|
            float bound_w  = dstw * abs_cos + dsth * abs_sin;
            float bound_h  = dstw * abs_sin + dsth * abs_cos;
            float scale    = fminf((float)dstw / bound_w, (float)dsth / bound_h); // shrink to fit target area
            float half_w   = dstw / 2.0f * scale;
            float half_h   = dsth / 2.0f * scale;
            float cx       = dstx + dstw / 2.0f;
            float cy       = dsty + dsth / 2.0f;
            // four corners relative to center (shrinked)
            float pts[4][2] = {
                { -half_w, -half_h },
                {  half_w, -half_h },
                {  half_w,  half_h },
                { -half_w,  half_h },
            };
            for (int i = 0; i < 4; i++) {
                pv[i].x = cos_a * pts[i][0] - sin_a * pts[i][1] + cx;
                pv[i].y = sin_a * pts[i][0] + cos_a * pts[i][1] + cy;
            }
        } else {
            #define OFFSET_XY -0.5f
            pv[0].x = OFFSET_XY + dstx;            pv[0].y = OFFSET_XY + dsty;
            pv[1].x = OFFSET_XY + dstx + dstw - 1; pv[1].y = OFFSET_XY + dsty;
            pv[2].x = OFFSET_XY + dstx + dstw - 1; pv[2].y = OFFSET_XY + dsty + dsth - 1;
            pv[3].x = OFFSET_XY + dstx;            pv[3].y = OFFSET_XY + dsty + dsth - 1;
        }
        IDirect3DVertexBuffer9_Unlock(vb);
    } else {
        printf("<4> setup_vertex_buffer: lock failed !\n");
    }
}

static void vdev_reinit_surfaces(VDEV *vdev)
{
    if (vdev->flags & FLAG_DEVLOST) {
        if (0 != init_free_for_d3d(vdev, -1)) { printf("<4> reset d3d failed !\n"); return; }
        vdev->flags &= ~FLAG_DEVLOST;
    }
    if (vdev->flags & FLAG_REINITSURF) {
        printf("<6> vdev_reinit_surfaces: %d\n", vdev->surface_count);
        for (int i = 0; i < vdev->surface_count; i++) {
            SURFACE *s = &vdev->surfaces[i];
            int parent_root = s->parent < 0 || s->parent >= vdev->surface_count;
            int parent_pw   = !parent_root ? vdev->surfaces[s->parent].pw : vdev->window_width;
            int parent_ph   = !parent_root ? vdev->surfaces[s->parent].ph : vdev->window_height;
            int parent_dx   = !parent_root ? vdev->surfaces[s->parent].dx : 0;
            int parent_dy   = !parent_root ? vdev->surfaces[s->parent].dy : 0;
            int parent_dw   = !parent_root ? vdev->surfaces[s->parent].dw : parent_pw;
            int parent_dh   = !parent_root ? vdev->surfaces[s->parent].dh : parent_ph;
            int surface_pw, surface_ph;
            if ((s->w > 20 && s->h > 20) || (s->w > 1.0 && s->w <= 20 && s->h > 1.0 && s->h <= 20)) {
                if (s->w > 20 && s->h > 20) {
                    surface_pw = s->w;
                    surface_ph = s->h;
                } else {
                    if (s->w * parent_ph > s->h * parent_pw) {
                        surface_pw = parent_pw;
                        surface_ph = surface_pw * s->h / s->w;
                    } else {
                        surface_ph = parent_ph;
                        surface_pw = surface_ph * s->w / s->h;
                    }
                }
                if (s->letterbox) {
                    if (surface_pw * parent_ph > surface_ph * parent_pw) {
                        s->dw = parent_dw;
                        s->dh = s->dw * surface_ph / surface_pw;
                    } else {
                        s->dh = parent_dh;
                        s->dw = s->dh * surface_pw / surface_ph;
                    }
                } else {
                    s->dw = parent_dw;
                    s->dh = parent_dh;
                }
            } else {
                surface_pw = (s->w >= 0 && s->w <= 1.0) ? parent_pw * s->w : s->w;
                surface_ph = (s->h >= 0 && s->h <= 1.0) ? parent_ph * s->h : s->h;
                s->dw      = (s->w >= 0 && s->w <= 1.0) ? parent_dw * s->w : s->w;
                s->dh      = (s->h >= 0 && s->h <= 1.0) ? parent_dh * s->h : s->h;
            }

            if (s->x == -65536) s->dx = parent_dx + (parent_dw - s->dw) / 2;
            else if (s->x >=  0.0 && s->x <= 1.0) s->dx = parent_dx + parent_dw * s->x / surface_pw;
            else if (s->x >= -1.0 && s->x <= 0.0) s->dx = parent_dx + parent_dw * (1.0 + s->x / surface_pw) - s->dw;
            else s->dx = parent_dx + s->x;
            if (s->y == -65536) s->dy = parent_dy + (parent_dh - s->dh) / 2;
            else if (s->y >=  0.0 && s->y <= 1.0) s->dy = parent_dy + parent_dh * s->y / surface_ph;
            else if (s->y >= -1.0 && s->y <= 0.0) s->dy = parent_dy + parent_dh * (1.0 + s->y / surface_ph) - s->dh;
            else s->dy = parent_dy + s->y;

            if (!s->surface || surface_pw != s->pw || surface_ph != s->ph) {
                s->pw = surface_pw;
                s->ph = surface_ph;

                // release old direct3d offscreen surface
                if (s->surface) { IDirect3DSurface9_Release(s->surface); s->surface = NULL; }
                if (s->texture) { IDirect3DTexture9_Release(s->texture); s->texture = NULL; }

                // create new direct3d offscreen surface
                D3DFORMAT fmt = D3DFMT_A8R8G8B8; // default argb
                switch (s->pixfmt) {
                case SURFACE_PIXFMT_ARGB   : fmt = D3DFMT_A8R8G8B8; break;
                case SURFACE_PIXFMT_YUYV   : fmt = D3DFMT_YUY2;     break;
                case SURFACE_PIXFMT_UYVY   : fmt = D3DFMT_UYVY;     break;
                case SURFACE_PIXFMT_NV12   :
                case SURFACE_PIXFMT_NV21   : fmt = D3DFMT_L16;      break; // nv12/nv21: use L16 as container
                case SURFACE_PIXFMT_YUV420P: fmt = D3DFMT_L8;       break;
                }
                if (s->pixfmt >= 0 && s->pw > 0 && s->ph > 0 && vdev->pD3DDev) {
                    IDirect3DDevice9_CreateOffscreenPlainSurface(vdev->pD3DDev, s->pw, s->ph, fmt, D3DPOOL_SYSTEMMEM, &s->surface, NULL);
                    if (!s->surface) printf("<4> vdev_reinit_surfaces, create surface failed: %d, %d\n", s->pw, s->ph);
                    IDirect3DDevice9_CreateTexture(vdev->pD3DDev, s->pw, s->ph, 1, 0, fmt, D3DPOOL_DEFAULT, &s->texture, NULL);
                    if (!s->texture) printf("<4> vdev_reinit_surfaces, create texture failed: %d, %d\n", s->pw, s->ph);
                    if (!s->vb) IDirect3DDevice9_CreateVertexBuffer(vdev->pD3DDev, sizeof(CUSTOMVERTEX) * 4, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &s->vb, NULL);
                    if (!s->vb) printf("<4> vdev_reinit_surfaces, create vb failed !\n");
                } else {
                    printf("<4> vdev_reinit_surfaces, invaid surface size: %d, %d, vdev->pD3DDev: %p\n", s->pw, s->ph, vdev->pD3DDev);
                }
            }
            if (s->vb) setup_vertex_buffer(s->vb, s->dx, s->dy, s->dw, s->dh, s->pw, s->ph, s->rotate);
        }
        vdev->flags &= ~FLAG_REINITSURF;
    }
}

static LRESULT CALLBACK VDEV_WNDPROC(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
    VDEV *vdev = (VDEV*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
#else
    VDEV *vdev = (VDEV*)GetWindowLong(hwnd, GWL_USERDATA);
#endif
    IDEV *idev = &vdev->idev;
    int  ret = -1;
    switch (uMsg) {
    case WM_KEYUP: case WM_KEYDOWN: case WM_SYSKEYUP: case WM_SYSKEYDOWN:
        ret = idev->callback(idev->cbctx, IDEV_CALLBACK_KEY_EVENT, (void*)(uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN), wParam);
        if (ret != 0 && uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) PostQuitMessage(0);
        int idx = ((BYTE)wParam) / 32;
        int bit = ((BYTE)wParam) % 32;
        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) idev->key_bits[idx] |= (1 << bit);
        else idev->key_bits[idx] &= ~(1 << bit);
        return 0;
    case WM_MOUSEMOVE:
        idev->last_mouse_x = idev->curr_mouse_x;
        idev->last_mouse_y = idev->curr_mouse_y;
        idev->curr_mouse_x = (int32_t)((lParam >> 0) & 0xFFFF);
        idev->curr_mouse_y = (int32_t)((lParam >>16) & 0xFFFF);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_MOVE, idev, sizeof(IDEV));
        return 0;
    case WM_MOUSEWHEEL:
        return 0;
    case WM_LBUTTONUP:
        idev->last_mouse_btns  = idev->curr_mouse_btns;
        idev->curr_mouse_btns &= ~(1 << 0);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_LBTNUP, idev, sizeof(IDEV));
        return 0;
    case WM_LBUTTONDOWN:
        idev->last_mouse_btns  = idev->curr_mouse_btns;
        idev->curr_mouse_btns |=  (1 << 0);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_LBTNDOWN, idev, sizeof(IDEV));
        return 0;
    case WM_MBUTTONUP:
        idev->last_mouse_btns  = idev->curr_mouse_btns;
        idev->curr_mouse_btns &= ~(1 << 1);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_MBTNUP, idev, sizeof(IDEV));
        return 0;
    case WM_MBUTTONDOWN:
        idev->last_mouse_btns  = idev->curr_mouse_btns;
        idev->curr_mouse_btns |=  (1 << 1);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_MBTNDOWN, idev, sizeof(IDEV));
        return 0;
    case WM_RBUTTONUP:
        idev->last_mouse_btns  = idev->curr_mouse_btns;
        idev->curr_mouse_btns &= ~(1 << 2);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_RBTNUP, idev, sizeof(IDEV));
        return 0;
    case WM_RBUTTONDOWN:
        idev->last_mouse_btns  = idev->curr_mouse_btns;
        idev->curr_mouse_btns |=  (1 << 2);
        idev->callback(idev->cbctx, IDEV_CALLBACK_MOUSE_RBTNDOWN, idev, sizeof(IDEV));
        return 0;
    case WM_SIZE:
        if (lParam == 0) break;
        vdev->window_width  = LOWORD(lParam);
        vdev->window_height = HIWORD(lParam);
        vdev->flags |= FLAG_REINITSURF;
        if (vdev->flags & FLAG_INITED) {
            pthread_mutex_lock(&vdev->lock);
            vdev_reinit_surfaces(vdev);
            pthread_mutex_unlock(&vdev->lock);
            if (0 != vdev->callback(vdev->cbctx, VDEV_CALLBACK_VDEV_RESIZE, NULL, 0)) vdev_render(vdev);
        }
        break;
    case WM_PAINT:
        if (0 != vdev->callback(vdev->cbctx, VDEV_CALLBACK_VDEV_PAINT, NULL, 0)) vdev_render(vdev);
        break;
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
    int      winflags = 0, x, y, w, h, ret;

    wc.lpfnWndProc   = VDEV_WNDPROC;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon  (NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = VDEV_WND_CLASS;
    if (!RegisterClass(&wc)) { printf("<4> vdev RegisterClass failed !\n"); return NULL; }

    pthread_mutex_lock(&vdev->lock);
    vdev->flags &= ~(FLAG_INITED|FLAG_CLOSED);
    pthread_mutex_unlock(&vdev->lock);
    if (vdev->flags & FLAG_FULLSCREEN) {
        winflags = WS_POPUP;
    } else {
        winflags = WS_SYSMENU|WS_MINIMIZEBOX;
        if (vdev->flags & FLAG_RESIZE) winflags |= WS_MAXIMIZEBOX|WS_SIZEBOX;
    }
    vdev->hwnd = CreateWindow(VDEV_WND_CLASS, vdev->title, winflags, CW_USEDEFAULT, CW_USEDEFAULT,
        vdev->window_width, vdev->window_height, NULL, NULL, wc.hInstance, NULL);
    if (!vdev->hwnd) { printf("<4> vdev CreateWindow failed !\n"); goto done; }

#ifdef _WIN64
    SetWindowLongPtr(vdev->hwnd, GWLP_USERDATA, (LONG_PTR)vdev);
#else
    SetWindowLong(vdev->hwnd, GWL_USERDATA, (LONG)vdev);
#endif

    GetClientRect(vdev->hwnd, &rect);
    w = vdev->window_width  + (vdev->window_width  - rect.right );
    h = vdev->window_height + (vdev->window_height - rect.bottom);
    x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    x = x > 0 ? x : 0;
    y = y > 0 ? y : 0;

    MoveWindow(vdev->hwnd, x, y, w, h, FALSE);
    pthread_mutex_lock(&vdev->lock);
    ret = init_free_for_d3d(vdev, 1);
    pthread_mutex_unlock(&vdev->lock);
    if (ret == -1) goto done;
    if (!(vdev->flags & FLAG_NOSHOW)) {
        ShowWindow(vdev->hwnd, SW_SHOW);
        UpdateWindow(vdev->hwnd);
    }

    vdev->callback(vdev->cbctx, VDEV_CALLBACK_VDEV_INITED, NULL, 0);
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage (&msg);
    }
    vdev->callback(vdev->cbctx, VDEV_CALLBACK_VDEV_CLOSED, NULL, 0);

done:
    pthread_mutex_lock(&vdev->lock);
    init_free_for_d3d(vdev, 0);
    pthread_mutex_unlock(&vdev->lock);
    if (vdev->hwnd) { DestroyWindow(vdev->hwnd); vdev->hwnd = NULL; }
    UnregisterClass(VDEV_WND_CLASS, wc.hInstance);
    return NULL;
}

static long defcb(void *cbctx, int type, void *buf, int len) { return -1; }

void* vdev_init(void *params, PFN_VDEV_CB callback, void *cbctx)
{
    int surfaces_count = 0;
    if (!params) return NULL;
    char *str = strstr(params, "surfaces:"); if (str) sscanf(str, "surfaces:%d", &surfaces_count);
    surfaces_count = surfaces_count ? surfaces_count : 2;
    VDEV *vdev = calloc(1, sizeof(VDEV) + surfaces_count * sizeof(SURFACE));
    if (!vdev) return NULL;
    vdev->surface_count = surfaces_count;

    if (strstr(params, "fullscreen")) vdev->flags |= FLAG_FULLSCREEN;
    if (strstr(params, "inithidden")) vdev->flags |= FLAG_NOSHOW;
    if (strstr(params, "resize"    )) vdev->flags |= FLAG_RESIZE;

    str = strstr(params, "title:");
    if (str) sscanf(str, "title:%63[^,]", vdev->title);
    else strncpy(vdev->title, VDEV_WND_NAME, sizeof(vdev->title) - 1);

    str = strstr(params, "w:"); if (str) sscanf(str, "w:%d", &vdev->window_width );
    str = strstr(params, "h:"); if (str) sscanf(str, "h:%d", &vdev->window_height);
    vdev->window_width  = vdev->window_width  ? vdev->window_width  : 640;
    vdev->window_height = vdev->window_height ? vdev->window_height : 480;

    pthread_mutex_init(&vdev->lock, NULL);
    vdev->callback           = callback ? callback : defcb;
    vdev->cbctx              = cbctx;
    vdev->idev.callback      = vdev->callback;
    vdev->idev.cbctx         = vdev->cbctx;
    vdev->mthread.proc       = vdev_thread_proc;
    vdev->mthread.arg        = vdev;
    vdev->hDll               = LoadLibrary(TEXT("d3d9.dll"));
    vdev->pfnDirect3DCreate9 = (PFNDirect3DCreate9)GetProcAddress(vdev->hDll, "Direct3DCreate9");

    char tmp[64]; snprintf(tmp, sizeof(tmp), "parent:-1,letterbox:1,sw:%d,sh:%d,sx:center,sy:center", vdev->window_width, vdev->window_height);
    vdev_set(vdev, VDEV_KEY_SURFACE_PARAMS, tmp);

    if (strstr(params, "show")) {
        vdev_set(vdev, VDEV_KEY_STATE, (void*)1);
        for (int i = 0; i < 10 && !(vdev->flags & FLAG_INITED); i++) usleep(100 * 1000);
    }
    return vdev;
}

void vdev_exit(void *ctx)
{
    if (!ctx) return;
    VDEV *vdev = ctx;
    if (vdev->hwnd) PostMessage(vdev->hwnd, WM_CLOSE, 0, 0);
    module_thread_set_state(&vdev->mthread, 0);
    if (vdev->hDll) FreeLibrary(vdev->hDll);
    pthread_mutex_destroy(&vdev->lock);
    free(vdev);
}

BMP* vdev_lock(void *ctx, int idx)
{
    VDEV *vdev = ctx;
    if (!vdev || !vdev->pD3DDev || idx < 0 || idx >= vdev->surface_count) return NULL;

    pthread_mutex_lock(&vdev->lock);
    vdev_reinit_surfaces(vdev);

    SURFACE *surf = &vdev->surfaces[idx];
    D3DLOCKED_RECT lr;
    if (!surf->surface || FAILED(IDirect3DSurface9_LockRect(surf->surface, &lr, NULL, 0))) {
        printf("<4> vdev IDirect3DSurface9_LockRect failed, surf->surface: %p !\n", surf->surface); goto fail;
    }

    surf->bitmap.width  = surf->pw;
    surf->bitmap.height = surf->ph;
    surf->bitmap.stride = lr.Pitch;
    surf->bitmap.pdata  = lr.pBits;
    surf->bitmap.cdepth = surf->pixfmt == SURFACE_PIXFMT_ARGB ? 32 : surf->pixfmt;
    return &surf->bitmap;

fail:
    pthread_mutex_unlock(&vdev->lock);
    return NULL;
}

void vdev_unlock(void *ctx, int idx)
{
    VDEV *vdev = ctx;
    if (!vdev || !vdev->pD3DDev || idx < 0 || idx >= vdev->surface_count) return;
    if (vdev->surfaces[idx].surface) IDirect3DSurface9_UnlockRect(vdev->surfaces[idx].surface);
    pthread_mutex_unlock(&vdev->lock);
}

static void copy_surface_to_texture(VDEV *vdev, LPDIRECT3DSURFACE9 surface, LPDIRECT3DTEXTURE9 texture)
{
    if (!surface || !texture) { /* printf("<4> vdev surface/texture is NULL !\n"); */ return; }
    LPDIRECT3DSURFACE9 textSurf = NULL;
    IDirect3DTexture9_GetSurfaceLevel(texture, 0, &textSurf);
    if (textSurf) {
        HRESULT hr = IDirect3DDevice9_UpdateSurface(vdev->pD3DDev, surface, NULL, textSurf, NULL);
        if (FAILED(hr)) printf("<4> vdev IDirect3DDevice9_UpdateSurface failed !\n");
        IDirect3DSurface9_Release(textSurf);
    } else { printf("<4> vdev IDirect3DTexture9_GetSurfaceLevel failed !\n"); }
}

void vdev_render(void *ctx)
{
    VDEV *vdev = ctx;
    if (!vdev || !vdev->pD3DDev || !vdev->bkbuf || !(vdev->flags & FLAG_INITED)) return;

    pthread_mutex_lock(&vdev->lock);

    for (int i = 0; i < vdev->surface_count; i++) {
        SURFACE *surf = &vdev->surfaces[i];
        copy_surface_to_texture(vdev, surf->surface, surf->texture);
    }

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(vdev->pD3DDev))) {
        IDirect3DDevice9_Clear(vdev->pD3DDev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
        for (int i = 0; i < vdev->surface_count; i++) {
            SURFACE *surf = &vdev->surfaces[i];
            if (!surf->vb || !surf->texture) continue;

            if (i == vdev->surface_count - 1) {
                IDirect3DDevice9_SetRenderState(vdev->pD3DDev, D3DRS_ALPHABLENDENABLE, TRUE);
                IDirect3DDevice9_SetRenderState(vdev->pD3DDev, D3DRS_SRCBLEND , D3DBLEND_SRCALPHA   );
                IDirect3DDevice9_SetRenderState(vdev->pD3DDev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                IDirect3DDevice9_SetTextureStageState(vdev->pD3DDev, 0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1);
                IDirect3DDevice9_SetTextureStageState(vdev->pD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE    );
            }

            IDirect3DDevice9_SetSamplerState(vdev->pD3DDev, 0, D3DSAMP_MINFILTER, surf->rotate ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            IDirect3DDevice9_SetSamplerState(vdev->pD3DDev, 0, D3DSAMP_MAGFILTER, surf->rotate ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            IDirect3DDevice9_SetSamplerState(vdev->pD3DDev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

            IDirect3DDevice9_SetStreamSource(vdev->pD3DDev, 0, surf->vb, 0, sizeof(CUSTOMVERTEX));
            IDirect3DDevice9_SetFVF(vdev->pD3DDev, D3DFVF_CUSTOMVERTEX);
            IDirect3DDevice9_SetTexture(vdev->pD3DDev, 0, (IDirect3DBaseTexture9*)surf->texture);
            IDirect3DDevice9_DrawPrimitive(vdev->pD3DDev, D3DPT_TRIANGLEFAN, 0, 2);

            if (i == vdev->surface_count - 1) IDirect3DDevice9_SetRenderState(vdev->pD3DDev, D3DRS_ALPHABLENDENABLE, FALSE);
        }

        IDirect3DDevice9_EndScene(vdev->pD3DDev);
    } else {
        printf("<4> vdev IDirect3DDevice9_BeginScene failed !\n");
    }

    RECT rect = { 0, 0, vdev->window_width, vdev->window_height };
    HRESULT hr = IDirect3DDevice9_Present(vdev->pD3DDev, &rect, &rect, NULL, NULL);
    if (hr == D3DERR_DEVICELOST) vdev->flags |= FLAG_DEVLOST;

    pthread_mutex_unlock(&vdev->lock);
}

long vdev_set(void *ctx, char *key, void *val)
{
    if (!ctx || !key) return -1;
    VDEV *vdev = (VDEV*)ctx;
    if (strcmp(key, VDEV_KEY_STATE) == 0) {
        if (vdev->hwnd && ((intptr_t)val == 0 || (intptr_t)val == 4)) PostMessage(vdev->hwnd, WM_CLOSE, 0, 0);
        module_thread_set_state(&vdev->mthread, (intptr_t)val);
    } else if (strncmp(key, VDEV_KEY_SURFACE_PARAMS, sizeof(VDEV_KEY_SURFACE_PARAMS) - 2) == 0 && val) {
        int idx = atoi(key + sizeof(VDEV_KEY_SURFACE_PARAMS) - 2);
        if (idx < 0 || idx >= vdev->surface_count) return -1;

        SURFACE *surf = &vdev->surfaces[idx];
        char    *str  = NULL;
        if (strstr(val, "pixfmt:")) {
            if (strstr(val, "none"   )) surf->pixfmt = SURFACE_PIXFMT_NONE;
            if (strstr(val, "argb"   )) surf->pixfmt = SURFACE_PIXFMT_ARGB;
            if (strstr(val, "yuyv"   )) surf->pixfmt = SURFACE_PIXFMT_YUYV;
            if (strstr(val, "uyvy"   )) surf->pixfmt = SURFACE_PIXFMT_UYVY;
            if (strstr(val, "nv12"   )) surf->pixfmt = SURFACE_PIXFMT_NV12;
            if (strstr(val, "nv21"   )) surf->pixfmt = SURFACE_PIXFMT_NV21;
            if (strstr(val, "yuv420p")) surf->pixfmt = SURFACE_PIXFMT_YUV420P;
        }
        str = strstr(val, "sw:"); if (str) sscanf(str, "sw:%f", &surf->w);
        str = strstr(val, "sh:"); if (str) sscanf(str, "sh:%f", &surf->h);
        str = strstr(val, "sx:");
        if (str) {
            if (strstr(str, "sx:center")) surf->x = -65536;
            else sscanf(str, "sx:%f", &surf->x);
        }
        str = strstr(val, "sy:");
        if (str) {
            if (strstr(str, "sy:center")) surf->y = -65536;
            else sscanf(str, "sy:%f", &surf->y);
        }
        str = strstr(val, "parent:"   ); if (str) sscanf(str, "parent:%d"   , &surf->parent   );
        str = strstr(val, "letterbox:"); if (str) sscanf(str, "letterbox:%d", &surf->letterbox);
        str = strstr(val, "rotate:"   ); if (str) sscanf(str, "rotate:%d"   , &surf->rotate   );
        vdev->flags |= FLAG_REINITSURF;
        vdev->callback(vdev->cbctx, VDEV_CALLBACK_VDEV_RESIZE, NULL, 0);
    } else return -1;
    return 0;
}

long vdev_get(void *ctx, char *key, void *val)
{
    if (!ctx || !key) return 0;
    VDEV *vdev = (VDEV*)ctx;
    if (strcmp(key, VDEV_KEY_STATE ) == 0) return (vdev->flags & FLAG_CLOSED) ? VDEV_CALLBACK_VDEV_CLOSED : (long)module_thread_get_state(&vdev->mthread);
    if (strcmp(key, VDEV_KEY_WIDTH ) == 0) return vdev->window_width;
    if (strcmp(key, VDEV_KEY_HEIGHT) == 0) return vdev->window_height;
    if (strcmp(key, VDEV_KEY_IDEV  ) == 0) return (long)&vdev->idev;
    if (strcmp(key, VDEV_KEY_HWND  ) == 0) return (long) vdev->hwnd;
    return 0;
}

void vdev_dump(void *ctx, char *str, int len, int page)
{
    if (!ctx) return;
    VDEV *vdev = (VDEV*)ctx;
    char  buf[256];
    if (page == 0) {
        module_thread_dump(&vdev->mthread, buf, sizeof(buf));
        snprintf(str, len,
            "self: %p, flags: 0x%x\n"
            "hwnd: %p, title: %s, window_width: %d, window_height: %d\n"
            "callback: %p, cbctx: %p\n"
            "hDll: %p, pfnDirect3DCreate9: %p\n"
            "pD3D9: %p, pD3DDev: %p, bkbuf: %p\n"
            "mod_thread: %s\n",
            vdev, vdev->flags,
            vdev->hwnd, vdev->title, vdev->window_width, vdev->window_height,
            vdev->callback, vdev->cbctx,
            vdev->hDll, vdev->pfnDirect3DCreate9,
            vdev->pD3D9, vdev->pD3DDev, vdev->bkbuf,
            buf);
    } else if (page == 1) {
        for (int i = 0; i < vdev->surface_count; i++) {
            SURFACE *surf = &vdev->surfaces[i];
            int ret = snprintf(str, len,
                "surface[%d]: %p\n"
                "  pixfmt: %d, parent: %d\n"
                "  w: %.2f, h: %.2f, x: %.2f, y: %.2f\n"
                "  letterbox: %d\n"
                "  rotate: %d\n"
                "  pw: %d, ph: %d\n"
                "  dx: %d, dy: %d, dw: %d, dh: %d\n"
                "  surface: %p\n"
                "  texture: %p\n"
                "  vb: %p\n",
                i, surf,
                surf->pixfmt, surf->parent,
                surf->w, surf->h, surf->x, surf->y,
                surf->letterbox,
                surf->rotate,
                surf->pw, surf->ph,
                surf->dx, surf->dy, surf->dw, surf->dh,
                surf->surface,
                surf->texture,
                surf->vb);
            if (ret <= 0) break;
            str += ret; len -= ret;
        }
    }
}
