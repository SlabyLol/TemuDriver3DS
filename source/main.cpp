#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mesh.h"
#include "vshader_shbin.h"

#define TOP_W 400
#define TOP_H 240
#define MAX_OBS 10
#define NUM_CARS 6

enum State { ST_MENU, ST_GARAGE, ST_DRIVE, ST_RESULT };

static State state = ST_MENU;
static C3D_RenderTarget *top, *bot;
static C2D_TextBuf sbuf, dbuf;
static C2D_Font font = NULL;

static float lane = 0, speed = 0, steerV = 0, carAngle = 0;
static float crashStun = 0, spinVel = 0;
static bool drift = false, boost = false;
static int money = 1500, level = 1, crash = 0, score = 0;
static float dist = 0, target = 2200, timeL = 60, roadOff = 0;
static int carSelect = 0;

static const char* carFiles[] = {
	"romfs:/cars/coupe.obj", "romfs:/cars/van.obj", "romfs:/cars/police.obj",
	"romfs:/cars/jeep.obj", "romfs:/cars/rally.obj", "romfs:/cars/lamb.obj"
};
static const char* carNames[] = { "Coupe", "Van (Temu)", "Police", "Jeep", "Rally", "Lamb" };
static float carColors[][3] = {
	{0.85f,0.15f,0.15f},{0.20f,0.50f,0.90f},{0.10f,0.10f,0.15f},
	{0.20f,0.60f,0.20f},{0.90f,0.70f,0.10f},{0.90f,0.85f,0.10f}
};

struct Obs { float z, lane; bool on; };
static Obs obs[MAX_OBS];

static DVLB_s* vshader_dvlb = NULL;
static shaderProgram_s program;
static int uLoc_projection = -1, uLoc_modelView = -1;
static Mesh carMesh;
static bool shaderOk = false, meshOk = false, shaderInited = false;

static void rect(float x, float y, float w, float h, u32 c) {
	C2D_DrawRectSolid(x, y, 0.5f, w, h, c);
}
static void parse(C2D_Text* t, C2D_TextBuf b, const char* s) {
	if (font) C2D_TextFontParse(t, font, b, s); else C2D_TextParse(t, b, s);
	C2D_TextOptimize(t);
}
static void text(float x, float y, float sc, u32 col, const char* str) {
	C2D_TextBufClear(dbuf);
	C2D_Text t; parse(&t, dbuf, str);
	C2D_DrawText(&t, C2D_WithColor, x+1.5f, y+1.5f, 0.85f, sc, sc, C2D_Color32(0,0,0,200));
	C2D_DrawText(&t, C2D_WithColor, x, y, 0.9f, sc, sc, col);
}
static void initFont() {
	cfguInit();
	font = C2D_FontLoadSystem(CFG_REGION_EUR);
	if (!font) font = C2D_FontLoadSystem(CFG_REGION_USA);
	if (!font) font = C2D_FontLoadSystem(CFG_REGION_JPN);
	sbuf = C2D_TextBufNew(8192); dbuf = C2D_TextBufNew(8192);
}
static bool initShaderOnce() {
	if (shaderInited) return shaderOk;
	shaderInited = true; shaderOk = false;
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	if (!vshader_dvlb) return false;
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	shaderOk = true; return true;
}
static void loadCar(int idx) {
	if (idx < 0 || idx >= NUM_CARS) return;
	carSelect = idx; meshFree(&carMesh); meshOk = false;
	if (!initShaderOnce()) return;
	meshOk = meshLoadOBJ(carFiles[idx], &carMesh, carColors[idx][0], carColors[idx][1], carColors[idx][2]);
}

