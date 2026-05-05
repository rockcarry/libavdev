#include <stdlib.h>
#include <string.h>
#include "idev.h"

static long defcb(void *cbctx, int type, void *buf, int len) { return 0; }

void* idev_init(void *params, PFN_IDEV_CB callback, void *cbctx)
{
    IDEV *dev = calloc(1, sizeof(IDEV));
    if (dev) {
        dev->callback = callback ? callback : defcb;
        dev->cbctx    = cbctx;
    }
    return dev;
}

void idev_exit(void *ctx) { free(ctx); }

long idev_set(void *ctx, char *name, void *data) { return 0; }
long idev_get(void *ctx, char *name, void *data) { return 0; }

int idev_getkey(void *ctx, int key)
{
    if (!ctx) return 0;
    IDEV *dev = ctx;
    return !!(dev->key_bits[((uint8_t)key) / 32] & (1 << (((uint8_t)key) % 32)));
}

void idev_getmouse(void *ctx, int *x, int *y, int *btns)
{
    if (!ctx) return;
    IDEV *dev = ctx;
    if (x   ) *x    = dev->mouse_x;
    if (y   ) *y    = dev->mouse_y;
    if (btns) *btns = dev->mouse_btns;
}
