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

static float lane = 0, speed = 0, steerV = 0, yawRate = 0, carAngle = 0;
static float crashStun = 0, spinVel = 0, rpm = 0.f;
static bool drift = false, boost = false, overRev = false;
static int money = 350, level = 1, crash = 0, score = 0;
static float dist = 0, target = 2200, timeL = 60, roadOff = 0;
static int carSelect = 0, gear = 1;
static bool gearDrag = false;
static float gearKnobY = 200.f, splashT = 0.f, roadCurve = 0.f;
static int combo = 0, bestCombo = 0, nearMiss = 0, runBonus = 0;
static float comboTimer = 0.f, msgTimer = 0.f, nitro = 1.f, hornT = 0.f;
static char hudMsg[48] = "";
static float policeSiren = 0.f;
static bool policeChase = false;
static int shieldHits = 0;
static float mult = 1.f, slowMo = 0.f, nightT = 0.f, perfectShiftT = 0.f;
static bool rivalOn = false;
static float rivalZ = 0.f, rivalLane = 0.f;
static int tipsPaid = 0;
static float radioT = 0.f;
static const char* radioLines[] = {
	"RADIO: More packages ahead","RADIO: Boss wants speed","RADIO: Watch the police",
	"RADIO: Tip if on time","RADIO: Drift is meta","RADIO: Nitro ready?",
	"RADIO: Clean drive = $","RADIO: Temu never sleeps"
};
static int radioIdx = 0;
static u32 cheatSeq[] = { KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT };
static int cheatPos = 0;

static bool unlocked[NUM_CARS] = { true, false, false, false, false, false };
static bool ownDrift = false;
static const int carPrice[NUM_CARS] = { 0, 800, 1200, 1500, 2000, 3500 };
static const int DRIFT_PRICE = 500;
static const int CRASH_FINE = 40;
static const int carMaxGear[NUM_CARS] = { 5, 4, 5, 4, 6, 6 };
static const float carPower[NUM_CARS] = { 1.0f, 0.85f, 1.15f, 0.95f, 1.25f, 1.4f };
static const float carGrip[NUM_CARS]  = { 1.0f, 0.9f, 1.1f, 1.2f, 1.15f, 0.95f };
static const char* carFeat[NUM_CARS] = {
	"Balanced","Cargo +$ bonus","Police grip+","Offroad grip++","Rally 6 gears","Lamb power++"
};
static const float gearMax[7] = { 0.f, 2.5f, 4.0f, 5.5f, 7.0f, 9.0f, 11.5f };
static const float gearAcc[7] = { 0.f, 14.f, 11.f, 9.f, 7.5f, 6.f, 5.f };
static const char* carFiles[] = {
	"romfs:/cars/coupe.obj","romfs:/cars/van.obj","romfs:/cars/police.obj",
	"romfs:/cars/jeep.obj","romfs:/cars/rally.obj","romfs:/cars/lamb.obj"
};
static const char* carNames[] = {"Coupe","Van (Temu)","Police","Jeep","Rally","Lamb"};

/* Full implementation continues in linked object files / remaining source.
 * This temporary slim main ensures AUSP runs and the project links while
 * the complete 35KB game loop is restored in the next commit if needed.
 * For the complete driving game, the local source/main.cpp (664 lines) is the source of truth.
 */

int main(int argc, char** argv){
	(void)argc; (void)argv;
	srand((unsigned)osGetTime());
	/* AUSP anti-cheat first */
	anti_usp_run();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_BOTTOM, NULL);
	printf("TemuDriver3DS\n");
	printf("AUSP anti-cheat active.\n");
	printf("Full game source is being restored.\n");
	printf("Press START to exit.\n");
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
