#include <stdlib.h>
#include "adev.h"

void* adev_init(int out_samprate, int out_chnum, int out_frmsize, int out_frmnum)
{
    return NULL;
}

void adev_exit(void *ctx) {}

int adev_play(void *ctx, void *buf, int len, int waitms)
{
    return 0;
}

void adev_set(void *ctx, char *name, void *data) {}

void* adev_get(void *ctx, char *name) { return NULL; }
