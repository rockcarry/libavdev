#include <stdlib.h>
#include "vdev.h"

void* vdev_init(void *params, PFN_VDEV_CB callback, void *cbctx) { return NULL; }
void  vdev_exit(void *ctx) {}

BMP*  vdev_lock  (void *ctx, int idx) { return NULL; }
void  vdev_unlock(void *ctx, int idx) {}
void  vdev_render(void *ctx) {}

long  vdev_set (void *ctx, char *key, void *val) { return 0; }
long  vdev_get (void *ctx, char *key, void *val) { return 0; }
void  vdev_dump(void *ctx, char *str, int len, int page) {}
