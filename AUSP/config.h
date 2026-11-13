#ifndef AUSP_CONFIG_H
#define AUSP_CONFIG_H

#define AUSP_VERSION_MAJOR      1
#define AUSP_VERSION_MINOR      4
#define AUSP_VERSION_PATCH      0

#define AUSP_REACTION_SCREEN    1
#define AUSP_REACTION_ACTION    2
#define AUSP_REACTION_BOTH      3

#define AUSP_REACTION_MODE      AUSP_REACTION_BOTH

#define AUSP_TITLE              "TEMU DRIVER 3DS"
#define AUSP_SUBTITLE           "CHEAT PLUGIN DETECTED"
#define AUSP_LINE1              "A plugin / cheat / NTR / 3GX was found."
#define AUSP_LINE2              "Temu wages are low enough already."
#define AUSP_LINE3              "Remove all plugins and the plugin loader,"
#define AUSP_LINE4              "then restart your 3DS."
#define AUSP_LINE5              "(NTR / Luma Plugin Loader / 3GX / CTRPF)"
#define AUSP_FOOTER             "Press any button to exit - no cheats allowed"

#define AUSP_EXIT_ON_KEY        1
#define AUSP_FORCE_EXIT         1

#define AUSP_SCAN_CODE          1
#define AUSP_SCAN_DATA          1
#define AUSP_MAX_REGIONS        48
#define AUSP_MIN_REGION_SIZE    0x1000

#define AUSP_SCREEN_TOP         1
#define AUSP_SCREEN_BOTTOM      0

#define AUSP_SPLASH_ENABLE      1
#define AUSP_SPLASH_DURATION    90

#define AUSP_SPLASH_BG          C2D_Color32(12, 14, 28, 255)
#define AUSP_SPLASH_ACCENT      C2D_Color32(255, 80, 40, 255)
#define AUSP_SPLASH_TEXT        C2D_Color32(240, 245, 255, 255)
#define AUSP_SPLASH_SUB         C2D_Color32(200, 160, 140, 255)

#define AUSP_IGNORE_NIMBUS      1

#endif
