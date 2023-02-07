#ifndef __LIBAVDEV_VDEV__
#define __LIBAVDEV_VDEV__

#include <stdint.h>

enum {
    VDEV_MSG_KEY_EVENT = 1,
    VDEV_MSG_MOUSE_EVENT,
};

typedef int (*PFN_VDEV_MSG_CB)(void *cbctx, int msg, uint32_t param1, uint32_t param2, void *param3);

// params: "fullscreen" - use directdraw fullscreen mode, "inithidden" - init window but not show it
void* vdev_init(int w, int h, char *params, PFN_VDEV_MSG_CB callback, void *cbctx);
void  vdev_free(void *ctx, int close);
void  vdev_set (void *ctx, char *name, void *data);
void* vdev_get (void *ctx, char *name);

#endif
