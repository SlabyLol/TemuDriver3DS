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
#define MAX_OBS 8
#define MAX_PKG 5
#define MAX_GATE 3
#define NUM_CARS 6
#define NUM_TRAFFIC 8
#define PI 3.14159265f
#define DEG2RAD (PI/180.f)
#define MAX_STEER_DEG 25.f
#define SAVE_PATH "sdmc:/TemuDriver3DS.sav"
#define SAVE_MAGIC 0x54444D31u

enum State { ST_SPLASH, ST_MENU, ST_GARAGE, ST_SHOP, ST_DRIVE, ST_RESULT, ST_GAMEOVER };

static State state = ST_SPLASH;
static C3D_RenderTarget *top, *bot;
static C2D_TextBuf sbuf, dbuf;
static C2D_Font font = NULL;

static float lane = 0, speed = 0, steerV = 0, yawRate = 0, carAngle = 0;
static float crashStun = 0, spinVel = 0;
static float rpm = 0.f;
static bool drift = false, boost = false, overRev = false;
static int money = 350, level = 1, crash = 0, score = 0;
static float dist = 0, target = 2200, timeL = 60, roadOff = 0;
static int carSelect = 0;
static int gear = 1;
static bool gearDrag = false;
static float gearKnobY = 200.f;
static float splashT = 0.f;
static float roadCurve = 0.f;
static int combo = 0, bestCombo = 0, nearMiss = 0, runBonus = 0;
static float comboTimer = 0.f, msgTimer = 0.f, nitro = 1.f, hornT = 0.f;
static char hudMsg[48] = "";
static float policeSiren = 0.f;
static bool policeChase = false;

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
static float carColors[][3] = {
	{0.85f,0.15f,0.15f},{0.2f,0.5f,0.9f},{0.1f,0.1f,0.15f},
	{0.2f,0.6f,0.2f},{0.9f,0.7f,0.1f},{0.9f,0.85f,0.1f}
};

static const char* trafficFiles[NUM_TRAFFIC] = {
	"romfs:/cars/coupe.obj","romfs:/cars/police.obj","romfs:/cars/jeep.obj","romfs:/cars/rally.obj",
	"romfs:/cars/lamb.obj","romfs:/cars/armor.obj","romfs:/cars/fenyr.obj","romfs:/cars/mobil.obj"
};
static float trafficColors[NUM_TRAFFIC][3] = {
	{0.85f,0.15f,0.15f},{0.1f,0.1f,0.2f},{0.2f,0.55f,0.25f},{0.9f,0.7f,0.1f},
	{0.9f,0.85f,0.15f},{0.4f,0.4f,0.45f},{0.7f,0.2f,0.2f},{0.15f,0.35f,0.7f}
};

struct Obs { float z, lane, spd; int meshId; bool on; };
static Obs obs[MAX_OBS];
struct Pkg { float z, lane; bool on; int worth; };
static Pkg pkgs[MAX_PKG];
struct Gate { float z; bool on; };
static Gate gates[MAX_GATE];

static DVLB_s* vshader_dvlb = NULL;
static shaderProgram_s program;
static int uLoc_projection = -1, uLoc_modelView = -1;
static Mesh carMesh, groundMesh, buildMesh;
static Mesh trafficMeshes[NUM_TRAFFIC];
static bool trafficOk[NUM_TRAFFIC];
static bool shaderOk = false, meshOk = false, groundOk = false, buildOk = false, shaderInited = false;
static bool trafficLoaded = false;

static ndspWaveBuf waveBuf;
static s16* audioBuf = NULL;
static bool soundOk = false;
static float enginePhase = 0.f;

struct SaveData {
	u32 magic; int money, level, carSelect;
	u8 unlocked[NUM_CARS]; u8 ownDrift; u8 pad[3];
};

static void saveGame(void){
	SaveData s; memset(&s, 0, sizeof(s));
	s.magic = SAVE_MAGIC; s.money = money; s.level = level; s.carSelect = carSelect;
	for(int i=0;i<NUM_CARS;i++) s.unlocked[i] = unlocked[i] ? 1 : 0;
	s.ownDrift = ownDrift ? 1 : 0;
	FILE* f = fopen(SAVE_PATH, "wb"); if(!f) return;
	fwrite(&s, sizeof(s), 1, f); fclose(f);
}
static void loadGame(void){
	FILE* f = fopen(SAVE_PATH, "rb"); if(!f) return;
	SaveData s;
	if(fread(&s, sizeof(s), 1, f) == 1 && s.magic == SAVE_MAGIC){
		money = s.money; if(money < 0) money = 0;
		level = s.level; if(level < 1) level = 1;
		carSelect = s.carSelect; if(carSelect < 0 || carSelect >= NUM_CARS) carSelect = 0;
		for(int i=0;i<NUM_CARS;i++) unlocked[i] = s.unlocked[i] != 0;
		unlocked[0] = true; ownDrift = s.ownDrift != 0;
	}
	fclose(f);
}

