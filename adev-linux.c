#include <stdlib.h>
#include "adev.h"

void* adev_init(void *params, PFN_ADEV_CB callback, void *cbctx)
{
    return NULL;
}

void adev_exit(void *ctx)
{
}

int adev_play(void *ctx, void *buf, int len, int waitms)
{
    return 0;
}

long adev_set(void *ctx, char *key, void *val)
{
    return 0;
}

long adev_get(void *ctx, char *key, void *val)
{
    return 0;
}

void adev_dump(void *ctx, char *str, int len, int page)
{
}

