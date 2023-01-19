#ifndef __FFRENDER_ADEV__
#define __FFRENDER_ADEV__

void* adev_init(int out_samprate, int out_chnum, int out_frmsize, int out_frmnum);
void  adev_free(void *ctx);
int   adev_play(void *ctx, void *buf, int len, int waitms);

#endif
