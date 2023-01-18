#include <stdlib.h>
#include <stdio.h>
#include "texture.h"
#include "display.h"

int main(void)
{
    void    *disp    = display_init(640, 480, "inithidden", NULL, NULL);
    TEXTURE *texture = display_get(disp, "texture");
    int      i, j;
    texture_lock(texture);
    for (i = 0; i < texture->h; i++) {
        for (j = 0; j < texture->w; j++) {
            texture_setcolor(texture, j, i, RGB(i, j, i));
        }
    }
    texture_unlock(texture);
    display_set(disp, "show", NULL);
    display_free(disp, 0);
    return 0;
}