static void rect(float x,float y,float w,float h,u32 c){ C2D_DrawRectSolid(x,y,0.5f,w,h,c); }
static void parse(C2D_Text* t,C2D_TextBuf b,const char* s){
	if(font) C2D_TextFontParse(t,font,b,s); else C2D_TextParse(t,b,s); C2D_TextOptimize(t);
}
static void text(float x,float y,float sc,u32 col,const char* str){
	C2D_TextBufClear(dbuf); C2D_Text t; parse(&t,dbuf,str);
	C2D_DrawText(&t,C2D_WithColor,x+1.5f,y+1.5f,0.85f,sc,sc,C2D_Color32(0,0,0,200));
	C2D_DrawText(&t,C2D_WithColor,x,y,0.9f,sc,sc,col);
}
static void initFont(){
	cfguInit();
	font=C2D_FontLoadSystem(CFG_REGION_EUR);
	if(!font) font=C2D_FontLoadSystem(CFG_REGION_USA);
	if(!font) font=C2D_FontLoadSystem(CFG_REGION_JPN);
	sbuf=C2D_TextBufNew(8192); dbuf=C2D_TextBufNew(8192);
}
static void initSound(){
	soundOk = false;
	if(ndspInit() != 0) return;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, 22050.0f);
	ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
	const int samples = 2048;
	audioBuf = (s16*)linearAlloc(samples * sizeof(s16));
	if(!audioBuf) return;
	memset(audioBuf, 0, samples * sizeof(s16));
	memset(&waveBuf, 0, sizeof(waveBuf));
	waveBuf.data_vaddr = audioBuf; waveBuf.nsamples = samples;
	ndspChnWaveBufAdd(0, &waveBuf); soundOk = true;
}
static void updateEngineSound(float spd, bool gas, bool over){
	if(!soundOk || !audioBuf) return;
	if(waveBuf.status != NDSP_WBUF_DONE && waveBuf.status != NDSP_WBUF_FREE) return;
	float freq = 40.f + spd * 35.f + (gas ? 25.f : 0.f) + (over ? 80.f : 0.f);
	float vol = (spd < 0.15f && !gas) ? 0.f : (0.12f + spd * 0.04f + (over ? 0.15f : 0.f));
	if(vol > 0.55f) vol = 0.55f;
	int n = waveBuf.nsamples;
	for(int i=0;i<n;i++){
		enginePhase += freq * (2.f * PI / 22050.f);
		if(enginePhase > 2.f*PI) enginePhase -= 2.f*PI;
		float s = (sinf(enginePhase) > 0.f) ? 1.f : -1.f;
		s += 0.3f * sinf(enginePhase * 2.f);
		if(over) s += 0.4f * sinf(enginePhase * 5.f);
		audioBuf[i] = (s16)(s * vol * 12000.f);
	}
	DSP_FlushDataCache(audioBuf, n * sizeof(s16));
	ndspChnWaveBufAdd(0, &waveBuf);
}
static void exitSound(){
	if(soundOk){ ndspChnWaveBufClear(0); if(audioBuf) linearFree(audioBuf); audioBuf=NULL; ndspExit(); soundOk=false; }
}
static bool initShaderOnce(){
	if(shaderInited) return shaderOk;
	shaderInited=true; shaderOk=false;
	vshader_dvlb=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);
	if(!vshader_dvlb) return false;
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program,&vshader_dvlb->DVLE[0]);
	uLoc_projection=shaderInstanceGetUniformLocation(program.vertexShader,"projection");
	uLoc_modelView=shaderInstanceGetUniformLocation(program.vertexShader,"modelView");
	shaderOk=true; return true;
}
static void loadCar(int idx){
	if(idx<0||idx>=NUM_CARS) return;
	carSelect=idx; meshFree(&carMesh); meshOk=false;
	if(!initShaderOnce()) return;
	meshOk=meshLoadOBJ(carFiles[idx],&carMesh,carColors[idx][0],carColors[idx][1],carColors[idx][2]);
}
static void loadTrafficMeshes(void){
	if(trafficLoaded) return;
	if(!initShaderOnce()) return;
	for(int i=0;i<NUM_TRAFFIC;i++){
		memset(&trafficMeshes[i], 0, sizeof(Mesh));
		trafficOk[i] = meshLoadOBJ(trafficFiles[i], &trafficMeshes[i],
			trafficColors[i][0], trafficColors[i][1], trafficColors[i][2]);
	}
	trafficLoaded = true;
}
static void loadWorldMeshes(){
	if(!initShaderOnce()) return;
	meshFree(&groundMesh); meshFree(&buildMesh);
	groundOk = meshLoadOBJ("romfs:/city/road-straight.obj", &groundMesh, 0.35f, 0.35f, 0.4f);
	if(!groundOk) groundOk = meshLoadCube(&groundMesh, 0.3f, 0.3f, 0.35f);
	buildOk = meshLoadOBJ("romfs:/city/low-detail-building-d.obj", &buildMesh, 0.7f, 0.45f, 0.35f);
	if(!buildOk) buildOk = meshLoadCube(&buildMesh, 0.65f, 0.4f, 0.35f);
	loadTrafficMeshes();
}
static void drawMesh3D(Mesh* m, float angleY, float px, float py, float pz, float sx, float sy, float sz){
	if(!shaderOk || !m || !m->loaded) return;
	C3D_BindProgram(&program);
	C3D_AttrInfo* attr = C3D_GetAttrInfo(); AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3); AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 4);
	C3D_BufInfo* buf = C3D_GetBufInfo(); BufInfo_Init(buf);
	BufInfo_Add(buf, m->vbo, sizeof(MeshVertex), 2, 0x10);
	C3D_TexEnv* env = C3D_GetTexEnv(0); C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	C3D_CullFace(GPU_CULL_NONE); C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	C3D_Mtx projection, modelView;
	Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(55.0f), 400.0f/240.0f, 0.2f, 100.0f, false);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView, px, py, pz, true);
	Mtx_RotateY(&modelView, angleY, true);
	Mtx_Scale(&modelView, sx, sy, sz);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
	C3D_DrawArrays(GPU_TRIANGLES, 0, m->count);
}