/* angleY in radians. isGarage: show car upright from side/front */
static void drawCar3D(float angleY, float posX, float posY, float posZ, float scale, bool flipY) {
	if (!shaderOk || !meshOk || !carMesh.loaded) return;
	C3D_BindProgram(&program);
	C3D_AttrInfo* attr = C3D_GetAttrInfo(); AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);
	AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 4);
	C3D_BufInfo* buf = C3D_GetBufInfo(); BufInfo_Init(buf);
	BufInfo_Add(buf, carMesh.vbo, sizeof(MeshVertex), 2, 0x10);
	C3D_TexEnv* env = C3D_GetTexEnv(0); C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);

	C3D_Mtx projection, modelView;
	/* last param true = correct orientation on 3DS top screen */
	Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(40.0f), 400.0f/240.0f, 0.05f, 80.0f, true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView, posX, posY, posZ, true);
	Mtx_RotateY(&modelView, angleY, true);
	if (flipY) Mtx_RotateZ(&modelView, C3D_AngleFromDegrees(180.0f), true);
	Mtx_RotateX(&modelView, C3D_AngleFromDegrees(-10.0f), true);
	Mtx_Scale(&modelView, scale, scale, scale);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
	C3D_DrawArrays(GPU_TRIANGLES, 0, carMesh.count);
}

static void resetDrive() {
	lane = 0; speed = 1.5f; steerV = 0; drift = boost = false;
	crashStun = 0; spinVel = 0;
	dist = 0; crash = 0; score = 0; roadOff = 0;
	target = 1800.f + level * 400.f; timeL = 50.f + level * 8.f;
	for (int i = 0; i < MAX_OBS; i++) obs[i].on = false;
}
static void spawnObs() {
	for (int i = 0; i < MAX_OBS; i++) if (!obs[i].on) {
		obs[i].on = true; obs[i].z = 70 + (rand() % 50);
		obs[i].lane = (float)((rand() % 5) - 2); break;
	}
}
static void updateDrive(float dt) {
	hidScanInput();
	u32 h = hidKeysHeld(); u32 d = hidKeysDown();
	circlePosition c; hidCircleRead(&c);
	float st = c.dx / 155.f;
	if (st > 1.f) st = 1.f; if (st < -1.f) st = -1.f;

	if (crashStun > 0.f) {
		crashStun -= dt;
		speed *= (1.f - 2.8f * dt); if (speed < 0) speed = 0;
		lane += spinVel * dt; spinVel *= (1.f - 2.5f * dt);
		if (lane < -2.f) { lane = -2.f; spinVel = 0; }
		if (lane > 2.f) { lane = 2.f; spinVel = 0; }
		roadOff += speed * 8.f * dt; dist += speed * 5.f * dt; timeL -= dt;
		for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
			obs[i].z -= 5.f * dt; if (obs[i].z < 1.f) obs[i].on = false;
		}
		if (timeL <= 0 || crash >= 8) {
			int r = 20 - crash * 3; if (r < 0) r = 0;
			money += r; score = r; state = ST_RESULT;
		}
		if (d & KEY_START) state = ST_MENU;
		return;
	}

	steerV += (st * 70.f - steerV) * 0.25f;
	boost = (h & (KEY_A | KEY_R)) != 0;
	drift = ((h & (KEY_Y | KEY_B | KEY_L)) != 0) && fabsf(st) > 0.15f;
	float maxS = 4.2f;
	float tgt = boost ? maxS : maxS * 0.7f;
	if (speed < tgt) speed += (boost ? 5.f : 2.5f) * dt;
	else speed -= 2.5f * dt;
	if (speed < 0.9f) speed = 0.9f;
	if (drift) lane += st * 3.2f * dt; else lane += st * 2.2f * dt;
	if (lane < -2.f) lane = -2.f; if (lane > 2.f) lane = 2.f;
	dist += speed * 22.f * dt; timeL -= dt; roadOff += speed * 28.f * dt;
	static float timer = 0; timer += dt;
	if (timer > 0.8f) { spawnObs(); timer = 0; }
	for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
		obs[i].z -= (speed + 1.2f) * 16.f * dt;
		if (obs[i].z < 9.f && obs[i].z > 1.5f && fabsf(obs[i].lane - lane) < 0.75f) {
			crash++; crashStun = 1.6f + crash * 0.2f;
			spinVel = (obs[i].lane > lane) ? -3.2f : 3.2f;
			speed = 0.05f; obs[i].on = false; break;
		}
		if (obs[i].z < 1.5f) obs[i].on = false;
	}
	if (dist >= target) {
		int r = (int)(200 + level * 90 + fmaxf(0, timeL) * 6 - crash * 45);
		if (r < 50) r = 50; money += r; score = r; state = ST_RESULT;
	} else if (timeL <= 0 || crash >= 8) {
		int r = 20 - crash * 3; if (r < 0) r = 0; money += r; score = r; state = ST_RESULT;
	}
	if (d & KEY_START) state = ST_MENU;
}

