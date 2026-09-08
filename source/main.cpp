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
#define PI 3.14159265f

enum State { ST_MENU, ST_GARAGE, ST_DRIVE, ST_RESULT };

static State state = ST_MENU;
static C3D_RenderTarget *top, *bot;
static C2D_TextBuf sbuf, dbuf;
static C2D_Font font = NULL;

static float lane = 0, speed = 0, steerV = 0, yawRate = 0, carAngle = 0;
static float crashStun = 0, spinVel = 0;
static bool drift = false, boost = false;
static int money = 1500, level = 1, crash = 0, score = 0;
static float dist = 0, target = 2200, timeL = 60, roadOff = 0;
static int carSelect = 0;

static const char* carFiles[] = {
	"romfs:/cars/coupe.obj","romfs:/cars/van.obj","romfs:/cars/police.obj",
	"romfs:/cars/jeep.obj","romfs:/cars/rally.obj","romfs:/cars/lamb.obj"
};
static const char* carNames[] = {"Coupe","Van (Temu)","Police","Jeep","Rally","Lamb"};
static float carColors[][3] = {
	{0.85f,0.15f,0.15f},{0.2f,0.5f,0.9f},{0.1f,0.1f,0.15f},
	{0.2f,0.6f,0.2f},{0.9f,0.7f,0.1f},{0.9f,0.85f,0.1f}
};

struct Obs { float z, lane, spd; bool on; };
static Obs obs[MAX_OBS];

static DVLB_s* vshader_dvlb = NULL;
static shaderProgram_s program;
static int uLoc_projection = -1, uLoc_modelView = -1;
static Mesh carMesh, roadMesh, buildMesh;
static bool shaderOk = false, meshOk = false, roadOk = false, buildOk = false, shaderInited = false;

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
static void loadCity(){
	if(!initShaderOnce()) return;
	meshFree(&roadMesh); meshFree(&buildMesh);
	roadOk=meshLoadOBJ("romfs:/city/road-straight.obj",&roadMesh,0.35f,0.35f,0.4f);
	if(!roadOk) roadOk=meshLoadCube(&roadMesh,0.35f,0.35f,0.4f);
	buildOk=meshLoadOBJ("romfs:/city/low-detail-building-d.obj",&buildMesh,0.7f,0.45f,0.35f);
	if(!buildOk) buildOk=meshLoadCube(&buildMesh,0.7f,0.45f,0.35f);
}

static void drawMesh(Mesh* m, float angleY, float posX, float posY, float posZ, float sx, float sy, float sz){
	if(!shaderOk||!m||!m->loaded) return;
	C3D_BindProgram(&program);
	C3D_AttrInfo* attr=C3D_GetAttrInfo(); AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr,0,GPU_FLOAT,3); AttrInfo_AddLoader(attr,1,GPU_FLOAT,4);
	C3D_BufInfo* buf=C3D_GetBufInfo(); BufInfo_Init(buf);
	BufInfo_Add(buf,m->vbo,sizeof(MeshVertex),2,0x10);
	C3D_TexEnv* env=C3D_GetTexEnv(0); C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL);
	C3D_Mtx projection, modelView;
	Mtx_PerspTilt(&projection,C3D_AngleFromDegrees(48.0f),400.0f/240.0f,0.08f,80.0f,true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uLoc_projection,&projection);
	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView,posX,posY,posZ,true);
	Mtx_RotateY(&modelView,angleY,true);
	Mtx_Scale(&modelView,sx,sy,sz);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uLoc_modelView,&modelView);
	C3D_DrawArrays(GPU_TRIANGLES,0,m->count);
}

static void resetDrive(){
	lane=0; speed=0.f; steerV=0; yawRate=0;
	drift=boost=false; crashStun=0; spinVel=0;
	dist=0; crash=0; score=0; roadOff=0;
	target=1800.f+level*400.f; timeL=55.f+level*8.f;
	for(int i=0;i<MAX_OBS;i++) obs[i].on=false;
}
static void spawnObs(){
	for(int i=0;i<MAX_OBS;i++) if(!obs[i].on){
		obs[i].on=true; obs[i].z=70+(rand()%50);
		obs[i].lane=(float)((rand()%5)-2);
		obs[i].spd=0.5f+(rand()%40)/100.f; break;
	}
}

