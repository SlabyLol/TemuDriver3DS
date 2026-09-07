#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

#define VERSION "1.3.0"
#define MAX_OBS 16
#define MAX_SMOKE 28
#define LANES 4

enum State { ST_BOOT, ST_MENU, ST_GARAGE, ST_DRIVE, ST_RESULT };

struct Car { float x,y,speed,angle; bool drift; };
struct Obs  { float x,y,speed; int type; bool on; };
struct P    { float x,y,vx,vy,life; bool on; };

State state = ST_BOOT;
Car player;
Obs obs[MAX_OBS];
P smoke[MAX_SMOKE];

int money=1000, level=1, crashes=0, score=0;
float dist=0, target=1800, timeLeft=50, roadY=0;
int upSpeed=0, upHand=0, upRew=0;
float steerVis=0;

C3D_RenderTarget *topT, *botT;
C2D_SpriteSheet sheet=nullptr;

bool sndOk=false;
ndspWaveBuf wbuf[6];
u8* adata[6]={nullptr};

bool loadWav(const char* p, int s){
    FILE*f=fopen(p,"rb"); if(!f)return false;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz<44){fclose(f);return false;}
    u8*b=(u8*)linearAlloc(sz); if(!b){fclose(f);return false;}
    fread(b,1,sz,f); fclose(f);
    adata[s]=b; wbuf[s].data_vaddr=b+44; wbuf[s].nsamples=(sz-44)/2; wbuf[s].status=NDSP_WBUF_DONE;
    return true;
}
void sfx(int s,float v=0.6f){
    if(!sndOk||s<0||s>5||!adata[s])return;
    ndspChnWaveBufClear(s); wbuf[s].status=NDSP_WBUF_DONE;
    ndspChnSetInterp(s,NDSP_INTERP_LINEAR); ndspChnSetRate(s,22050.f);
    ndspChnSetFormat(s,NDSP_FORMAT_MONO_PCM16);
    float m[12]={v,v}; ndspChnSetMix(s,m); ndspChnWaveBufAdd(s,&wbuf[s]);
}
void initSnd(){
    if(ndspInit()!=0)return; ndspSetOutputMode(NDSP_OUTPUT_STEREO); sndOk=true;
    loadWav("romfs:/sfx/engine.wav",0); loadWav("romfs:/sfx/drift.wav",1);
    loadWav("romfs:/sfx/crash.wav",2); loadWav("romfs:/sfx/coin.wav",3);
    loadWav("romfs:/sfx/select.wav",4); loadWav("romfs:/sfx/success.wav",5);
}
void exitSnd(){ for(int i=0;i<6;i++)if(adata[i])linearFree(adata[i]); ndspExit(); }

void spawnObs(){
    for(int i=0;i<MAX_OBS;i++) if(!obs[i].on){
        obs[i].on=true; int l=rand()%LANES;
        obs[i].x=50.f+l*75.f+37.f; obs[i].y=-60.f;
        obs[i].speed=2.0f+(rand()%100)/50.f+level*0.2f; obs[i].type=rand()%4; break;
    }
}
void spawnSm(float x,float y){
    for(int i=0;i<MAX_SMOKE;i++) if(!smoke[i].on){
        smoke[i].on=true; smoke[i].x=x+(rand()%30-15); smoke[i].y=y+20;
        smoke[i].vx=(rand()%40-20)*0.07f; smoke[i].vy=0.5f+(rand()%20)*0.06f;
        smoke[i].life=0.55f+(rand()%30)*0.02f; break;
    }
}
void reset(){
    player.x=TOP_W/2; player.y=TOP_H-70; player.speed=2.4f+upSpeed*0.4f;
    player.angle=0; player.drift=false;
    for(int i=0;i<MAX_OBS;i++)obs[i].on=false;
    for(int i=0;i<MAX_SMOKE;i++)smoke[i].on=false;
    dist=0; crashes=0; score=0; target=1700+level*480; timeLeft=45+level*9; roadY=0; steerVis=0;
}
bool hit(float x1,float y1,float x2,float y2,float r){ float dx=x1-x2,dy=y1-y2; return dx*dx+dy*dy<r*r; }

