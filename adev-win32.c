#include <stdint.h>
#include <windows.h>
#include "adev.h"
#include "utils.h"

#define DEF_OUT_SAMPLE_RATE  8000
#define DEF_OUT_CHANEL_NUM   1
#define DEF_OUT_FRAME_SIZE   320
#define DEF_OUT_FRAME_NUM    3
typedef struct {
    HWAVEOUT hWaveOut;
    WAVEHDR *sWaveOutHdr;
    int      nWaveOutHead;
    int      nWaveOutTail;
    int      nWaveOutBufn;
    HANDLE   hWaveOutSem ;
} ADEV;

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ADEV *dev = (ADEV*)dwInstance;
    switch (uMsg) {
    case WOM_DONE:
        if (++dev->nWaveOutHead == dev->nWaveOutBufn) dev->nWaveOutHead = 0;
        ReleaseSemaphore(dev->hWaveOutSem, 1, NULL);
        break;
    }
}

void* adev_init(int out_samprate, int out_chnum, int out_frmsize, int out_frmnum)
{
    ADEV *dev = NULL;
    out_samprate = out_samprate ? out_samprate : DEF_OUT_SAMPLE_RATE;
    out_chnum    = out_chnum ? out_chnum : DEF_OUT_CHANEL_NUM;
    out_frmsize  = out_frmsize ? out_frmsize : DEF_OUT_FRAME_SIZE;
    out_frmnum   = out_frmnum ? out_frmnum : DEF_OUT_FRAME_NUM;
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

void adev_set(void *ctx, char *name, void *data)
{
    ADEV *dev = (ADEV*)ctx;
    if (dev && name) {
        if      (strcmp(name, "pause" ) == 0) waveOutPause  (dev->hWaveOut);
        else if (strcmp(name, "resume") == 0) waveOutRestart(dev->hWaveOut);
        else if (strcmp(name, "reset" ) == 0) waveOutReset  (dev->hWaveOut);
    }
}

void* adev_get(void *ctx, char *name) { return NULL; }
