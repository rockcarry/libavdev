#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
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
    int      nWaveOutBufn;
    HANDLE   hWaveOutSem ;

    HWAVEIN  hWaveRec;
    WAVEHDR *sWaveRecHdr;
    int      nWaveRecBufn;

    PFN_ADEV_CALLBACK callback;
    void             *cbctx;
} ADEV;

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ADEV    *dev = (ADEV*)dwInstance;
    WAVEHDR *phdr= (WAVEHDR*)dwParam1;
    switch (uMsg) {
    case WOM_DONE:
        if (dev->callback) dev->callback(dev->cbctx, ADEV_CMD_DATA_PLAY, phdr->lpData, phdr->dwBytesRecorded);
        if (++dev->nWaveOutHead == dev->nWaveOutBufn) dev->nWaveOutHead = 0;
        ReleaseSemaphore(dev->hWaveOutSem, 1, NULL);
        break;
    }
}

static BOOL CALLBACK waveRecProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ADEV    *dev = (ADEV*)dwInstance;
    WAVEHDR *phdr= (WAVEHDR*)dwParam1;
    switch (uMsg) {
    case WIM_DATA:
        if (dev->callback) dev->callback(dev->cbctx, ADEV_CMD_DATA_RECORD, phdr->lpData, phdr->dwBytesRecorded);
        waveInAddBuffer(hwi, phdr, sizeof(WAVEHDR));
        break;
    }
    return TRUE;
}

void* adev_init(int out_samprate, int out_chnum, int out_frmsize, int out_frmnum)
{
    ADEV *dev = NULL;
    out_samprate = out_samprate ? out_samprate : DEF_OUT_SAMPLE_RATE;
    out_chnum    = out_chnum    ? out_chnum    : DEF_OUT_CHANEL_NUM;
    out_frmsize  = out_frmsize  ? out_frmsize  : DEF_OUT_FRAME_SIZE;
    out_frmnum   = out_frmnum   ? out_frmnum   : DEF_OUT_FRAME_NUM;
    dev = calloc(1, sizeof(ADEV) + out_frmnum * (sizeof(WAVEHDR) + out_frmsize * sizeof(int16_t) * out_chnum));
    if (dev) {
        WAVEFORMATEX wfx = {0};
        DWORD        result;
        int          i;
        wfx.cbSize          = sizeof(wfx);
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample  = 16;
        wfx.nSamplesPerSec  = out_samprate;
        wfx.nChannels       = out_chnum;
        wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
        result = waveOutOpen(&dev->hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)dev, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            adev_exit(dev); dev = NULL;
        }

        dev->sWaveOutHdr = (WAVEHDR*)(dev + 1);
        for (i = 0; i < out_frmnum; i++) {
            dev->sWaveOutHdr[i].dwUser = (DWORD_PTR)(out_frmsize * sizeof(int16_t) * out_chnum);
            dev->sWaveOutHdr[i].lpData = (LPSTR)(dev + 1) + out_frmnum * sizeof(WAVEHDR) + i * out_frmsize * sizeof(int16_t) * out_chnum;
            waveOutPrepareHeader(dev->hWaveOut, &dev->sWaveOutHdr[i], sizeof(WAVEHDR));
        }

        dev->hWaveOutSem  = CreateSemaphore(NULL, out_frmnum, out_frmnum, NULL);
        dev->nWaveOutBufn = out_frmnum;
    }
    return dev;
}

void adev_exit(void *ctx)
{
    ADEV *dev = (ADEV*)ctx;
    if (dev) {
        int  i;
        waveOutReset(dev->hWaveOut);
        for (i = 0; i < dev->nWaveOutBufn; i++) {
            if (dev->sWaveOutHdr[i].lpData) waveOutUnprepareHeader(dev->hWaveOut, &dev->sWaveOutHdr[i], sizeof(WAVEHDR));
        }
        waveOutClose(dev->hWaveOut);
        CloseHandle (dev->hWaveOutSem);
        adev_record(dev, 0, 0, 0, 0, 0);
        free(dev);
    }
}

