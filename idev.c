#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "idev.h"

static long defcb(void *cbctx, int type, void *buf, int len) { return 0; }

void* idev_init(void *params, PFN_IDEV_CB callback, void *cbctx)
{
    if (!params) return NULL;
    IDEV *dev = calloc(1, sizeof(IDEV));
    if (!dev) return NULL;
    dev->callback = callback ? callback : defcb;
    dev->cbctx    = cbctx;
    pthread_mutex_init(&dev->lock, NULL);
    return dev;
}

void idev_exit(void *ctx) {
    if (!ctx) return;
    IDEV *dev = ctx;
    pthread_mutex_destroy(&dev->lock);
    free(ctx);
}

long idev_set(void *ctx, char *name, void *data) { return 0; }
long idev_get(void *ctx, char *name, void *data) { return 0; }

void idev_dump(void *ctx, char *str, int len, int page)
{
    if (!ctx || !str) return;
    IDEV *dev = ctx;
    snprintf(str, len,
        "self: %p\n"
        "key_bits: %08x %08x %08x %08x %08x %08x %08x %08x\n"
        "last_mouse: (%4d, %4d), btn: 0x%x\n"
        "curr_mouse: (%4d, %4d), btn: 0x%x\n"
        "callback: %p, cbctx: %p\n",
        dev, dev->key_bits[0], dev->key_bits[1], dev->key_bits[2], dev->key_bits[3], dev->key_bits[4], dev->key_bits[5], dev->key_bits[6], dev->key_bits[7],
        dev->last_mouse_x, dev->last_mouse_y, dev->last_mouse_btns,
        dev->curr_mouse_x, dev->curr_mouse_y, dev->curr_mouse_btns,
        dev->callback, dev->cbctx);
}

int idev_enqueue_key(void *ctx, int key)
{
    if (!ctx) return -1;
    IDEV *dev = ctx;
    int   n   = key & 0xFF;
    int   p   = (key >> 8);
    int   ret = -1;
    if (p) dev->key_bits[n / 32] |= (1 << (n % 32));
    else   dev->key_bits[n / 32] &=~(1 << (n % 32));
    pthread_mutex_lock(&dev->lock);
    if (dev->keyqueue_size < KEYQUEUE_SIZE) {
        dev->keyqueue_buf[dev->keyqueue_tail] = key;
        dev->keyqueue_size += 1;
        dev->keyqueue_tail += 1;
        dev->keyqueue_tail %= KEYQUEUE_SIZE;
        ret = 0;
    }
    pthread_mutex_unlock(&dev->lock);
    return ret;
}

int idev_dequeue_key(void *ctx)
{
    if (!ctx) return -1;
    IDEV *dev = ctx;
    int   key = -1;
    pthread_mutex_lock(&dev->lock);
    if (dev->keyqueue_size > 0) {
        key = dev->keyqueue_buf[dev->keyqueue_head];
        dev->keyqueue_size -= 1;
        dev->keyqueue_head += 1;
        dev->keyqueue_head %= KEYQUEUE_SIZE;
    }
    pthread_mutex_unlock(&dev->lock);
    return key;
}

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
    if (x   ) *x    = dev->curr_mouse_x;
    if (y   ) *y    = dev->curr_mouse_y;
    if (btns) *btns = dev->curr_mouse_btns;
}