static void setMsg(const char* m){ snprintf(hudMsg,sizeof(hudMsg),"%s",m); msgTimer=1.4f; }
static void addCombo(int n){
	combo += n; if(combo > bestCombo) bestCombo = combo;
	comboTimer = 3.5f; runBonus += 2 + combo;
	if(combo >= 5) setMsg("COMBO x5!");
	else if(combo >= 3) setMsg("NICE!");
}
static void spawnPkg(){
	for(int i=0;i<MAX_PKG;i++) if(!pkgs[i].on){
		pkgs[i].on=true; pkgs[i].z=70+(rand()%40);
		pkgs[i].lane=(float)((rand()%5)-2); pkgs[i].worth=5+(rand()%15); break;
	}
}
static void spawnGate(){
	for(int i=0;i<MAX_GATE;i++) if(!gates[i].on){
		gates[i].on=true; gates[i].z=90+(rand()%30); break;
	}
}
static void resetDrive(){
	lane=0; speed=0.f; steerV=0; yawRate=0; rpm=0; roadCurve=0;
	drift=boost=overRev=false; crashStun=0; spinVel=0;
	gear=1; gearDrag=false; gearKnobY=200.f;
	dist=0; crash=0; score=0; roadOff=0;
	target=1800.f+level*400.f; timeL=55.f+level*8.f;
	for(int i=0;i<MAX_OBS;i++) obs[i].on=false;
	for(int i=0;i<MAX_PKG;i++) pkgs[i].on=false;
	for(int i=0;i<MAX_GATE;i++) gates[i].on=false;
	combo=0; nearMiss=0; runBonus=0; comboTimer=0; msgTimer=0; nitro=1.f;
	policeChase=false; policeSiren=0; hudMsg[0]=0;
}
static void spawnObs(){
	int active=0;
	for(int i=0;i<MAX_OBS;i++) if(obs[i].on) active++;
	if(active >= 4) return;
	for(int i=0;i<MAX_OBS;i++) if(!obs[i].on){
		obs[i].on=true; obs[i].z=60+(rand()%50);
		obs[i].lane=(float)((rand()%5)-2);
		obs[i].spd=0.2f+(rand()%40)/100.f;
		obs[i].meshId = rand() % NUM_TRAFFIC; break;
	}
}
static float gearYFromNum(int g){ return 210.f - (float)(g - 1) * (100.f / 5.f); }
static int gearFromY(float y){
	int g = 1 + (int)((210.f - y) / (100.f / 5.f) + 0.5f);
	if(g < 1) g = 1; if(g > 6) g = 6; return g;
}
static void handleGearTouch(){
	touchPosition t; hidTouchRead(&t);
	u32 h = hidKeysHeld(); u32 d = hidKeysDown(); u32 u = hidKeysUp();
	int maxG = carMaxGear[carSelect];
	if(d & KEY_TOUCH){
		if(t.px >= 245 && t.px <= 315 && t.py >= 100 && t.py <= 220) gearDrag = true;
	}
	if(u & KEY_TOUCH) gearDrag = false;
	if(gearDrag && (h & KEY_TOUCH)){
		gearKnobY = (float)t.py;
		if(gearKnobY < 110.f) gearKnobY = 110.f;
		if(gearKnobY > 210.f) gearKnobY = 210.f;
		gear = gearFromY(gearKnobY);
		if(gear > maxG) gear = maxG;
	} else {
		if(gear > maxG) gear = maxG;
		gearKnobY = gearYFromNum(gear);
	}
}
static void doCrashFine(void){
	money -= CRASH_FINE; if(money < 0) money = 0;
	saveGame();
	if(money <= 0){ score = 0; state = ST_GAMEOVER; }
}

