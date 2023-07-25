#include <stdlib.h>
#include <string.h>
#include "idev.h"

void* idev_init(char *params, PFN_IDEV_MSG_CB callback, void *cbctx)
{
    IDEV *dev = calloc(1, sizeof(IDEV));
    return dev;
}

void idev_exit(void *ctx) { free(ctx); }

void idev_set(void *ctx, char *name, void *data)
{
    if (!ctx || !name) return;
    IDEV *dev = ctx;
    if      (strcmp(name, "callback") == 0) dev->callback = (PFN_IDEV_MSG_CB)data;
    else if (strcmp(name, "cbctx"   ) == 0) dev->cbctx = data;
}

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
