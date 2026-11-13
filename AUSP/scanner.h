#ifndef AUSP_SCANNER_H
#define AUSP_SCANNER_H

#include <3ds.h>

typedef struct {
    u32 base;
    u32 size;
    u32 state;
    u32 perm;
} AuspMemRegion;

int ausp_scan_process(void);

#endif
