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
