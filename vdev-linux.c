#include <stdlib.h>
#include "vdev.h"

void* vdev_init(int w, int h, char *params, PFN_VDEV_MSG_CB callback, void *cbctx)
{
    return NULL;
}

void  vdev_exit(void *ctx, int close) {}
void  vdev_set(void *ctx, char *name, void *data) {}
void* vdev_get(void *ctx, char *name) { return NULL; }