static void updateDrive(float dt){
	hidScanInput();
	u32 h=hidKeysHeld(); u32 d=hidKeysDown();
	circlePosition c; hidCircleRead(&c);
	float stick=c.dx/155.f;
	if(stick>1.f) stick=1.f; if(stick<-1.f) stick=-1.f;
	handleGearTouch();

	if(crashStun>0.f){
		crashStun-=dt; speed*=(1.f-2.5f*dt); if(speed<0) speed=0;
		lane+=spinVel*dt; spinVel*=(1.f-2.f*dt);
		if(lane<-2.2f){lane=-2.2f;spinVel=0;} if(lane>2.2f){lane=2.2f;spinVel=0;}
		roadOff+=speed*5.f*dt; dist+=speed*3.f*dt; timeL-=dt;
		updateEngineSound(speed, false, false);
		for(int i=0;i<MAX_OBS;i++) if(obs[i].on){
			obs[i].z -= (speed + 0.5f) * 10.f * dt;
			if(obs[i].z<1.f) obs[i].on=false;
		}
		if(timeL<=0||crash>=6){
			int r=3-crash*2; if(r<0)r=0; r+=runBonus/2; money+=r; score=r; saveGame();
			state = (money<=0) ? ST_GAMEOVER : ST_RESULT;
		}
		if(d&KEY_START){ saveGame(); state=ST_MENU; }
		return;
	}

	boost = (h & (KEY_A | KEY_R)) != 0;
	drift = ownDrift && ((h & (KEY_Y | KEY_L)) != 0) && fabsf(stick) > 0.2f && speed > 2.0f;
	bool braking = (h & KEY_B) != 0;

	int maxG = carMaxGear[carSelect];
	if(gear > maxG) gear = maxG;
	float maxS = gearMax[gear] * carPower[carSelect];
	float acc = gearAcc[gear] * carPower[carSelect];

	rpm = (maxS > 0.1f) ? (speed / maxS) : 0.f;
	if(rpm > 1.f) rpm = 1.f;
	overRev = boost && rpm > 0.92f && gear < maxG;
	if(overRev){ acc *= 0.35f; speed -= 2.5f * dt; lane += ((rand()%3)-1) * 0.4f * dt; }

	float force = 0.f;
	if(boost) force += acc;
	force -= 2.0f + speed * 0.35f;
	if(braking) force -= 16.f;
	if(drift) force -= 3.f;
	speed += force * dt * 0.32f;
	if(speed > maxS * 1.05f) speed = maxS * 1.05f;
	if(speed < 0.f) speed = 0.f;

	float targetSteerDeg = stick * MAX_STEER_DEG;
	steerV += (targetSteerDeg - steerV) * 12.f * dt;
	if(steerV > MAX_STEER_DEG) steerV = MAX_STEER_DEG;
	if(steerV < -MAX_STEER_DEG) steerV = -MAX_STEER_DEG;

	float grip = (drift ? 0.32f : 1.0f) * carGrip[carSelect];
	float move = stick * (0.55f + speed * 0.55f) * grip;
	if(drift) move *= 1.6f;
	lane += move * dt;
	yawRate = stick * speed * 0.15f * grip;
	if(lane < -2.1f) lane = -2.1f;
	if(lane >  2.1f) lane =  2.1f;

	roadCurve = 0.35f * sinf(roadOff * 0.04f);
	float distMul = (carSelect == 1) ? 1.15f : 1.0f;
	dist += speed * 18.f * dt * distMul;
	timeL -= dt; roadOff += speed * 20.f * dt;
	updateEngineSound(speed, boost, overRev);

	static float timer = 0; timer += dt;
	if(timer > 2.2f){ spawnObs(); if((rand()%3)==0) spawnPkg(); if((rand()%5)==0) spawnGate(); timer = 0; }

	if((h & KEY_X) && nitro > 0.15f && speed > 0.5f){
		speed += 8.f * dt; nitro -= 0.35f * dt; setMsg("NITRO!");
	} else if(nitro < 1.f) nitro += 0.08f * dt;
	if(nitro > 1.f) nitro = 1.f; if(nitro < 0.f) nitro = 0.f;

	if(d & KEY_SELECT){ hornT = 0.3f; setMsg("BEEP!"); }
	if(hornT > 0) hornT -= dt;
	if(comboTimer > 0){ comboTimer -= dt; if(comboTimer <= 0) combo = 0; }
	if(msgTimer > 0) msgTimer -= dt;

	if(!policeChase && speed > 6.f && (rand()%1200)==0){
		policeChase = true; policeSiren = 12.f; setMsg("POLICE!!!");
	}
	if(policeChase){
		policeSiren -= dt;
		if(policeSiren <= 0){ policeChase=false; setMsg("ESCAPED!"); runBonus += 25; }
		else if(speed < 3.f){ policeChase=false; money -= 30; if(money<0)money=0; setMsg("CAUGHT -$30"); }
	}

	for(int i=0;i<MAX_OBS;i++) if(obs[i].on){
		obs[i].z -= (speed - obs[i].spd + 1.2f) * 11.f * dt;
		obs[i].lane += sinf(roadOff * 0.08f + i) * 0.15f * dt;
		if(obs[i].lane < -2.f) obs[i].lane = -2.f;
		if(obs[i].lane >  2.f) obs[i].lane =  2.f;
		float dLane = fabsf(obs[i].lane - lane);
		if(obs[i].z < 12.f && obs[i].z > 2.f && dLane < 0.85f){
			crash++; crashStun = 1.6f + crash * 0.2f; combo=0; comboTimer=0;
			spinVel = (obs[i].lane > lane) ? -3.2f : 3.2f;
			speed *= 0.12f; yawRate = spinVel; obs[i].on = false;
			doCrashFine();
			if(state == ST_GAMEOVER) return;
			break;
		}
		if(obs[i].z < 14.f && obs[i].z > 8.f && dLane > 0.85f && dLane < 1.35f && speed > 2.f){
			nearMiss++; addCombo(1); setMsg("NEAR MISS!");
			obs[i].z -= 5.f;
		}
		if(obs[i].z < 1.5f) obs[i].on = false;
	}
	for(int i=0;i<MAX_PKG;i++) if(pkgs[i].on){
		pkgs[i].z -= (speed + 1.0f) * 11.f * dt;
		if(pkgs[i].z < 10.f && pkgs[i].z > 2.f && fabsf(pkgs[i].lane - lane) < 0.9f){
			money += pkgs[i].worth; runBonus += pkgs[i].worth;
			char b[32]; snprintf(b,sizeof(b),"PACKAGE +$%d", pkgs[i].worth);
			setMsg(b); pkgs[i].on=false; addCombo(1);
		}
		if(pkgs[i].z < 1.5f) pkgs[i].on=false;
	}
	for(int i=0;i<MAX_GATE;i++) if(gates[i].on){
		gates[i].z -= (speed + 1.0f) * 11.f * dt;
		if(gates[i].z < 8.f && gates[i].z > 2.f){
			if(speed > 5.f){ int g=10+(int)(speed*2); runBonus+=g; money+=g/2; setMsg("GATE BONUS!"); addCombo(2); }
			gates[i].on=false;
		}
		if(gates[i].z < 1.5f) gates[i].on=false;
	}
	if(dist >= target){
		int r = (int)(45 + level * 18 + fmaxf(0, timeL) * 1.5f - crash * 15);
		if(carSelect == 1) r = (int)(r * 1.15f);
		r += runBonus; if(r < 12) r = 12; money += r; score = r; saveGame(); state = ST_RESULT;
	} else if(timeL <= 0 || crash >= 6){
		int r = 3 - crash * 2; if(r < 0) r = 0; r += runBonus/2; money += r; score = r; saveGame();
		state = (money <= 0) ? ST_GAMEOVER : ST_RESULT;
	}
	if(d & KEY_START){ saveGame(); state = ST_MENU; }
}