void update(float dt){
    hidScanInput(); u32 held=hidKeysHeld(), down=hidKeysDown();
    circlePosition cp; hidCircleRead(&cp);

    float st=cp.dx/150.f; if(st>1)st=1; if(st<-1)st=-1;
    bool pedal = (held & KEY_A) || (held & KEY_R);
    bool wantDrift = (held & (KEY_Y|KEY_B|KEY_L));

    player.drift = wantDrift && fabsf(st)>0.2f;
    float hand=3.0f+upHand*0.6f;
    if(player.drift){
        player.x += st*hand*2.6f*dt*60;
        player.angle = st*28.f;
        if(rand()%2==0) spawnSm(player.x,player.y);
        static float dtm=0; dtm+=dt; if(dtm>0.3f){sfx(1,0.4f);dtm=0;}
    }else{
        player.x += st*hand*dt*60;
        player.angle *= 0.85f;
    }
    steerVis += (st*55.f - steerVis)*0.25f;

    if(player.x<40)player.x=40; if(player.x>TOP_W-40)player.x=TOP_W-40;

    float boost = pedal ? 1.35f : 1.0f;
    float fwd = player.speed * (player.drift?1.15f:1.f) * boost;
    dist += fwd*dt*60; roadY += fwd*dt*60; timeLeft -= dt;

    static float sp=0; sp+=dt;
    if(sp > fmaxf(0.4f,1.05f-level*0.05f)){ spawnObs(); sp=0; }

    for(int i=0;i<MAX_OBS;i++) if(obs[i].on){
        obs[i].y += (obs[i].speed + fwd*0.4f)*dt*60;
        if(obs[i].y>TOP_H+80){ obs[i].on=false; score+=10; }
        if(hit(player.x,player.y,obs[i].x,obs[i].y,38)){
            obs[i].on=false; crashes++; sfx(2,0.8f);
            player.x += (player.x>obs[i].x)?22:-22;
        }
    }
    for(int i=0;i<MAX_SMOKE;i++) if(smoke[i].on){
        smoke[i].x+=smoke[i].vx; smoke[i].y+=smoke[i].vy; smoke[i].life-=dt;
        if(smoke[i].life<=0)smoke[i].on=false;
    }

    if(dist>=target){
        float bonus=fmaxf(0,timeLeft)*5.f*(1.f+upRew*0.2f);
        int r=(int)(140+level*65+bonus-crashes*40); if(r<25)r=25;
        money+=r; score=r; sfx(5,0.9f); state=ST_RESULT;
    }else if(timeLeft<=0||crashes>=8){
        int r=15-crashes*3; if(r<0)r=0; money+=r; score=r; state=ST_RESULT;
    }
    if(down&KEY_START) state=ST_MENU;
}

void drawRect(float x,float y,float w,float h,u32 c){ C2D_DrawRectSolid(x,y,0.5f,w,h,c); }

void drawTop(){
    C2D_TargetClear(topT, C2D_Color32(25,25,40,255));
    C2D_SceneBegin(topT);

    drawRect(36,0,TOP_W-72,TOP_H, C2D_Color32(50,50,60,255));
    for(int i=1;i<LANES;i++){
        float lx=36+i*((TOP_W-72.f)/LANES);
        for(int yy=-60;yy<TOP_H+60;yy+=42){
            float y=fmodf(yy+roadY,42.f)-25;
            drawRect(lx-2.5f,y,5,20, C2D_Color32(240,230,80,255));
        }
    }
    drawRect(0,0,36,TOP_H, C2D_Color32(100,30,30,255));
    drawRect(TOP_W-36,0,36,TOP_H, C2D_Color32(100,30,30,255));

    u32 cols[4]={C2D_Color32(60,130,210,255),C2D_Color32(35,170,130,255),
                 C2D_Color32(220,175,40,255),C2D_Color32(160,65,210,255)};
    for(int i=0;i<MAX_OBS;i++) if(obs[i].on){
        float ox=obs[i].x, oy=obs[i].y;
        drawRect(ox-20,oy-28,40,56, cols[obs[i].type%4]);
        drawRect(ox-13,oy-14,26,16, C2D_Color32(150,210,255,255));
    }
    for(int i=0;i<MAX_SMOKE;i++) if(smoke[i].on){
        u8 a=(u8)(smoke[i].life*160);
        drawRect(smoke[i].x-8,smoke[i].y-8,16,16, C2D_Color32(190,190,190,a));
    }
    float px=player.x, py=player.y;
    drawRect(px-20,py-28,40,56, C2D_Color32(230,50,50,255));
    drawRect(px-13,py-14,26,16, C2D_Color32(140,200,255,255));
    if(player.drift){
        drawRect(px-32,py+22,14,8, C2D_Color32(200,200,200,140));
        drawRect(px+18,py+22,14,8, C2D_Color32(200,200,200,140));
    }
}

