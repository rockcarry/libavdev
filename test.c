#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "texture.h"
#include "adev.h"
#include "vdev.h"

#ifdef _TEST_ADEV_
static void gen_sin_wav(int16_t *pcm, int n, int samprate, int freq)
{
    int  i;
    for (i = 0; i < n; i++) {
        pcm[i] = 32760 * sin(i * 2 * M_PI * freq / samprate);
    }
}

int main(void)
{
    void *adev = adev_init(0, 0, 0, 0);
    int16_t buf[320];
    gen_sin_wav(buf, 320, 8000, 500);
    while (1) {
        adev_play(adev, buf, sizeof(buf), -1);
    }
    adev_exit(adev);
    return 0;
}
#endif

#ifdef _TEST_RECORD_
static void my_adev_callback(void *ctxt, int cmd, void *buf, int len)
{
    void    *vdev    = ctxt;
    int16_t *pcm     = buf;
    TEXTURE *texture = vdev ? vdev_get(vdev, "texture") : NULL;
    switch (cmd) {
    case ADEV_CMD_DATA_RECORD:
#ifdef DEBUG
        printf("record data, ctxt: %p, buf: %p, len: %d\n", ctxt, buf, len); fflush(stdout);
#endif
        if (texture && buf && len > 0) {
            int lasty, cury, x, i;
            if (0 == texture_lock(texture)) {
                texture_fillrect(texture, 0, 0, texture->w, texture->h, 0);
                lasty = pcm[0] * texture->h / 0x10000 + texture->h / 2;
                for (x = 1; x < texture->w; x++) {
                    i    = (len / sizeof(int16_t) - 1) * x / (texture->w - 1);
                    cury = texture->h / 2 - pcm[i] * texture->h / 0x10000;
                    texture_line(texture, x - 1, lasty, x, cury, RGB(0, 255, 0));
                    lasty = cury;
                }
                texture_unlock(texture);
            }
        }
        break;
    }
}

int main(void)
{
    void *adev = adev_init(0, 0, 0, 0);
    void *vdev = vdev_init(640, 480, NULL, NULL, NULL);

    adev_set(adev, "callback", my_adev_callback);
    adev_set(adev, "cbctx"   , vdev);
    adev_record(adev, 1, 0, 0, 0, 0);

    vdev_exit(vdev, 0);
    adev_exit(adev);
    return 0;
}
#endif

#ifdef _TEST_VDEV_
int main(void)
{
    void    *vdev    = vdev_init(640, 480, "inithidden", NULL, NULL);
    TEXTURE *texture = vdev_get(vdev, "texture");
    int      i, j;
    if (0 == texture_lock(texture)) {
        for (i = 0; i < texture->h; i++) {
            for (j = 0; j < texture->w; j++) {
                texture_setcolor(texture, j, i, RGB(i, j, i));
            }
        }
        texture_unlock(texture);
    }
    vdev_set(vdev, "show", NULL);
    vdev_exit(vdev, 0);
    return 0;
}
#endif