static void drawSplash(){
	C2D_TargetClear(top, C2D_Color32(12, 14, 28, 255)); C2D_SceneBegin(top);
	rect(0, 70, TOP_W, 90, C2D_Color32(190, 28, 38, 255));
	text(70, 90, 1.1f, C2D_Color32(255,255,255,255), "TEMU DRIVER");
	text(110, 130, 0.5f, C2D_Color32(255,200,200,255), "v1.0");
	C2D_TargetClear(bot, C2D_Color32(12, 14, 28, 255)); C2D_SceneBegin(bot);
	text(70, 100, 0.55f, C2D_Color32(200,200,210,255), "Loading...");
}
static void drawMenu(){
	C2D_TargetClear(top,C2D_Color32(15,18,40,255)); C2D_SceneBegin(top);
	rect(0,0,TOP_W,90,C2D_Color32(190,28,38,255));
	text(55,25,1.05f,C2D_Color32(255,255,255,255),"TEMU DRIVER");
	text(85,58,0.5f,C2D_Color32(255,210,210,255),"LOW PAY  LONG HOURS");
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	rect(22,12,276,42,C2D_Color32(35,115,220,255)); text(50,22,0.65f,C2D_Color32(255,255,255,255),"A  START");
	rect(22,62,276,42,C2D_Color32(28,145,85,255)); text(50,72,0.65f,C2D_Color32(255,255,255,255),"X  GARAGE");
	rect(22,112,276,42,C2D_Color32(180,120,30,255)); text(50,122,0.65f,C2D_Color32(255,255,255,255),"Y  SHOP");
	rect(22,162,276,42,C2D_Color32(155,35,45,255)); text(50,172,0.65f,C2D_Color32(255,255,255,255),"START  EXIT");
	char buf[48]; snprintf(buf,sizeof(buf),"$%d  LV%d  %s",money,level,ownDrift?"DRIFT":"NO DRIFT");
	text(22,212,0.45f,C2D_Color32(255,230,70,255),buf);
}
static void drawGarage(float dt){
	circlePosition cp; hidCircleRead(&cp);
	carAngle += dt*1.2f + (cp.dx/160.f)*dt*3.f;
	C3D_RenderTargetClear(top, C3D_CLEAR_ALL, C2D_Color32(60,65,90,255), 0);
	C3D_FrameDrawOn(top);
	if(meshOk && unlocked[carSelect]) drawMesh3D(&carMesh, carAngle, 0.f, -0.2f, -5.5f, 1.0f, 1.0f, 1.0f);
	C2D_Prepare();
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	char buf[72];
	snprintf(buf,sizeof(buf),"%s",carNames[carSelect]); text(16,10,0.6f,C2D_Color32(255,255,255,255),buf);
	text(16,38,0.4f,C2D_Color32(180,200,220,255),carFeat[carSelect]);
	snprintf(buf,sizeof(buf),"Gears 1-%d  PWR %.0f%%", carMaxGear[carSelect], carPower[carSelect]*100.f);
	text(16,58,0.4f,C2D_Color32(160,160,170,255),buf);
	if(unlocked[carSelect]) text(16,82,0.48f,C2D_Color32(100,255,130,255),"OWNED");
	else { snprintf(buf,sizeof(buf),"BUY $%d (A)", carPrice[carSelect]);
		text(16,82,0.48f, money>=carPrice[carSelect]?C2D_Color32(255,220,80,255):C2D_Color32(255,90,90,255), buf); }
	snprintf(buf,sizeof(buf),"$%d",money); text(16,110,0.5f,C2D_Color32(255,230,70,255),buf);
	rect(22,185,276,40,C2D_Color32(120,35,45,255)); text(55,195,0.55f,C2D_Color32(255,255,255,255),"B  BACK");
}
static void drawShop(){
	C2D_TargetClear(top,C2D_Color32(20,22,40,255)); C2D_SceneBegin(top);
	text(120,40,0.9f,C2D_Color32(255,255,255,255),"SHOP");
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	char buf[64];
	if(ownDrift){ rect(22,30,276,50,C2D_Color32(40,80,50,255)); text(40,42,0.55f,C2D_Color32(100,255,130,255),"DRIFT  OWNED"); }
	else { rect(22,30,276,50,C2D_Color32(80,60,20,255));
		snprintf(buf,sizeof(buf),"A  BUY DRIFT  $%d", DRIFT_PRICE);
		text(30,42,0.55f, money>=DRIFT_PRICE?C2D_Color32(255,220,80,255):C2D_Color32(255,100,100,255), buf); }
	snprintf(buf,sizeof(buf),"Money: $%d", money); text(22,100,0.5f,C2D_Color32(255,230,70,255),buf);
	rect(22,185,276,40,C2D_Color32(120,35,45,255)); text(55,195,0.55f,C2D_Color32(255,255,255,255),"B  BACK");
}
static void drawDrive(){
	u32 skyCol = policeChase ? C2D_Color32(60+(int)(sinf(policeSiren*8.f)*40), 40, 100, 255) : C2D_Color32(100, 170, 230, 255);
	C3D_RenderTargetClear(top, C3D_CLEAR_ALL, skyCol, 0);
	C3D_FrameDrawOn(top);
	float scroll = fmodf(roadOff * 0.1f, 4.0f);
	float curveX = roadCurve - lane * 0.15f;
	if(groundOk){
		for(int i = 0; i < 16; i++){
			float z = -0.5f - (float)i * 3.5f + scroll;
			drawMesh3D(&groundMesh, 0.f, curveX, -1.4f, z, 7.5f, 0.12f, 3.5f);
		}
	}
	if(buildOk){
		float bscroll = fmodf(roadOff * 0.07f, 5.0f);
		for(int i = 0; i < 10; i++){
			float z = -2.0f - (float)i * 5.0f + bscroll;
			drawMesh3D(&buildMesh, 0.f, -4.2f + curveX, -0.5f, z, 2.0f, 2.4f, 2.0f);
			drawMesh3D(&buildMesh, PI,  4.2f + curveX, -0.5f, z, 2.0f, 2.4f, 2.0f);
		}
	}
	for(int i = 0; i < MAX_OBS; i++) if(obs[i].on && obs[i].z < 75.f){
		int mid = obs[i].meshId; if(mid < 0 || mid >= NUM_TRAFFIC) mid = 0;
		Mesh* tm = (trafficOk[mid]) ? &trafficMeshes[mid] : (meshOk ? &carMesh : NULL);
		if(!tm || !tm->loaded) continue;
		float rel = obs[i].lane - lane;
		float wx = rel * 1.3f + roadCurve * 0.5f;
		float wz = -2.0f - obs[i].z * 0.15f;
		float sc = 0.26f + 0.38f / (1.f + obs[i].z * 0.04f);
		drawMesh3D(tm, 0.f, wx, -1.05f, wz, sc, sc, sc);
	}
	for(int i=0;i<MAX_PKG;i++) if(pkgs[i].on && pkgs[i].z<70.f){
		float rel=pkgs[i].lane-lane;
		float wx=rel*1.3f+roadCurve*0.5f;
		float wz=-2.0f-pkgs[i].z*0.15f;
		float sc=0.15f+0.2f/(1.f+pkgs[i].z*0.04f);
		if(groundOk) drawMesh3D(&groundMesh, 0.f, wx, -0.9f, wz, sc, sc*2.f, sc);
	}
	if(meshOk){
		float yaw = PI + (steerV * DEG2RAD);
		if(crashStun > 0.f) yaw += spinVel * 0.2f;
		if(drift) yaw += (steerV > 0 ? 0.12f : (steerV < 0 ? -0.12f : 0.f));
		drawMesh3D(&carMesh, yaw, 0.f, -1.7f, -2.6f, 0.45f, 0.45f, 0.45f);
	}
	C2D_Prepare();
	C2D_TargetClear(bot, C2D_Color32(16,16,26,255));
	C2D_SceneBegin(bot);
	float p = dist/target; if(p>1)p=1;
	rect(12,6,190,10,C2D_Color32(35,35,50,255));
	rect(12,6,190*p,10,C2D_Color32(35,200,90,255));
	rect(12,20,190,8,C2D_Color32(35,35,50,255));
	rect(12,20,190*rpm,8, overRev ? C2D_Color32(255,40,40,255) : C2D_Color32(40,180,255,255));
	char buf[64];
	snprintf(buf,sizeof(buf),"SPD %.0f  $%d",speed*8.f,money);
	text(12,34,0.45f,C2D_Color32(255,255,255,255),buf);
	if(overRev) text(140,34,0.45f,C2D_Color32(255,60,60,255),"OVER-REV!");
	snprintf(buf,sizeof(buf),"TIME %.0f  CRASH %d",timeL,crash);
	text(12,54,0.45f,C2D_Color32(255,255,255,255),buf);
	if(drift) text(12,72,0.45f,C2D_Color32(255,200,40,255),"DRIFT");
	else if(!ownDrift) text(12,72,0.4f,C2D_Color32(150,150,160,255),"NO DRIFT (shop)");
	if(combo > 1){ char cb[24]; snprintf(cb,sizeof(cb),"COMBO x%d",combo); text(160,72,0.45f,C2D_Color32(255,180,40,255),cb); }
	if(msgTimer > 0) text(12,90,0.5f,C2D_Color32(100,255,180,255),hudMsg);
	if(policeChase) text(160,54,0.45f,C2D_Color32(80,120,255,255),"SIREN!");
	rect(12,100,80,6,C2D_Color32(30,30,40,255));
	rect(12,100,80*nitro,6,C2D_Color32(80,200,255,255));
	text(95,96,0.35f,C2D_Color32(150,200,255,255),"X NITRO");
	C2D_DrawCircleSolid(70,175,0.5f,38,C2D_Color32(35,35,48,255));
	C2D_DrawCircleSolid(70,175,0.55f,10,C2D_Color32(200,35,35,255));
	float a = steerV * DEG2RAD;
	rect(70-22*cosf(a),175-22*sinf(a)-3,44,6,C2D_Color32(170,170,180,255));
	rect(150,140,44,60,C2D_Color32(45,45,55,255));
	rect(154,145,36,50,boost?C2D_Color32(230,45,45,255):C2D_Color32(85,28,28,255));
	rect(255,100,50,115,C2D_Color32(30,30,40,255));
	rect(275,110,10,95,C2D_Color32(60,60,70,255));
	int maxG = carMaxGear[carSelect];
	for(int g=1;g<=6;g++){
		float gy = gearYFromNum(g);
		char gn[4]; snprintf(gn,sizeof(gn),"%d",g);
		u32 col = (g <= maxG) ? C2D_Color32(160,160,170,255) : C2D_Color32(70,70,80,255);
		text(258, gy - 8, 0.38f, col, gn);
	}
	C2D_DrawCircleSolid(280, gearKnobY, 0.6f, 13, C2D_Color32(200,50,50,255));
	char gbuf[8]; snprintf(gbuf,sizeof(gbuf),"G%d", gear);
	text(258, 220, 0.5f, C2D_Color32(255,220,80,255), gbuf);
}
static void drawResult(){
	C2D_TargetClear(top,C2D_Color32(15,18,40,255)); C2D_SceneBegin(top);
	rect(45,45,310,140,C2D_Color32(35,38,65,255));
	char buf[48]; snprintf(buf,sizeof(buf),"PAY  $%d",score);
	text(110,80,0.9f,C2D_Color32(90,255,140,255),buf);
	snprintf(buf,sizeof(buf),"Combo best x%d", bestCombo);
	text(100,130,0.45f,C2D_Color32(255,200,80,255),buf);
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	rect(22,88,276,55,C2D_Color32(35,145,75,255));
	text(65,102,0.75f,C2D_Color32(255,255,255,255),"A  CONTINUE");
}
static void drawGameOver(){
	C2D_TargetClear(top,C2D_Color32(40,10,10,255)); C2D_SceneBegin(top);
	text(90,90,1.0f,C2D_Color32(255,60,60,255),"GAME OVER");
	text(50,140,0.45f,C2D_Color32(255,200,200,255),"Broke. Temu life.");
	C2D_TargetClear(bot,C2D_Color32(30,15,15,255)); C2D_SceneBegin(bot);
	rect(22,80,276,55,C2D_Color32(100,40,40,255));
	text(40,95,0.55f,C2D_Color32(255,255,255,255),"A  RETRY $200");
}