void drawBottom(){
    C2D_TargetClear(botT, C2D_Color32(18,18,28,255));
    C2D_SceneBegin(botT);

    drawRect(0,0,BOT_W,36, C2D_Color32(30,30,50,255));
    float prog = dist/target; if(prog>1)prog=1;
    drawRect(20,10,280,16, C2D_Color32(40,40,55,255));
    drawRect(20,10,280*prog,16, C2D_Color32(50,200,110,255));

    drawRect(10,48,145,50, C2D_Color32(35,35,55,255));
    drawRect(165,48,145,50, C2D_Color32(35,35,55,255));

    // Steering wheel
    float cx=100, cy=155;
    C2D_DrawCircleSolid(cx,cy,0.4f,48, C2D_Color32(40,40,50,255));
    C2D_DrawCircleSolid(cx,cy,0.45f,38, C2D_Color32(60,60,70,255));
    C2D_DrawCircleSolid(cx,cy,0.5f,14, C2D_Color32(200,50,50,255));
    float a = steerVis * 3.14159f/180.f;
    float c=cosf(a), s=sinf(a);
    drawRect(cx-30*c, cy-30*s-2, 60, 5, C2D_Color32(120,120,130,255));
    drawRect(cx+30*s-2, cy-30*c, 5, 60, C2D_Color32(120,120,130,255));

    // Pedal
    float ppx=230, ppy=140;
    drawRect(ppx,ppy,50,70, C2D_Color32(50,50,60,255));
    bool pedalOn = (hidKeysHeld() & (KEY_A|KEY_R));
    drawRect(ppx+6,ppy+8,38,54, pedalOn ? C2D_Color32(230,60,60,255) : C2D_Color32(90,40,40,255));

    // Time bar
    float tprog = timeLeft / (45.f+level*9); if(tprog>1)tprog=1; if(tprog<0)tprog=0;
    drawRect(20,210,280,12, C2D_Color32(40,40,55,255));
    drawRect(20,210,280*tprog,12, C2D_Color32(230,180,50,255));
}

void drawMenu(){
    C2D_TargetClear(topT, C2D_Color32(20,20,35,255));
    C2D_SceneBegin(topT);
    drawRect(80,40,240,80, C2D_Color32(40,40,70,255));
    drawRect(100,60,200,40, C2D_Color32(230,50,50,255));

    C2D_TargetClear(botT, C2D_Color32(18,18,28,255));
    C2D_SceneBegin(botT);
    drawRect(40,40,240,40, C2D_Color32(50,120,200,255));
    drawRect(40,95,240,40, C2D_Color32(50,160,100,255));
    drawRect(40,150,240,40, C2D_Color32(160,60,60,255));
}

void drawGarage(){
    C2D_TargetClear(topT, C2D_Color32(20,20,35,255));
    C2D_SceneBegin(topT);
    drawRect(50,30,300,180, C2D_Color32(35,35,55,255));

    C2D_TargetClear(botT, C2D_Color32(18,18,28,255));
    C2D_SceneBegin(botT);
    drawRect(30,30,260,35, C2D_Color32(60,100,180,255));
    drawRect(30,75,260,35, C2D_Color32(50,140,90,255));
    drawRect(30,120,260,35, C2D_Color32(160,120,40,255));
    drawRect(30,180,260,35, C2D_Color32(100,50,50,255));
}

void drawResult(){
    C2D_TargetClear(topT, C2D_Color32(20,20,35,255));
    C2D_SceneBegin(topT);
    drawRect(60,50,280,140, C2D_Color32(40,40,65,255));

    C2D_TargetClear(botT, C2D_Color32(18,18,28,255));
    C2D_SceneBegin(botT);
    drawRect(40,80,240,50, C2D_Color32(50,150,80,255));
}

int main(){
    srand(osGetTime());
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topT = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    botT = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    romfsInit();
    initSnd();

    bool run=true; u64 last=osGetTime(); int boot=0;

    while(aptMainLoop() && run){
        u64 now=osGetTime(); float dt=(now-last)/1000.f; if(dt>0.05f)dt=0.05f; last=now;
        hidScanInput(); u32 down=hidKeysDown();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        switch(state){
        case ST_BOOT:
            drawMenu();
            boot++; if(boot>90) state=ST_MENU;
            break;
        case ST_MENU:
            drawMenu();
            if(down&KEY_A){ sfx(4); reset(); state=ST_DRIVE; }
            if(down&KEY_X){ sfx(4); state=ST_GARAGE; }
            if(down&KEY_START) run=false;
            break;
        case ST_GARAGE:
            drawGarage();
            if(down&KEY_A){ int c=150+upSpeed*95; if(money>=c){money-=c;upSpeed++;sfx(3);} }
            if(down&KEY_X){ int c=120+upHand*75; if(money>=c){money-=c;upHand++;sfx(3);} }
            if(down&KEY_Y){ int c=200+upRew*140; if(money>=c){money-=c;upRew++;sfx(3);} }
            if(down&KEY_START||down&KEY_B){ sfx(4); state=ST_MENU; }
            break;
        case ST_DRIVE:
            update(dt);
            drawTop();
            drawBottom();
            break;
        case ST_RESULT:
            drawResult();
            if(down&KEY_A){
                if(dist>=target) level++;
                sfx(4); state=ST_MENU;
            }
            break;
        }

        C3D_FrameEnd(0);
    }

    exitSnd(); romfsExit();
    C2D_Fini(); C3D_Fini(); gfxExit();
    return 0;
}
