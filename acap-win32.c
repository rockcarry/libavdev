#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "acap.h"
#include "utils.h"

typedef struct {
    HWAVEIN  hWaveRec;
    WAVEHDR *sWaveRecHdr;
    int      nWaveRecBufn;

    #define FLAG_RAW  (1 << 0)
    #define FLAG_PCM  (1 << 1)
    #define FLAG_ALAW (1 << 2)
    #define FLAG_ULAW (1 << 3)
    uint32_t      flags;
    int           samprate;
    int           chnnum;
    int           frmsize;
    int           total;

    PFN_ACAP_CB   callback;
    void         *cbctx;

    uint8_t      *g711_buf;
    int           g711_len;
} ACAP;

static uint8_t pcm2alaw(int16_t pcm)
{
    uint8_t sign = (pcm >> 8) & (1 << 7);
    int     mask, eee, wxyz, alaw;
    if (sign) pcm = -pcm;
    for (mask = (1 << 14), eee = 7; !(pcm & mask) && eee; eee--, mask >>= 1);
    wxyz = (pcm >> (!eee ? 4 : (eee + 3))) & 0xf;
    alaw = sign | (eee << 4) | wxyz;
    return (alaw ^ 0xd5);
}

static uint8_t pcm2ulaw(int16_t pcm)
{
    uint8_t sign = (pcm >> 8) & (1 << 7);
    int     mask, eee, wxyz, ulaw;
    if (sign) pcm = -pcm;
#if 1 // enable biased linear input
    pcm  = pcm < 0x7f7b ? pcm : 0x7f7b;
    pcm += 0x84;
#endif
    for (mask = (1 << 14), eee = 7; !(pcm & mask) && eee; eee--, mask >>= 1);
    wxyz = (pcm >> (eee + 3)) & 0xf;
    ulaw = sign | (eee << 4) | wxyz;
    return (ulaw ^ 0xff);
}

static BOOL CALLBACK waveRecProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ACAP    *acap = (ACAP*)dwInstance;
    WAVEHDR *phdr = (WAVEHDR*)dwParam1;
    ACAP_FRAME frame;
    switch (uMsg) {
    case WIM_DATA:
        frame.ts = get_tick_count();
        if (acap->flags & FLAG_RAW) {
            frame.type = ACAP_FRAME_TYPE_RAW;
            frame.buf  = (uint8_t*)phdr->lpData;
            frame.len  = phdr->dwBytesRecorded;
            acap->callback(acap->cbctx, ACAP_CALLBACK_DATA, &frame, sizeof(frame));
        }
        if (acap->flags & FLAG_PCM) {
            frame.type = ACAP_FRAME_TYPE_PCM;
            frame.buf  = (uint8_t*)phdr->lpData;
            frame.len  = phdr->dwBytesRecorded;
            acap->callback(acap->cbctx, ACAP_CALLBACK_DATA, &frame, sizeof(frame));
        }
        if (acap->flags & (FLAG_ALAW|FLAG_ULAW)) {
            if (acap->g711_len < phdr->dwBytesRecorded / sizeof(int16_t)) {
                uint8_t *buf = realloc(acap->g711_buf, phdr->dwBytesRecorded / sizeof(int16_t));
                if (buf) {
                    acap->g711_buf = buf;
                    acap->g711_len = phdr->dwBytesRecorded / sizeof(int16_t);
                } else {
                    printf("<4> acap->g711_buf is NULL !\n");
                    break;
                }
            }
        }
        if (acap->flags & FLAG_ALAW) {
            for (int i = 0; i < phdr->dwBytesRecorded / sizeof(int16_t); i++) acap->g711_buf[i] = pcm2alaw(((int16_t*)phdr->lpData)[i]);
            frame.type = ACAP_FRAME_TYPE_ALAW;
            frame.buf  = acap->g711_buf;
            frame.len  = phdr->dwBytesRecorded / sizeof(int16_t);
            acap->callback(acap->cbctx, ACAP_CALLBACK_DATA, &frame, sizeof(frame));
        }
        if (acap->flags & FLAG_ULAW) {
            for (int i = 0; i < phdr->dwBytesRecorded / sizeof(int16_t); i++) acap->g711_buf[i] = pcm2ulaw(((int16_t*)phdr->lpData)[i]);
            frame.type = ACAP_FRAME_TYPE_ULAW;
            frame.buf  = acap->g711_buf;
            frame.len  = phdr->dwBytesRecorded / sizeof(int16_t);
            acap->callback(acap->cbctx, ACAP_CALLBACK_DATA, &frame, sizeof(frame));
        }
        acap->total += phdr->dwBytesRecorded;
        waveInAddBuffer(hwi, phdr, sizeof(WAVEHDR));
        break;
    }
    return TRUE;
}

