#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mesh.h"
#include "vshader_shbin.h"
#include "anti_usp.h"

#define TOP_W 400
#define TOP_H 240
#define MAX_OBS 8
#define MAX_PKG 5
#define MAX_GATE 3
#define MAX_PAD 4
#define MAX_SHIELD_PK 2
#define NUM_CARS 6
#define NUM_TRAFFIC 8
#define PI 3.14159265f
#define DEG2RAD (PI/180.f)
#define MAX_STEER_DEG 25.f
#define SAVE_PATH "sdmc:/TemuDriver3DS.sav"
#define SAVE_MAGIC 0x54444D31u

enum State { ST_SPLASH, ST_MENU, ST_GARAGE, ST_SHOP, ST_DRIVE, ST_RESULT, ST_GAMEOVER, ST_ADMIN };

static State state = ST_SPLASH;
static C3D_RenderTarget *top, *bot;
static C2D_TextBuf sbuf, dbuf;
static C2D_Font font = NULL;

/* NOTE: Full game source was truncated in this push attempt - will follow with complete file */
int main(int argc, char** argv){
	(void)argc; (void)argv;
	anti_usp_run();
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	printf("TemuDriver3DS - main restored partially, full push next\n");
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
	}
	gfxExit();
	return 0;
}