int adev_play(void *ctx, void *buf, int len, int waitms)
{
    ADEV *dev = (ADEV*)ctx;
    int   ret = -1;
    if (dev) {
        if (WAIT_OBJECT_0 != WaitForSingleObject(dev->hWaveOutSem, waitms)) goto done;
        dev->sWaveOutHdr[dev->nWaveOutTail].dwBufferLength = MIN((int)dev->sWaveOutHdr[dev->nWaveOutTail].dwUser, len);
        memcpy(dev->sWaveOutHdr[dev->nWaveOutTail].lpData, buf, dev->sWaveOutHdr[dev->nWaveOutTail].dwBufferLength);
        waveOutWrite(dev->hWaveOut, &dev->sWaveOutHdr[dev->nWaveOutTail], sizeof(WAVEHDR));
        if (++dev->nWaveOutTail == dev->nWaveOutBufn) dev->nWaveOutTail = 0;
        ret = 0;
    }
done:
    return ret;
}

int adev_record(void *ctx, int start, int rec_samprate, int rec_chnum, int rec_frmsize, int rec_frmnum)
{
    int   ret = -1, i;
    ADEV *dev = (ADEV*)ctx;
    if (!dev) return -1;

    if (start && !dev->hWaveRec) {
        rec_samprate = rec_samprate ? rec_samprate : DEF_REC_SAMPLE_RATE;
        rec_chnum    = rec_chnum    ? rec_chnum    : DEF_REC_CHANEL_NUM;
        rec_frmsize  = rec_frmsize  ? rec_frmsize  : DEF_REC_FRAME_SIZE;
        rec_frmnum   = rec_frmnum   ? rec_frmnum   : DEF_REC_FRAME_NUM;

        WAVEFORMATEX wfx = {0};
        DWORD        result;
        wfx.cbSize          = sizeof(wfx);
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample  = 16;
        wfx.nSamplesPerSec  = rec_samprate;
        wfx.nChannels       = rec_chnum;
        wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
        result = waveInOpen(&dev->hWaveRec, WAVE_MAPPER, &wfx, (DWORD_PTR)waveRecProc, (DWORD_PTR)dev, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            printf("failed to open wavein device !\n");
            start = 0; goto handle_stop;
        }

        dev->nWaveRecBufn = rec_frmnum;
        dev->sWaveRecHdr  = malloc(rec_frmnum * (sizeof(WAVEHDR) + rec_frmsize * sizeof(int16_t) * rec_chnum));
        if (!dev->sWaveRecHdr) {
            printf("failed to allocate wavehdr for wavein !\n");
            start = 0; goto handle_stop;
        }

        for (i = 0; i < rec_frmnum; i++) {
            dev->sWaveRecHdr[i].dwBufferLength = (DWORD_PTR)(rec_frmsize * sizeof(int16_t) * rec_chnum);
            dev->sWaveRecHdr[i].lpData         = (LPSTR)dev->sWaveRecHdr + rec_frmnum * sizeof(WAVEHDR) + i * rec_frmsize * sizeof(int16_t) * rec_chnum;
            waveInPrepareHeader(dev->hWaveRec, &dev->sWaveRecHdr[i], sizeof(WAVEHDR));
            waveInAddBuffer    (dev->hWaveRec, &dev->sWaveRecHdr[i], sizeof(WAVEHDR));
        }
        waveInStart(dev->hWaveRec);
        ret = 0;
    }

handle_stop:
    if (!start && dev->hWaveRec) {
        waveInStop(dev->hWaveRec);
        if (dev->sWaveRecHdr) {
            for (i = 0; i < dev->nWaveRecBufn; i++) {
                if (dev->sWaveRecHdr[i].lpData) waveInUnprepareHeader(dev->hWaveRec, &dev->sWaveRecHdr[i], sizeof(WAVEHDR));
            }
            free(dev->sWaveRecHdr);
        }
        waveInClose(dev->hWaveRec);
        dev->hWaveRec     = NULL;
        dev->sWaveRecHdr  = NULL;
        dev->nWaveRecBufn = 0;
    }
    return ret;
}

void adev_set(void *ctx, char *name, void *data)
{
    ADEV *dev = (ADEV*)ctx;
    if (dev && name) {
        if      (strcmp(name, "pause"   ) == 0) waveOutPause  (dev->hWaveOut);
        else if (strcmp(name, "resume"  ) == 0) waveOutRestart(dev->hWaveOut);
        else if (strcmp(name, "reset"   ) == 0) waveOutReset  (dev->hWaveOut);
        else if (strcmp(name, "callback") == 0) dev->callback = data;
        else if (strcmp(name, "cbctx"   ) == 0) dev->cbctx    = data;
    }
}

long adev_get(void *ctx, char *name, void *data) { return 0; }