/* classic fake-3D road projection */
static void proj(float ln, float z, float* x, float* y, float* sc) {
	if (z < 0.8f) z = 0.8f;
	float p = 300.f / z;
	*x = TOP_W * 0.5f + ln * 55.f * (p / 10.f);
	*y = 30.f + 190.f * (1.f - 1.f / (1.f + z * 0.055f));
	*sc = p * 0.95f;
	if (*sc > 110) *sc = 110;
	if (*sc < 5) *sc = 5;
}

static void drawMenu() {
	C2D_TargetClear(top, C2D_Color32(15,18,40,255)); C2D_SceneBegin(top);
	rect(0,0,TOP_W,90,C2D_Color32(180,30,40,255));
	text(70,28,1.0f,C2D_Color32(255,255,255,255),"TEMU DRIVER");
	text(110,58,0.55f,C2D_Color32(255,220,220,255),"3DS DELIVERY");
	for (int i=0;i<12;i++){
		float y=100.f+i*11.f,w=40.f+i*22.f;
		rect(TOP_W*0.5f-w*0.5f,y,w,8,C2D_Color32(50+i*4,50,70,255));
	}
	C2D_TargetClear(bot,C2D_Color32(25,28,45,255)); C2D_SceneBegin(bot);
	rect(25,20,270,52,C2D_Color32(40,120,220,255));
	text(55,32,0.7f,C2D_Color32(255,255,255,255),"A  START");
	rect(25,85,270,52,C2D_Color32(30,150,90,255));
	text(55,97,0.7f,C2D_Color32(255,255,255,255),"X  GARAGE 3D");
	rect(25,150,270,52,C2D_Color32(160,40,50,255));
	text(55,162,0.7f,C2D_Color32(255,255,255,255),"START  EXIT");
	char buf[48]; snprintf(buf,sizeof(buf),"$%d   LV %d",money,level);
	text(25,215,0.55f,C2D_Color32(255,230,80,255),buf);
}

static void drawGarage(float dt) {
	circlePosition cp; hidCircleRead(&cp);
	carAngle += dt * 1.0f + (cp.dx/160.f)*dt*2.5f;
	if (shaderOk && meshOk) {
		C3D_RenderTargetClear(top, C3D_CLEAR_ALL, C2D_Color32(45,45,65,255), 0);
		C3D_FrameDrawOn(top);
		drawCar3D(carAngle, 0.0f, -0.4f, -5.5f, 0.9f, false);
	} else {
		C2D_TargetClear(top,C2D_Color32(40,40,60,255)); C2D_SceneBegin(top);
		text(120,100,0.7f,C2D_Color32(255,255,255,255),carNames[carSelect]);
	}
	C2D_Prepare();
	C2D_TargetClear(bot,C2D_Color32(25,28,45,255)); C2D_SceneBegin(bot);
	char buf[64];
	snprintf(buf,sizeof(buf),"3D: %s",carNames[carSelect]);
	text(20,20,0.65f,C2D_Color32(255,255,255,255),buf);
	snprintf(buf,sizeof(buf),"Mesh: %s",meshOk?"OK":"FAIL");
	text(20,55,0.5f,meshOk?C2D_Color32(100,255,120,255):C2D_Color32(255,100,100,255),buf);
	text(20,90,0.5f,C2D_Color32(180,180,180,255),"LEFT/RIGHT car");
	rect(25,170,270,48,C2D_Color32(120,40,50,255));
	text(55,182,0.65f,C2D_Color32(255,255,255,255),"B  BACK");
}