static void updateDrive(float dt){
	hidScanInput();
	u32 h=hidKeysHeld(); u32 d=hidKeysDown();
	circlePosition c; hidCircleRead(&c);
	float stick=c.dx/155.f;
	if(stick>1.f) stick=1.f; if(stick<-1.f) stick=-1.f;

	if(crashStun>0.f){
		crashStun-=dt;
		speed*=(1.f-3.0f*dt); if(speed<0) speed=0;
		lane+=spinVel*dt; spinVel*=(1.f-2.2f*dt);
		if(lane<-2.2f){lane=-2.2f;spinVel=0;}
		if(lane>2.2f){lane=2.2f;spinVel=0;}
		roadOff+=speed*6.f*dt; dist+=speed*4.f*dt; timeL-=dt;
		for(int i=0;i<MAX_OBS;i++) if(obs[i].on){ obs[i].z-=4.f*dt; if(obs[i].z<1.f) obs[i].on=false; }
		if(timeL<=0||crash>=8){ int r=15-crash*3; if(r<0)r=0; money+=r; score=r; state=ST_RESULT; }
		if(d&KEY_START) state=ST_MENU;
		return;
	}

	boost=(h&(KEY_A|KEY_R))!=0;
	drift=((h&(KEY_Y|KEY_L))!=0)&&fabsf(stick)>0.12f;
	bool braking=(h&KEY_B)!=0;

	float engineForce = boost ? 16.0f : 0.f;
	float drag = 2.2f + speed*0.25f;
	float brake = braking ? 18.0f : 0.f;
	speed += (engineForce - drag - brake)*dt*0.4f;
	if(speed<0.f) speed=0.f;
	if(speed>12.0f) speed=12.0f;

	float grip = drift ? 0.45f : 1.0f;
	float steerTarget = stick * (2.8f / (0.6f + speed*0.12f + 0.01f));
	yawRate += (steerTarget - yawRate)*8.f*dt*grip;
	lane += yawRate * speed * 0.22f * dt;
	steerV += (stick*65.f - steerV)*0.3f;
	if(lane<-2.15f){ lane=-2.15f; yawRate*=-0.4f; speed*=0.85f; }
	if(lane>2.15f){ lane=2.15f; yawRate*=-0.4f; speed*=0.85f; }

	dist += speed*18.f*dt; timeL -= dt; roadOff += speed*22.f*dt;
	static float timer=0; timer+=dt;
	if(timer>0.8f){ spawnObs(); timer=0; }

	for(int i=0;i<MAX_OBS;i++) if(obs[i].on){
		obs[i].z -= (speed + obs[i].spd)*14.f*dt;
		if(obs[i].z<10.f && obs[i].z>1.8f && fabsf(obs[i].lane-lane)<0.8f){
			crash++; crashStun=1.5f+crash*0.25f;
			spinVel=(obs[i].lane>lane)?-3.5f:3.5f;
			speed*=0.08f; yawRate=spinVel; obs[i].on=false; break;
		}
		if(obs[i].z<1.5f) obs[i].on=false;
	}
	if(dist>=target){
		int r=(int)(220+level*90+fmaxf(0,timeL)*7-crash*50);
		if(r<50)r=50; money+=r; score=r; state=ST_RESULT;
	}else if(timeL<=0||crash>=8){
		int r=15-crash*3; if(r<0)r=0; money+=r; score=r; state=ST_RESULT;
	}
	if(d&KEY_START) state=ST_MENU;
}

/* ========== MENU only 2D ========== */
static void drawMenu(){
	C2D_TargetClear(top,C2D_Color32(12,14,32,255)); C2D_SceneBegin(top);
	rect(0,0,TOP_W,88,C2D_Color32(190,28,38,255));
	text(55,22,1.05f,C2D_Color32(255,255,255,255),"TEMU DRIVER");
	text(95,55,0.5f,C2D_Color32(255,210,210,255),"v1.0  REAL 3D");
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	rect(22,18,276,50,C2D_Color32(35,115,220,255));
	text(50,30,0.72f,C2D_Color32(255,255,255,255),"A  START");
	rect(22,82,276,50,C2D_Color32(28,145,85,255));
	text(50,94,0.72f,C2D_Color32(255,255,255,255),"X  GARAGE");
	rect(22,146,276,50,C2D_Color32(155,35,45,255));
	text(50,158,0.72f,C2D_Color32(255,255,255,255),"START  EXIT");
	char buf[48]; snprintf(buf,sizeof(buf),"$%d  LV%d",money,level);
	text(22,210,0.55f,C2D_Color32(255,230,70,255),buf);
}

