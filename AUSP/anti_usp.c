#include "anti_usp.h"
#include "config.h"
#include "scanner.h"
#include "screen.h"
#include <stdio.h>

static AuspActionFn g_action = NULL;

void anti_usp_set_action(AuspActionFn fn)
{
    g_action = fn;
}

AuspResult anti_usp_check(void)
{
    if (ausp_scan_process())
        return AUSP_PLUGIN_FOUND;
    return AUSP_CLEAN;
}

void anti_usp_show_screen(void)
{
    ausp_draw_screen();
}

void anti_usp_run(void)
{
    if (anti_usp_check() != AUSP_PLUGIN_FOUND)
        return;

    if (AUSP_REACTION_MODE == AUSP_REACTION_SCREEN ||
        AUSP_REACTION_MODE == AUSP_REACTION_BOTH)
    {
        anti_usp_show_screen();
    }

    if (AUSP_REACTION_MODE == AUSP_REACTION_ACTION ||
        AUSP_REACTION_MODE == AUSP_REACTION_BOTH)
    {
        if (g_action)
            g_action();
    }

    if (AUSP_FORCE_EXIT)
        svcExitProcess();
}

const char *anti_usp_version(void)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d",
             AUSP_VERSION_MAJOR, AUSP_VERSION_MINOR, AUSP_VERSION_PATCH);
    return buf;
}
