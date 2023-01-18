#include <stdlib.h>
#include <stdio.h>
#include "texture.h"
#include "vdev.h"

int main(void)
{
    void    *vdev    = vdev_init(640, 480, "inithidden", NULL, NULL);
    TEXTURE *texture = vdev_get(vdev, "texture");
    int      i, j;
    texture_lock(texture);
    for (i = 0; i < texture->h; i++) {
        for (j = 0; j < texture->w; j++) {
            texture_setcolor(texture, j, i, RGB(i, j, i));
        }
    }
    texture_unlock(texture);
    vdev_set(vdev, "show", NULL);
    vdev_free(vdev, 0);
    return 0;
}