/* ========== GARAGE pure 3D ========== */
static void drawGarage(float dt){
	circlePosition cp; hidCircleRead(&cp);
	carAngle+=dt*1.1f+(cp.dx/160.f)*dt*2.8f;
	C3D_RenderTargetClear(top,C3D_CLEAR_ALL,C2D_Color32(50,55,75,255),0);
	C3D_FrameDrawOn(top);
	if(meshOk) drawMesh(&carMesh, carAngle, 0.f, -0.4f, -5.0f, 0.95f, 0.95f, 0.95f);
	C2D_Prepare();
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	char buf[64];
	snprintf(buf,sizeof(buf),"%s",carNames[carSelect]);
	text(20,20,0.7f,C2D_Color32(255,255,255,255),buf);
	snprintf(buf,sizeof(buf),"Mesh: %s",meshOk?"OK":"FAIL");
	text(20,55,0.5f,meshOk?C2D_Color32(100,255,130,255):C2D_Color32(255,90,90,255),buf);
	text(20,90,0.48f,C2D_Color32(180,180,190,255),"LEFT/RIGHT  Circle rotate");
	rect(22,170,276,46,C2D_Color32(120,35,45,255));
	text(55,182,0.65f,C2D_Color32(255,255,255,255),"B  BACK");
}

/* ========== DRIVE pure 3D models ========== */
static void drawDrive(){
	/* full 3D top screen */
	C3D_RenderTargetClear(top, C3D_CLEAR_ALL, C2D_Color32(110, 175, 235, 255), 0);
	C3D_FrameDrawOn(top);

	/* road tiles along Z */
	if(roadOk){
		float base = fmodf(roadOff * 0.08f, 2.0f);
		for(int i=0;i<14;i++){
			float z = -1.0f - i*2.0f + base;
			drawMesh(&roadMesh, 0.f, -lane*0.15f, -1.2f, z, 4.5f, 1.f, 2.0f);
		}
	}

	/* buildings left / right */
	if(buildOk){
		float base = fmodf(roadOff * 0.05f, 3.0f);
		for(int i=0;i<8;i++){
			float z = -2.0f - i*3.0f + base;
			drawMesh(&buildMesh, 0.f, -3.2f - lane*0.1f, -0.8f, z, 1.8f, 1.8f, 1.8f);
			drawMesh(&buildMesh, PI,  3.2f - lane*0.1f, -0.8f, z, 1.8f, 1.8f, 1.8f);
		}
	}

	/* traffic cars (same mesh, different positions) */
	if(meshOk){
		for(int i=0;i<MAX_OBS;i++) if(obs[i].on && obs[i].z<80.f){
			float rel = obs[i].lane - lane;
			float wx = rel * 1.15f;
			float wz = -2.5f - obs[i].z * 0.12f;
			float sc = 0.25f + 0.35f / (1.f + obs[i].z * 0.03f);
			drawMesh(&carMesh, PI, wx, -1.0f, wz, sc, sc, sc);
		}
		/* player hood */
		float yaw = steerV * 0.02f + yawRate * 0.12f;
		if(crashStun>0.f) yaw += spinVel * 0.2f;
		drawMesh(&carMesh, yaw, lane*0.1f, -1.85f, -2.9f, 0.4f, 0.4f, 0.4f);
	}

	C2D_Prepare();

	/* bottom HUD only (not top 2D road) */
	C2D_TargetClear(bot,C2D_Color32(16,16,26,255)); C2D_SceneBegin(bot);
	float p=dist/target; if(p>1)p=1;
	rect(12,10,296,15,C2D_Color32(35,35,50,255));
	rect(12,10,296*p,15,C2D_Color32(35,200,90,255));
	float tp=timeL/(55.f+level*8.f); if(tp<0)tp=0; if(tp>1)tp=1;
	rect(12,30,296,11,C2D_Color32(35,35,50,255));
	rect(12,30,296*tp,11,C2D_Color32(230,175,35,255));
	char buf[64];
	snprintf(buf,sizeof(buf),"SPD %.0f  DIST %.0f",speed*8.f,dist);
	text(12,50,0.5f,C2D_Color32(255,255,255,255),buf);
	snprintf(buf,sizeof(buf),"TIME %.0f  $%d",timeL,money);
	text(12,72,0.5f,C2D_Color32(255,255,255,255),buf);
	if(crashStun>0.f) text(12,94,0.7f,C2D_Color32(255,35,35,255),"CRASH!");
	else { snprintf(buf,sizeof(buf),"CRASH %d",crash); text(12,94,0.5f,C2D_Color32(255,130,130,255),buf); }
	C2D_DrawCircleSolid(90,168,0.5f,50,C2D_Color32(35,35,48,255));
	C2D_DrawCircleSolid(90,168,0.55f,13,C2D_Color32(200,35,35,255));
	float a=steerV*0.0174533f;
	rect(90-30*cosf(a),168-30*sinf(a)-3,60,6,C2D_Color32(170,170,180,255));
	rect(208,125,62,82,C2D_Color32(45,45,55,255));
	rect(216,133,46,66,boost?C2D_Color32(230,45,45,255):C2D_Color32(85,28,28,255));
	text(10,218,0.38f,C2D_Color32(190,190,200,255),"HOLD A=GAS  B=BRAKE  Y=DRIFT");
}

