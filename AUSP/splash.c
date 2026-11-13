#include "splash.h"
#include "config.h"
#include <citro2d.h>
#include <3ds.h>

static void draw_logo(float cx, float cy, float scale)
{
    float s = scale;

    C2D_DrawRectSolid(cx - 55*s, cy - 45*s, 0.4f, 110*s, 90*s, AUSP_SPLASH_ACCENT);
    C2D_DrawRectSolid(cx - 48*s, cy - 38*s, 0.5f, 96*s, 76*s, AUSP_SPLASH_BG);

    C2D_DrawCircleSolid(cx, cy - 5*s, 0.6f, 28*s, AUSP_SPLASH_ACCENT);
    C2D_DrawCircleSolid(cx, cy - 5*s, 0.7f, 18*s, AUSP_SPLASH_BG);

    C2D_DrawRectSolid(cx - 8*s, cy + 18*s, 0.6f, 16*s, 22*s, AUSP_SPLASH_ACCENT);
}

void ausp_show_splash(void)
{
    if (!AUSP_SPLASH_ENABLE)
        return;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    C2D_TextBuf buf = C2D_TextBufNew(4096);
    C2D_Text txtTitle, txtSub, txtSupported;

    C2D_TextParse(&txtTitle, buf, "AUSP-3DS");
    C2D_TextOptimize(&txtTitle);

    C2D_TextParse(&txtSub, buf, "Anti-Cheat Protection");
    C2D_TextOptimize(&txtSub);

    C2D_TextParse(&txtSupported, buf, "Supported by AUSP-3DS");
    C2D_TextOptimize(&txtSupported);

    int frames = 0;
    while (aptMainLoop() && frames < AUSP_SPLASH_DURATION)
    {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, AUSP_SPLASH_BG);
        C2D_SceneBegin(top);

        draw_logo(200.0f, 95.0f, 1.15f);

        C2D_DrawText(&txtTitle, C2D_AlignCenter | C2D_WithColor,
                     200.0f, 155.0f, 0.5f, 0.85f, 0.85f, AUSP_SPLASH_TEXT);

        C2D_DrawText(&txtSub, C2D_AlignCenter | C2D_WithColor,
                     200.0f, 185.0f, 0.5f, 0.55f, 0.55f, AUSP_SPLASH_SUB);

        C2D_DrawText(&txtSupported, C2D_AlignCenter | C2D_WithColor,
                     200.0f, 215.0f, 0.5f, 0.50f, 0.50f, AUSP_SPLASH_ACCENT);

        C3D_FrameEnd(0);
        frames++;
    }

    C2D_TextBufDelete(buf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}
