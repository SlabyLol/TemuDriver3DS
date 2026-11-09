#include "scanner.h"
#include "config.h"
#include <string.h>

static bool region_contains(const u8 *base, u32 size, const u8 *sig, u32 siglen)
{
    if (!base || size < siglen || !sig || siglen == 0) return false;

    const u8 *end = base + size - siglen;
    for (const u8 *p = base; p <= end; p++) {
        if (memcmp(p, sig, siglen) == 0)
            return true;
    }
    return false;
}

static int collect_regions(AuspMemRegion *out, int max)
{
    u32 addr = 0;
    int count = 0;
    MemInfo info;
    PageInfo page;

    while (count < max && addr < 0x40000000) {
        Result res = svcQueryMemory(&info, &page, addr);
        if (R_FAILED(res)) break;
        if (info.size == 0) break;

        bool interesting = false;

        if (AUSP_SCAN_CODE && (info.perm & MEMPERM_EXECUTE))
            interesting = true;
        if (AUSP_SCAN_DATA && (info.perm & MEMPERM_READ) && !(info.perm & MEMPERM_EXECUTE))
            interesting = true;

        if (interesting &&
            info.size >= AUSP_MIN_REGION_SIZE &&
            (info.state == MEMSTATE_CODE || info.state == MEMSTATE_PRIVATE || info.state == MEMSTATE_SHARED))
        {
            out[count].base  = info.base_addr;
            out[count].size  = info.size;
            out[count].state = info.state;
            out[count].perm  = info.perm;
            count++;
        }

        u32 next = info.base_addr + info.size;
        if (next <= addr) break;
        addr = next;
    }

    return count;
}

int ausp_scan_process(void)
{
    AuspMemRegion regions[AUSP_MAX_REGIONS];
    int n = collect_regions(regions, AUSP_MAX_REGIONS);

    for (int i = 0; i < n; i++) {
        const u8 *base = (const u8 *)regions[i].base;
        u32 size = regions[i].size;

        for (u32 s = 0; s < AUSP_SIGNATURE_COUNT; s++) {
            if (region_contains(base, size,
                                ausp_signatures[s].data,
                                ausp_signatures[s].length))
            {
                return 1;
            }
        }
    }

    return 0;
}
