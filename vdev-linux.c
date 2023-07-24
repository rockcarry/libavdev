#include <stdlib.h>
#include "vdev.h"

void* vdev_init(int w, int h, char *params, PFN_VDEV_MSG_CB callback, void *cbctx) { return NULL; }
void  vdev_exit(void *ctx, int close) {}
BMP * vdev_lock(void *ctx) { return NULL; }
void  vdev_unlock(void *ctx) {}
void  vdev_set(void *ctx, char *name, void *data) {}
long  vdev_get(void *ctx, char *name, void *data) { return 0; }
