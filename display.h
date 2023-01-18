#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <stdint.h>

enum {
    DISP_MSG_KEY_EVENT = 1,
    DISP_MSG_MOUSE_EVENT,
};

typedef int (*PFN_DISP_MSG_CB)(void *cbctx, int msg, uint32_t param1, uint32_t param2, void *param3);

// params: "fullscreen" - use directdraw fullscreen mode, "inithidden" - init window but not show it
void* display_init(int w, int h, char *params, PFN_DISP_MSG_CB callback, void *cbctx);
void  display_free(void *ctx, int close);
void  display_set (void *ctx, char *name, void *data);
void* display_get (void *ctx, char *name);

#endif
