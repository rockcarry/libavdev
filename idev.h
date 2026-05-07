#ifndef __LIBAVDEV_IDEV__
#define __LIBAVDEV_IDEV__

#include <stdint.h>
#include <pthread.h>

enum {
    IDEV_CALLBACK_KEY_EVENT = 0x10180000,
    IDEV_CALLBACK_MOUSE_MOVE,
    IDEV_CALLBACK_MOUSE_LBTNUP,
    IDEV_CALLBACK_MOUSE_LBTNDOWN,
    IDEV_CALLBACK_MOUSE_MBTNUP,
    IDEV_CALLBACK_MOUSE_MBTNDOWN,
    IDEV_CALLBACK_MOUSE_RBTNUP,
    IDEV_CALLBACK_MOUSE_RBTNDOWN,
};

typedef long (*PFN_IDEV_CB)(void *cbctx, int type, void *buf, int len);

typedef struct {
    #define KEYQUEUE_SIZE 10
    int      keyqueue_head;
    int      keyqueue_tail;
    int      keyqueue_size;
    uint16_t keyqueue_buf[KEYQUEUE_SIZE];
    pthread_mutex_t lock;

    uint32_t key_bits[8];
    int32_t  last_mouse_x, last_mouse_y, last_mouse_btns;
    int32_t  curr_mouse_x, curr_mouse_y, curr_mouse_btns;
    PFN_IDEV_CB callback;
    void       *cbctx;
} IDEV;

void* idev_init(void *params, PFN_IDEV_CB callback, void *cbctx);
void  idev_exit(void *ctx);
long  idev_set (void *ctx, char *key, void *val);
long  idev_get (void *ctx, char *key, void *val);
void  idev_dump(void *ctx, char *str, int len, int page);

int   idev_enqueue_key(void *ctx, int key);
int   idev_dequeue_key(void *ctx);

int   idev_getkey  (void *ctx, int key);
void  idev_getmouse(void *ctx, int *x, int *y, int *btns);

#endif