int main(int argc, char** argv){
	(void)argc; (void)argv;
	srand((unsigned)osGetTime());
	memset(&carMesh,0,sizeof(carMesh));
	memset(&groundMesh,0,sizeof(groundMesh));
	memset(&buildMesh,0,sizeof(buildMesh));
	memset(trafficMeshes,0,sizeof(trafficMeshes));
	memset(trafficOk,0,sizeof(trafficOk));
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	romfsInit(); initFont(); initSound(); loadWorldMeshes();
	loadGame();
	bool run = true; u64 last = osGetTime();
	while(aptMainLoop() && run){
		u64 now = osGetTime();
		float dt = (now-last)/1000.f; if(dt>0.05f) dt=0.05f; last=now;
		hidScanInput(); u32 k = hidKeysDown();
		C2D_Prepare(); C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		switch(state){
		case ST_SPLASH:
			splashT += dt; drawSplash();
			if(splashT > 2.2f || (k & (KEY_A|KEY_START))) state = ST_MENU;
			break;
		case ST_MENU:
			drawMenu();
			if(k&KEY_A){
				if(!unlocked[carSelect]) carSelect = 0;
				loadCar(carSelect); loadWorldMeshes(); resetDrive(); state=ST_DRIVE;
			}
			if(k&KEY_X){ loadCar(carSelect); carAngle=0; state=ST_GARAGE; }
			if(k&KEY_Y) state=ST_SHOP;
			if(k&KEY_START){ saveGame(); run=false; }
			break;
		case ST_GARAGE:
			if(k&KEY_LEFT){ carSelect=(carSelect+NUM_CARS-1)%NUM_CARS; loadCar(carSelect); }
			if(k&KEY_RIGHT){ carSelect=(carSelect+1)%NUM_CARS; loadCar(carSelect); }
			if(k&KEY_A){
				if(!unlocked[carSelect] && money >= carPrice[carSelect]){
					money -= carPrice[carSelect]; unlocked[carSelect]=true; saveGame();
				}
			}
			if(k&(KEY_B|KEY_START)){ saveGame(); state=ST_MENU; }
			drawGarage(dt);
			break;
		case ST_SHOP:
			drawShop();
			if(k&KEY_A && !ownDrift && money >= DRIFT_PRICE){
				money -= DRIFT_PRICE; ownDrift = true; saveGame();
			}
			if(k&(KEY_B|KEY_START)) state=ST_MENU;
			break;
		case ST_DRIVE:
			updateDrive(dt); drawDrive();
			break;
		case ST_RESULT:
			drawResult();
			if(k&KEY_A){ if(dist>=target) level++; saveGame(); state=ST_MENU; }
			break;
		case ST_GAMEOVER:
			drawGameOver();
			if(k&KEY_A){ money = 200; crash=0; level=1; saveGame(); state=ST_MENU; }
			break;
		}
		C3D_FrameEnd(0);
	}
	saveGame();
	exitSound();
	meshFree(&carMesh); meshFree(&groundMesh); meshFree(&buildMesh);
	for(int i=0;i<NUM_TRAFFIC;i++) meshFree(&trafficMeshes[i]);
	if(shaderOk){ shaderProgramFree(&program); if(vshader_dvlb) DVLB_Free(vshader_dvlb); }
	if(font) C2D_FontFree(font);
	C2D_TextBufDelete(sbuf); C2D_TextBufDelete(dbuf);
	romfsExit(); cfguExit(); C2D_Fini(); C3D_Fini(); gfxExit();
	return 0;
}
