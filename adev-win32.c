#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "adev.h"
#include "utils.h"

#define DEF_OUT_SAMPLE_RATE  8000
#define DEF_OUT_CHANEL_NUM   1
#define DEF_OUT_FRAME_SIZE   320
#define DEF_OUT_FRAME_NUM    3

#define DEF_REC_SAMPLE_RATE  8000
#define DEF_REC_CHANEL_NUM   1
#define DEF_REC_FRAME_SIZE   320
#define DEF_REC_FRAME_NUM    3

typedef struct {
    HWAVEOUT hWaveOut;
    WAVEHDR *sWaveOutHdr;
    int      nWaveOutHead;
    int      nWaveOutTail;
    int      nWaveOutCurr;
    int      nWaveOutBufn;
    HANDLE   hWaveOutSem ;

    int      samprate;
    int      chnnum;
    int      frmsize;
    uint32_t total;
} ADEV;

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ADEV *adev = (ADEV*)dwInstance;
    switch (uMsg) {
    case WOM_DONE:
        if (++adev->nWaveOutHead == adev->nWaveOutBufn) adev->nWaveOutHead = 0;
        adev->nWaveOutCurr--;
        ReleaseSemaphore(adev->hWaveOutSem, 1, NULL);
        break;
    }
}

void* adev_init(void *params, PFN_ADEV_CB callback, void *cbctx)
{
    if (!params) return NULL;
    int samprate = 16000, chnnum = 1, frmsize = 512, frmnum = 3;
    char *str;
    str = strstr(params, "samprate:"); if (str) sscanf(str, "samprate:%d", &samprate);
    str = strstr(params, "chnnum:"  ); if (str) sscanf(str, "chnnum:%d"  , &chnnum  );
    str = strstr(params, "frmsize:" ); if (str) sscanf(str, "frmsize:%d" , &frmsize );
    str = strstr(params, "frmnum:"  ); if (str) sscanf(str, "frmnum:%d"  , &frmnum  );

    ADEV *adev = calloc(1, sizeof(ADEV) + frmnum * (sizeof(WAVEHDR) + frmsize * sizeof(int16_t) * chnnum));
    if (!adev) return NULL;

    WAVEFORMATEX wfx = {0};
    DWORD        result;
    int          i;
    wfx.cbSize          = sizeof(wfx);
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.wBitsPerSample  = 16;
    wfx.nSamplesPerSec  = samprate;
    wfx.nChannels       = chnnum;
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    result = waveOutOpen(&adev->hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)adev, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        printf("<4> adev_init waveOutOpen failed !\n");
        adev_exit(adev); return NULL;
    }

    adev->sWaveOutHdr = (WAVEHDR*)(adev + 1);
    for (i = 0; i < frmnum; i++) {
        adev->sWaveOutHdr[i].dwUser = (DWORD_PTR)(frmsize * sizeof(int16_t) * chnnum);
        adev->sWaveOutHdr[i].lpData = (LPSTR)(adev + 1) + frmnum * sizeof(WAVEHDR) + i * frmsize * sizeof(int16_t) * chnnum;
        waveOutPrepareHeader(adev->hWaveOut, &adev->sWaveOutHdr[i], sizeof(WAVEHDR));
    }

    adev->hWaveOutSem  = CreateSemaphore(NULL, frmnum, frmnum, NULL);
    adev->nWaveOutBufn = frmnum;
    adev->samprate     = samprate;
    adev->chnnum       = chnnum;
    adev->frmsize      = frmsize;
    return adev;
}

void adev_exit(void *ctx)
{
    if (!ctx) return;
    ADEV *adev = (ADEV*)ctx;
    waveOutReset(adev->hWaveOut);
    for (int i = 0; i < adev->nWaveOutBufn; i++) {
        if (adev->sWaveOutHdr[i].lpData) waveOutUnprepareHeader(adev->hWaveOut, &adev->sWaveOutHdr[i], sizeof(WAVEHDR));
    }
    waveOutClose(adev->hWaveOut);
    CloseHandle (adev->hWaveOutSem);
    free(adev);
}

int adev_play(void *ctx, void *buf, int len, int waitms)
{
    if (!ctx) return -1;
    ADEV *adev = (ADEV*)ctx;
    if (WAIT_OBJECT_0 != WaitForSingleObject(adev->hWaveOutSem, waitms)) return -1;
    adev->sWaveOutHdr[adev->nWaveOutTail].dwBufferLength = MIN((int)adev->sWaveOutHdr[adev->nWaveOutTail].dwUser, len);
    memcpy(adev->sWaveOutHdr[adev->nWaveOutTail].lpData, buf, adev->sWaveOutHdr[adev->nWaveOutTail].dwBufferLength);
    waveOutWrite(adev->hWaveOut, &adev->sWaveOutHdr[adev->nWaveOutTail], sizeof(WAVEHDR));
    if (++adev->nWaveOutTail == adev->nWaveOutBufn) adev->nWaveOutTail = 0;
    adev->nWaveOutCurr++;
    adev->total += len;
    return 0;
}

long adev_set(void *ctx, char *key, void *val)
{
    return 0;
}

long adev_get(void *ctx, char *key, void *val)
{
    if (!ctx || !key) return 0;
    ADEV *adev = (ADEV*)ctx;
    if (strcmp(key, ADEV_KEY_SAMPRATE) == 0) return (long)adev->samprate;
    return 0;
}

void adev_dump(void *ctx, char *str, int len, int page)
{
    if (!ctx) return;
    ADEV *adev = (ADEV*)ctx;
    snprintf(str, len,
        "self: %p, samprate: %d, chnnum: %d, frmsize: %d, total: %d\n"
        "hWaveOut: %p, sWaveOutHdr: %p, nWaveOutHead: %d, nWaveOutTail: %d, nWaveOutCurr: %d, nWaveOutBufn: %d, hWaveOutSem: %p\n",
        adev, adev->samprate, adev->chnnum, adev->frmsize, adev->total,
        adev->hWaveOut, adev->sWaveOutHdr, adev->nWaveOutHead, adev->nWaveOutTail, adev->nWaveOutCurr, adev->nWaveOutBufn, adev->hWaveOutSem);
}
