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

long adev_get(void *ctx, char *name, void *data) { return 0; }

int adev_record(void *ctx, int start, int rec_samprate, int rec_chnum, int rec_frmsize, int rec_frmnum)
{
    return 0;
}
