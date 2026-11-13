#ifndef AUSP_SIGNATURES_H
#define AUSP_SIGNATURES_H

#include <3ds.h>

typedef struct {
    const u8 *data;
    u32 length;
    const char *name;
    int is_nimbus;
} AuspSignature;

extern const AuspSignature ausp_signatures[];
extern const u32 AUSP_SIGNATURE_COUNT;

#endif
