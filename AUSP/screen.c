#include "screen.h"
#include "config.h"
#include <3ds.h>
#include <stdio.h>

void ausp_draw_screen(void)
{
    gfxInitDefault();

    if (AUSP_SCREEN_TOP)
        consoleInit(GFX_TOP, NULL);
    if (AUSP_SCREEN_BOTTOM)
        consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[2;1H");
    printf("  ========================================\n");
    printf("           %s\n", AUSP_TITLE);
    printf("           %s\n", AUSP_SUBTITLE);
    printf("  ========================================\n\n");
    printf("  %s\n", AUSP_LINE1);
    printf("  %s\n\n", AUSP_LINE2);
    printf("  %s\n", AUSP_LINE3);
    printf("  %s\n\n", AUSP_LINE4);
    printf("  %s\n\n", AUSP_LINE5);
    printf("  ========================================\n");
    printf("           %s\n", AUSP_FOOTER);
    printf("  ========================================\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (AUSP_EXIT_ON_KEY && kDown)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
}
