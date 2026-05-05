#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "acap.h"
#include "adev.h"
#include "vdev.h"
#include "idev.h"

#ifdef _TEST_ADEV_
static void gen_sin_wav(int16_t *pcm, int n, int samprate, int freq)
{
    int  i;
    for (i = 0; i < n; i++) {
        pcm[i] = 32760 * sin(i * 2 * M_PI * freq / samprate);
    }
}

int main(void)
{
    void *adev = adev_init("", NULL, NULL);
    int16_t buf[320];
    gen_sin_wav(buf, 320, 8000, 500);
    while (1) {
        adev_play(adev, buf, sizeof(buf), -1);
    }
    adev_exit(adev);
    return 0;
}
#endif

#ifdef _TEST_RECORD_
static void bmp_setpixel(BMP *pb, int x, int y, int c)
{
    if (x < 0 || x >= pb->width || y < 0 || y >= pb->height) return;
    *(uint32_t*)(pb->pdata + y * pb->stride + x * sizeof(uint32_t)) = c;
}

static void bmp_line(BMP *pb, int x1, int y1, int x2, int y2, int c)
{
    int dx, dy, d, e;
    if (!pb) return;

    dx = abs(x1 - x2);
    dy = abs(y1 - y2);
    if ((dx >= dy && x1 > x2) || (dx < dy && y1 > y2)) {
        d = x1; x1 = x2; x2 = d;
        d = y1; y1 = y2; y2 = d;
    }
    if (dx >= dy) {
        d = y2 - y1 > 0 ? 1 : -1;
        for (e = dx/2; x1 < x2; x1++, e += dy) {
            if (e >= dx) e -= dx, y1 += d;
            bmp_setpixel(pb, x1, y1, c);
        }
    } else {
        d = x2 - x1 > 0 ? 1 : -1;
        for (e = dy/2; y1 < y2; y1++, e += dx) {
            if (e >= dy) e -= dy, x1 += d;
            bmp_setpixel(pb, x1, y1, c);
        }
    }
    bmp_setpixel(pb, x2, y2, c);
}

static long my_acap_callback(void *ctxt, int type, void *buf, int len)
{
    void       *vdev  = ctxt;
    BMP        *bmp   = NULL;
    ACAP_FRAME *frame = buf;
    if (type == ACAP_CALLBACK_DATA && frame && frame->type == ACAP_FRAME_TYPE_PCM) {
#ifdef DEBUG
        printf("record data, ctxt: %p, buf: %p, len: %d\n", ctxt, frame->buf, frame->len); fflush(stdout);
#endif
        if ((bmp = vdev_lock(vdev, 0))) {
            if (frame->buf && frame->len > 0) {
                int lasty, cury, x, i;
                int16_t *pcm = (int16_t*)frame->buf;
                memset(bmp->pdata, 0, bmp->height * bmp->stride);
                lasty = bmp->height / 2 - pcm[0] * bmp->height / 0x10000;
                for (x = 1; x < bmp->width; x++) {
                    i    = (frame->len / sizeof(int16_t) - 1) * x / (bmp->width - 1);
                    cury = bmp->height / 2 - pcm[i] * bmp->height / 0x10000;
                    bmp_line(bmp, x - 1, lasty, x, cury, 0x00FF00);
                    lasty = cury;
                }
            }
            vdev_unlock(vdev, 0);
            vdev_render(vdev);
        }
    }
    return 0;
}

int main(void)
{
    void *vdev = vdev_init("show", NULL, NULL);
    void *acap = acap_init("", my_acap_callback, vdev);

    acap_set(acap, ACAP_KEY_STATE, (void*)1);
    while (vdev_get(vdev, VDEV_KEY_STATE, NULL) != VDEV_CALLBACK_VDEV_CLOSED) usleep(100 * 1000);

    vdev_exit(vdev);
    adev_exit(acap);
    return 0;
}
#endif

#ifdef _TEST_VDEV_
int main(void)
{
    void *vdev = vdev_init("show", NULL, NULL);
    void *idev = (void*)vdev_get(vdev, "idev", NULL);
    BMP  *bmp  = NULL;
    int   i, j;
    if ((bmp = vdev_lock(vdev, 0))) {
        for (i = 0; i < bmp->height; i++) {
            for (j = 0; j < bmp->width; j++) {
                *(uint32_t*)(bmp->pdata + i * bmp->stride + j * sizeof(uint32_t)) = (i << 16) | (j << 8) | (i << 0);
            }
        }
        vdev_unlock(vdev, 0);
        vdev_render(vdev);
    }
    while (1) {
        int state = vdev_get(vdev, VDEV_KEY_STATE, NULL);
        if (state == VDEV_CALLBACK_VDEV_CLOSED) break;
        if (idev_getkey(idev, 'A')) { printf("a pressed !\n"); fflush(stdout); }
        usleep(20 * 1000);
    }
    vdev_exit(vdev);
    return 0;
}
#endif