static void acap_set_state(ACAP *acap, int state)
{
    switch (state) {
    case 0: // stop
    case 2: // pause async
    case 3: // pause sync
        if (acap->hWaveRec) {
            waveInStop(acap->hWaveRec);
            if (acap->sWaveRecHdr) {
                for (int i = 0; i < acap->nWaveRecBufn; i++) {
                    if (acap->sWaveRecHdr[i].lpData) waveInUnprepareHeader(acap->hWaveRec, &acap->sWaveRecHdr[i], sizeof(WAVEHDR));
                }
            }
            waveInClose(acap->hWaveRec);
            acap->hWaveRec = NULL;
        }
        break;
    case 1:
        if (!acap->hWaveRec) {
            WAVEFORMATEX wfx = {0};
            DWORD        result;
            wfx.cbSize          = sizeof(wfx);
            wfx.wFormatTag      = WAVE_FORMAT_PCM;
            wfx.wBitsPerSample  = 16;
            wfx.nSamplesPerSec  = acap->samprate;
            wfx.nChannels       = acap->chnnum;
            wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
            wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
            result = waveInOpen(&acap->hWaveRec, WAVE_MAPPER, &wfx, (DWORD_PTR)waveRecProc, (DWORD_PTR)acap, CALLBACK_FUNCTION);
            if (result != MMSYSERR_NOERROR) { printf("acap_set_state, waveInOpen failed !\n"); break; }

            for (int i = 0; i < acap->nWaveRecBufn; i++) {
                acap->sWaveRecHdr[i].dwBufferLength = (DWORD_PTR)(acap->frmsize * sizeof(int16_t) * acap->chnnum);
                acap->sWaveRecHdr[i].lpData         = (LPSTR)acap->sWaveRecHdr + acap->nWaveRecBufn * sizeof(WAVEHDR) + i * acap->frmsize * sizeof(int16_t) * acap->chnnum;
                waveInPrepareHeader(acap->hWaveRec, &acap->sWaveRecHdr[i], sizeof(WAVEHDR));
                waveInAddBuffer    (acap->hWaveRec, &acap->sWaveRecHdr[i], sizeof(WAVEHDR));
            }
            waveInStart(acap->hWaveRec);
        }
        break;
    case 4: // restart
        acap_set_state(acap, 0);
        acap_set_state(acap, 1);
        break;
    }
}

static long defcb(void *ctx, int type, void *buf, int len) { return 0; }

void* acap_init(void *params, PFN_ACAP_CB callback, void *cbctx)
{
    if (!params) return NULL;
    int samprate = 16000, chnnum = 1, frmsize = 640, frmnum = 16;
    char *str;
    str = strstr(params, "samprate:"); if (str) sscanf(str, "samprate:%d", &samprate);
    str = strstr(params, "chnnum:"  ); if (str) sscanf(str, "chnnum:%d"  , &chnnum  );
    str = strstr(params, "frmsize:" ); if (str) sscanf(str, "frmsize:%d" , &frmsize );
    str = strstr(params, "frmnum:"  ); if (str) sscanf(str, "frmnum:%d"  , &frmnum  );

    ACAP *acap = calloc(1, sizeof(ACAP) + frmnum * (sizeof(WAVEHDR) + frmsize * sizeof(int16_t) * chnnum));
    if (!acap) return NULL;

    acap->sWaveRecHdr  = (WAVEHDR*)(acap + 1);
    acap->nWaveRecBufn = frmnum;
    acap->flags        = FLAG_PCM|FLAG_ALAW;
    acap->samprate     = samprate;
    acap->chnnum       = chnnum;
    acap->frmsize      = frmsize;
    acap->callback     = callback ? callback : defcb;
    acap->cbctx        = cbctx;
    return acap;
}

void acap_exit(void *ctx)
{
    if (!ctx) return;
    ACAP *acap = ctx;
    acap_set_state(acap, 0);
    free(acap->g711_buf);
    free(acap);
}

long acap_set(void *ctx, char *key, void *val)
{
    if (!ctx || !key) return -1;
    ACAP *acap = (ACAP*)ctx;
    if (strcmp(key, ACAP_KEY_STATE) == 0) {
        acap_set_state(acap, (intptr_t)val);
    } else if (strncmp(key, "i_en_", sizeof("i_en_") - 1) == 0) {
        uint32_t flags = 0;
        if (strstr(key, "raw" )) flags |= FLAG_RAW;
        if (strstr(key, "pcm" )) flags |= FLAG_PCM;
        if (strstr(key, "alaw")) flags |= FLAG_ALAW;
        if (strstr(key, "ulaw")) flags |= FLAG_ULAW;
        if (strstr(key, "all" )) flags |= FLAG_RAW|FLAG_PCM|FLAG_ALAW|FLAG_ULAW;
        if (val) acap->flags |=  flags;
        else     acap->flags &= ~flags;
    } else return -1;
    return 0;
}

long acap_get(void *ctx, char *key, void *val)
{
    if (!ctx || !key) return 0;
    ACAP *acap = (ACAP*)ctx;
    if (strcmp(key, ACAP_KEY_STATE   ) == 0) return (long)!!(acap->hWaveRec);
    if (strcmp(key, ACAP_KEY_EN_RAW  ) == 0) return (long)!!(acap->flags & FLAG_RAW );
    if (strcmp(key, ACAP_KEY_EN_PCM  ) == 0) return (long)!!(acap->flags & FLAG_PCM );
    if (strcmp(key, ACAP_KEY_EN_ALAW ) == 0) return (long)!!(acap->flags & FLAG_ALAW);
    if (strcmp(key, ACAP_KEY_EN_ULAW ) == 0) return (long)!!(acap->flags & FLAG_ULAW);
    if (strcmp(key, ACAP_KEY_SAMPRATE) == 0) return (long)acap->samprate;
    if (strcmp(key, ACAP_KEY_FRMSIZE ) == 0) return (long)acap->frmsize;
    return 0;
}

void acap_dump(void *ctx, char *str, int len, int page)
{
    if (!ctx) return;
    ACAP *acap = (ACAP*)ctx;
    snprintf(str, len,
        "self: %p, flags: 0x%x, samprate: %d, chnnum: %d, frmsize: %d, total: %d\n"
        "hWaveRec: %p, sWaveRecHdr: %p, nWaveRecBufn: %d\n"
        "callback: %p, cbctx: %p, g711_buf: %p, g711_len: %d\n",
        acap, acap->flags, acap->samprate, acap->chnnum, acap->frmsize, acap->total,
        acap->hWaveRec, acap->sWaveRecHdr, acap->nWaveRecBufn,
        acap->callback, acap->cbctx, acap->g711_buf, acap->g711_len);
}