static void drawResult(){
	C2D_TargetClear(top,C2D_Color32(12,14,32,255)); C2D_SceneBegin(top);
	rect(45,45,310,130,C2D_Color32(35,38,65,255));
	char buf[48]; snprintf(buf,sizeof(buf),"REWARD  $%d",score);
	text(95,90,0.95f,C2D_Color32(90,255,140,255),buf);
	C2D_TargetClear(bot,C2D_Color32(22,24,40,255)); C2D_SceneBegin(bot);
	rect(22,88,276,55,C2D_Color32(35,145,75,255));
	text(65,102,0.75f,C2D_Color32(255,255,255,255),"A  CONTINUE");
}

int main(int argc,char** argv){
	(void)argc;(void)argv;
	srand((unsigned)osGetTime());
	memset(&carMesh,0,sizeof(carMesh));
	memset(&roadMesh,0,sizeof(roadMesh));
	memset(&buildMesh,0,sizeof(buildMesh));
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
	bot=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
	romfsInit(); initFont();
	loadCity();
	bool run=true; u64 last=osGetTime();
	while(aptMainLoop()&&run){
		u64 now=osGetTime();
		float dt=(now-last)/1000.f; if(dt>0.05f)dt=0.05f; last=now;
		hidScanInput(); u32 k=hidKeysDown();
		C2D_Prepare(); C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		switch(state){
		case ST_MENU:
			drawMenu();
			if(k&KEY_A){ loadCar(carSelect); loadCity(); resetDrive(); state=ST_DRIVE; }
			if(k&KEY_X){ loadCar(carSelect); carAngle=0; state=ST_GARAGE; }
			if(k&KEY_START) run=false;
			break;
		case ST_GARAGE:
			if(k&KEY_LEFT) loadCar((carSelect+NUM_CARS-1)%NUM_CARS);
			if(k&KEY_RIGHT) loadCar((carSelect+1)%NUM_CARS);
			if(k&(KEY_B|KEY_START)){ C2D_Prepare(); state=ST_MENU; }
			drawGarage(dt);
			break;
		case ST_DRIVE:
			updateDrive(dt); drawDrive();
			break;
		case ST_RESULT:
			drawResult();
			if(k&KEY_A){ if(dist>=target) level++; state=ST_MENU; }
			break;
		}
		C3D_FrameEnd(0);
	}
	meshFree(&carMesh); meshFree(&roadMesh); meshFree(&buildMesh);
	if(shaderOk){ shaderProgramFree(&program); if(vshader_dvlb) DVLB_Free(vshader_dvlb); }
	if(font) C2D_FontFree(font);
	C2D_TextBufDelete(sbuf); C2D_TextBufDelete(dbuf);
	romfsExit(); cfguExit(); C2D_Fini(); C3D_Fini(); gfxExit();
	return 0;
}