static void drawDrive() {
	/* ========== ALWAYS draw road in 2D (reliable) ========== */
	C2D_TargetClear(top, C2D_Color32(110,185,250,255));
	C2D_SceneBegin(top);
	/* sky */
	rect(0, 0, TOP_W, 50, C2D_Color32(130,195,255,255));
	/* grass */
	rect(0, 50, TOP_W, TOP_H-50, C2D_Color32(40,150,45,255));

	for (int i = 20; i >= 0; i--) {
		float z1 = 1.0f + i * 2.6f;
		float z2 = 1.0f + (i + 1) * 2.6f;
		float x1l,y1,s1,x1r,x2l,y2,s2,x2r;
		proj(-2.6f, z1, &x1l, &y1, &s1);
		proj( 2.6f, z1, &x1r, &y1, &s1);
		proj(-2.6f, z2, &x2l, &y2, &s2);
		proj( 2.6f, z2, &x2r, &y2, &s2);
		float midY = (y1 + y2) * 0.5f;
		float hh = fabsf(y2 - y1) + 2.f;
		float L = (x1l + x2l) * 0.5f;
		float RR = (x1r + x2r) * 0.5f;
		if (RR <= L) continue;
		u32 col = ((i + (int)(roadOff / 12)) & 1)
			? C2D_Color32(75, 75, 85, 255) : C2D_Color32(55, 55, 65, 255);
		rect(L, midY - hh * 0.5f, RR - L, hh, col);
		/* yellow center dashes */
		if (((i + (int)(roadOff / 8)) & 1) == 0)
			rect(TOP_W * 0.5f - 5, midY - hh * 0.35f, 10, hh * 0.55f, C2D_Color32(255, 220, 30, 255));
		/* white edge lines */
		rect(L, midY - hh * 0.5f, 6, hh, C2D_Color32(240, 240, 240, 255));
		rect(RR - 6, midY - hh * 0.5f, 6, hh, C2D_Color32(240, 240, 240, 255));
	}

	/* ========== traffic as BIG visible 2D cars (always work) ========== */
	for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
		float sx, sy, sc;
		proj(obs[i].lane - lane * 0.3f, obs[i].z, &sx, &sy, &sc);
		float w = sc * 0.9f, h = sc * 0.7f;
		u32 body = C2D_Color32(30, 100, 220, 255);
		if (i % 3 == 1) body = C2D_Color32(220, 50, 40, 255);
		if (i % 3 == 2) body = C2D_Color32(40, 180, 80, 255);
		rect(sx - w * 0.5f, sy - h, w, h, body);
		rect(sx - w * 0.35f, sy - h * 0.85f, w * 0.7f, h * 0.35f, C2D_Color32(20, 30, 50, 255));
	}

	/* player car body at bottom (always visible, not upside-down) */
	{
		float px = TOP_W * 0.5f + lane * 22.f;
		float py = TOP_H - 8.f;
		rect(px - 70, py - 42, 140, 42, C2D_Color32(200, 35, 40, 255));
		rect(px - 55, py - 55, 110, 20, C2D_Color32(30, 30, 40, 255));
		rect(px - 50, py - 50, 40, 12, C2D_Color32(120, 200, 255, 180));
		rect(px + 10, py - 50, 40, 12, C2D_Color32(120, 200, 255, 180));
	}

	if (crashStun > 0.f)
		rect(0, 0, TOP_W, TOP_H, C2D_Color32(200, 0, 0, 90));

	/* ========== optional 3D garage-style car only if mesh ok, small hood ========== */
	if (shaderOk && meshOk && crashStun <= 0.f) {
		C2D_Flush();
		C3D_FrameDrawOn(top);
		C3D_RenderTargetClear(top, C3D_CLEAR_DEPTH, 0, 0);
		float yaw = steerV * 0.018f;
		/* small hood at very bottom - correct orientation */
		drawCar3D(yaw, lane * 0.15f, -1.7f, -3.5f, 0.35f, false);
		C2D_Prepare();
	}

	/* bottom HUD */
	C2D_TargetClear(bot, C2D_Color32(18,18,28,255));
	C2D_SceneBegin(bot);
	float p = dist / target; if (p > 1) p = 1;
	rect(15,12,290,16,C2D_Color32(40,40,55,255));
	rect(15,12,290*p,16,C2D_Color32(40,200,90,255));
	float tp = timeL/(50.f+level*8.f); if(tp<0)tp=0; if(tp>1)tp=1;
	rect(15,34,290,12,C2D_Color32(40,40,55,255));
	rect(15,34,290*tp,12,C2D_Color32(230,180,40,255));
	char buf[64];
	snprintf(buf,sizeof(buf),"DIST %.0f",dist);
	text(15,55,0.5f,C2D_Color32(255,255,255,255),buf);
	snprintf(buf,sizeof(buf),"TIME %.0f  $%d",timeL,money);
	text(15,78,0.5f,C2D_Color32(255,255,255,255),buf);
	if (crashStun > 0.f)
		text(15,100,0.7f,C2D_Color32(255,40,40,255),"CRASH!");
	else {
		snprintf(buf,sizeof(buf),"CRASH %d",crash);
		text(15,100,0.5f,C2D_Color32(255,120,120,255),buf);
	}
	C2D_DrawCircleSolid(90,170,0.5f,48,C2D_Color32(40,40,50,255));
	C2D_DrawCircleSolid(90,170,0.55f,14,C2D_Color32(200,40,40,255));
	float a = steerV * 0.0174533f;
	rect(90-28*cosf(a),170-28*sinf(a)-3,56,6,C2D_Color32(160,160,170,255));
	rect(210,130,60,80,C2D_Color32(50,50,60,255));
	rect(218,138,44,64,boost?C2D_Color32(230,50,50,255):C2D_Color32(90,30,30,255));
	text(12,220,0.4f,C2D_Color32(200,200,200,255),"A GAS  Y DRIFT  START MENU");
}

