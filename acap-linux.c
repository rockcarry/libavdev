#include <stdlib.h>
#include "acap.h"

void* acap_init(void *params, PFN_ACAP_CB callback, void *cbctx) { return NULL; }
void  acap_exit(void *ctx) {}
long  acap_set (void *ctx, char *key, void *val) { return 0; }
long  acap_get (void *ctx, char *key, void *val) { return 0; }
void  acap_dump(void *ctx, char *str, int len, int page) {}