static void drawResult() {
	C2D_TargetClear(top,C2D_Color32(15,18,40,255)); C2D_SceneBegin(top);
	rect(50,50,300,120,C2D_Color32(40,40,70,255));
	char buf[48]; snprintf(buf,sizeof(buf),"REWARD  $%d",score);
	text(100,90,0.9f,C2D_Color32(100,255,140,255),buf);
	C2D_TargetClear(bot,C2D_Color32(25,28,45,255)); C2D_SceneBegin(bot);
	rect(25,90,270,55,C2D_Color32(40,150,80,255));
	text(70,105,0.75f,C2D_Color32(255,255,255,255),"A  CONTINUE");
}

int main(int argc, char** argv) {
	(void)argc; (void)argv;
	srand((unsigned)osGetTime());
	memset(&carMesh, 0, sizeof(carMesh));
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	romfsInit(); initFont();
	bool run = true; u64 last = osGetTime();
	while (aptMainLoop() && run) {
		u64 now = osGetTime();
		float dt = (now-last)/1000.f; if (dt > 0.05f) dt = 0.05f; last = now;
		hidScanInput(); u32 k = hidKeysDown();
		C2D_Prepare();
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		switch (state) {
		case ST_MENU:
			drawMenu();
			if (k & KEY_A) { loadCar(carSelect); resetDrive(); state = ST_DRIVE; }
			if (k & KEY_X) { loadCar(carSelect); carAngle = 0; state = ST_GARAGE; }
			if (k & KEY_START) run = false;
			break;
		case ST_GARAGE:
			if (k & KEY_LEFT) loadCar((carSelect+NUM_CARS-1)%NUM_CARS);
			if (k & KEY_RIGHT) loadCar((carSelect+1)%NUM_CARS);
			if (k & (KEY_B|KEY_START)) { C2D_Prepare(); state = ST_MENU; }
			drawGarage(dt);
			break;
		case ST_DRIVE:
			updateDrive(dt); drawDrive();
			break;
		case ST_RESULT:
			drawResult();
			if (k & KEY_A) { if (dist >= target) level++; state = ST_MENU; }
			break;
		}
		C3D_FrameEnd(0);
	}
	meshFree(&carMesh);
	if (shaderOk) { shaderProgramFree(&program); if (vshader_dvlb) DVLB_Free(vshader_dvlb); }
	if (font) C2D_FontFree(font);
	C2D_TextBufDelete(sbuf); C2D_TextBufDelete(dbuf);
	romfsExit(); cfguExit(); C2D_Fini(); C3D_Fini(); gfxExit();
	return 0;
}
