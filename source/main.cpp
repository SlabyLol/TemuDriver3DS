#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

#define VERSION "2.0.0"
#define MAX_OBS 12
#define MAX_SMOKE 20
#define ROAD_SEGS 24

enum State { ST_BOOT, ST_MENU, ST_GARAGE, ST_DRIVE, ST_RESULT };

struct Obs {
    float z;
    float lane;
    float speed;
    int type;
    bool on;
};

struct Smoke {
    float x, y, z, life;
    bool on;
};

State state = ST_BOOT;
Obs obs[MAX_OBS];
Smoke smoke[MAX_SMOKE];

float playerLane = 0.0f;
float playerSpeed = 0.0f;
float playerZ = 0.0f;
float steer = 0.0f;
float steerVis = 0.0f;
bool drifting = false;
bool boosting = false;

int money = 1200, level = 1, crashes = 0, score = 0;
float dist = 0, target = 2000, timeLeft = 55;
int upSpeed = 0, upHand = 0, upRew = 0;

C3D_RenderTarget *topT, *botT;

bool sndOk = false;
ndspWaveBuf wbuf[6];
u8* adata[6] = {nullptr};

bool loadWav(const char* p, int s) {
    FILE* f = fopen(p, "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 44) { fclose(f); return false; }
    u8* b = (u8*)linearAlloc(sz); if (!b) { fclose(f); return false; }
    fread(b, 1, sz, f); fclose(f);
    adata[s] = b; wbuf[s].data_vaddr = b + 44; wbuf[s].nsamples = (sz - 44) / 2;
    wbuf[s].status = NDSP_WBUF_DONE; return true;
}
void sfx(int s, float v = 0.6f) {
    if (!sndOk || s < 0 || s > 5 || !adata[s]) return;
    ndspChnWaveBufClear(s); wbuf[s].status = NDSP_WBUF_DONE;
    ndspChnSetInterp(s, NDSP_INTERP_LINEAR); ndspChnSetRate(s, 22050.f);
    ndspChnSetFormat(s, NDSP_FORMAT_MONO_PCM16);
    float m[12] = {v, v}; ndspChnSetMix(s, m); ndspChnWaveBufAdd(s, &wbuf[s]);
}
void initSnd() {
    if (ndspInit() != 0) return;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO); sndOk = true;
    loadWav("romfs:/sfx/engine.wav", 0); loadWav("romfs:/sfx/drift.wav", 1);
    loadWav("romfs:/sfx/crash.wav", 2); loadWav("romfs:/sfx/coin.wav", 3);
    loadWav("romfs:/sfx/select.wav", 4); loadWav("romfs:/sfx/success.wav", 5);
}
void exitSnd() { for (int i = 0; i < 6; i++) if (adata[i]) linearFree(adata[i]); ndspExit(); }

void spawnObs() {
    for (int i = 0; i < MAX_OBS; i++) if (!obs[i].on) {
        obs[i].on = true;
        obs[i].z = 80.0f + (rand() % 40);
        obs[i].lane = ((rand() % 4) - 1.5f);
        obs[i].speed = 0.8f + (rand() % 50) / 80.0f + level * 0.15f;
        obs[i].type = rand() % 4;
        break;
    }
}

void spawnSmoke() {
    for (int i = 0; i < MAX_SMOKE; i++) if (!smoke[i].on) {
        smoke[i].on = true;
        smoke[i].x = playerLane * 40 + (rand() % 20 - 10);
        smoke[i].y = 180 + (rand() % 20);
        smoke[i].z = 2.0f;
        smoke[i].life = 0.6f + (rand() % 20) * 0.02f;
        break;
    }
}

void reset() {
    playerLane = 0; playerSpeed = 0; playerZ = 0; steer = 0; steerVis = 0;
    drifting = boosting = false;
    for (int i = 0; i < MAX_OBS; i++) obs[i].on = false;
    for (int i = 0; i < MAX_SMOKE; i++) smoke[i].on = false;
    dist = 0; crashes = 0; score = 0;
    target = 1800 + level * 500; timeLeft = 50 + level * 8;
}

void update(float dt) {
    hidScanInput();
    u32 held = hidKeysHeld(), down = hidKeysDown();
    circlePosition cp; hidCircleRead(&cp);

    float st = cp.dx / 155.0f;
    if (st > 1) st = 1; if (st < -1) st = -1;
    steer = st;
    steerVis += (st * 70.0f - steerVis) * 0.2f;

    boosting = (held & (KEY_A | KEY_R));
    drifting = (held & (KEY_Y | KEY_B | KEY_L)) && fabsf(st) > 0.25f;

    float maxSpd = 4.2f + upSpeed * 0.45f;
    float accel = boosting ? 3.5f : 1.8f;
    float targetSpd = boosting ? maxSpd : maxSpd * 0.7f;
    if (playerSpeed < targetSpd) playerSpeed += accel * dt;
    else playerSpeed -= 1.5f * dt;
    if (playerSpeed < 0.5f) playerSpeed = 0.5f;

    float hand = 2.2f + upHand * 0.5f;
    if (drifting) {
        playerLane += st * hand * 1.8f * dt;
        if (rand() % 3 == 0) spawnSmoke();
        static float dtm = 0; dtm += dt;
        if (dtm > 0.28f) { sfx(1, 0.35f); dtm = 0; }
    } else {
        playerLane += st * hand * dt;
    }
    if (playerLane < -1.6f) playerLane = -1.6f;
    if (playerLane > 1.6f) playerLane = 1.6f;

    dist += playerSpeed * 18.0f * dt;
    timeLeft -= dt;
    playerZ += playerSpeed * dt;

    static float sp = 0; sp += dt;
    if (sp > fmaxf(0.5f, 1.2f - level * 0.06f)) { spawnObs(); sp = 0; }

    for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
        obs[i].z -= (playerSpeed + obs[i].speed) * 12.0f * dt;
        if (obs[i].z < 1.5f) {
            if (fabsf(obs[i].lane - playerLane) < 0.7f) {
                crashes++;
                sfx(2, 0.8f);
                playerSpeed *= 0.4f;
            } else {
                score += 12;
            }
            obs[i].on = false;
        }
    }

    for (int i = 0; i < MAX_SMOKE; i++) if (smoke[i].on) {
        smoke[i].life -= dt;
        smoke[i].y += 30 * dt;
        if (smoke[i].life <= 0) smoke[i].on = false;
    }

    if (dist >= target) {
        float bonus = fmaxf(0, timeLeft) * 6.0f * (1.0f + upRew * 0.22f);
        int r = (int)(160 + level * 70 + bonus - crashes * 45);
        if (r < 30) r = 30;
        money += r; score = r; sfx(5, 0.9f); state = ST_RESULT;
    } else if (timeLeft <= 0 || crashes >= 8) {
        int r = 20 - crashes * 4; if (r < 0) r = 0;
        money += r; score = r; state = ST_RESULT;
    }
    if (down & KEY_START) state = ST_MENU;
}

void project(float lane, float z, float* sx, float* sy, float* scale) {
    if (z < 0.5f) z = 0.5f;
    float persp = 220.0f / z;
    *sx = TOP_W / 2.0f + lane * 55.0f * persp / 8.0f;
    *sy = 40.0f + (TOP_H - 50.0f) * (1.0f - 1.0f / (1.0f + z * 0.04f));
    *scale = persp * 0.9f;
    if (*scale > 90) *scale = 90;
    if (*scale < 4) *scale = 4;
}

void drawRect(float x, float y, float w, float h, u32 c) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, c);
}

void drawTop() {
    C2D_TargetClear(topT, C2D_Color32(135, 206, 235, 255));
    C2D_SceneBegin(topT);

    drawRect(0, 0, TOP_W, 70, C2D_Color32(100, 180, 255, 255));
    drawRect(0, 70, TOP_W, 30, C2D_Color32(180, 210, 240, 255));

    for (int i = ROAD_SEGS; i >= 0; i--) {
        float z1 = 2.0f + i * 3.5f;
        float z2 = 2.0f + (i + 1) * 3.5f;

        float x1l, y1, s1, x1r, x2l, y2, s2, x2r;
        project(-1.8f, z1, &x1l, &y1, &s1);
        project(1.8f, z1, &x1r, &y1, &s1);
        project(-1.8f, z2, &x2l, &y2, &s2);
        project(1.8f, z2, &x2r, &y2, &s2);

        u32 roadCol = (i % 2 == 0) ? C2D_Color32(60, 60, 70, 255) : C2D_Color32(50, 50, 60, 255);
        float midY = (y1 + y2) / 2;
        float h = fabsf(y2 - y1) + 2;
        float left = (x1l + x2l) / 2;
        float right = (x1r + x2r) / 2;
        drawRect(left, midY - h/2, right - left, h, roadCol);

        if (i % 2 == 0) {
            float cx = TOP_W / 2.0f;
            drawRect(cx - 2, midY - h/2, 4, h * 0.6f, C2D_Color32(240, 230, 80, 255));
        }
    }

    drawRect(0, 100, 40, TOP_H - 100, C2D_Color32(34, 139, 34, 255));
    drawRect(TOP_W - 40, 100, 40, TOP_H - 100, C2D_Color32(34, 139, 34, 255));

    u32 cols[4] = {
        C2D_Color32(50, 120, 220, 255),
        C2D_Color32(30, 160, 120, 255),
        C2D_Color32(220, 170, 40, 255),
        C2D_Color32(150, 60, 200, 255)
    };
    for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
        float sx, sy, sc;
        project(obs[i].lane, obs[i].z, &sx, &sy, &sc);
        float w = sc * 0.7f, h = sc * 1.0f;
        drawRect(sx - w/2, sy - h, w, h, cols[obs[i].type % 4]);
        drawRect(sx - w*0.3f, sy - h*0.75f, w*0.6f, h*0.3f, C2D_Color32(160, 210, 255, 255));
    }

    for (int i = 0; i < MAX_SMOKE; i++) if (smoke[i].on) {
        u8 a = (u8)(smoke[i].life * 150);
        drawRect(smoke[i].x - 10, smoke[i].y - 10, 20, 20, C2D_Color32(200, 200, 200, a));
    }

    float hoodY = TOP_H - 55;
    drawRect(TOP_W/2 - 70, hoodY, 140, 55, C2D_Color32(200, 40, 40, 255));
    drawRect(TOP_W/2 - 50, hoodY + 10, 100, 25, C2D_Color32(40, 40, 50, 255));
    C2D_DrawCircleSolid(TOP_W/2, TOP_H - 15, 0.6f, 22, C2D_Color32(30, 30, 35, 255));
    C2D_DrawCircleSolid(TOP_W/2, TOP_H - 15, 0.65f, 8, C2D_Color32(180, 40, 40, 255));
}

void drawBottom() {
    C2D_TargetClear(botT, C2D_Color32(15, 15, 25, 255));
    C2D_SceneBegin(botT);

    float prog = dist / target; if (prog > 1) prog = 1;
    drawRect(15, 12, 290, 18, C2D_Color32(40, 40, 55, 255));
    drawRect(15, 12, 290 * prog, 18, C2D_Color32(40, 200, 100, 255));

    float tprog = timeLeft / (50.f + level * 8); if (tprog > 1) tprog = 1; if (tprog < 0) tprog = 0;
    drawRect(15, 38, 290, 12, C2D_Color32(40, 40, 55, 255));
    drawRect(15, 38, 290 * tprog, 12, C2D_Color32(230, 180, 40, 255));

    float sprog = playerSpeed / (4.2f + upSpeed * 0.45f); if (sprog > 1) sprog = 1;
    drawRect(15, 58, 140, 14, C2D_Color32(40, 40, 55, 255));
    drawRect(15, 58, 140 * sprog, 14, C2D_Color32(80, 160, 255, 255));

    float cx = 100, cy = 150;
    C2D_DrawCircleSolid(cx, cy, 0.4f, 52, C2D_Color32(35, 35, 45, 255));
    C2D_DrawCircleSolid(cx, cy, 0.45f, 40, C2D_Color32(55, 55, 65, 255));
    C2D_DrawCircleSolid(cx, cy, 0.5f, 12, C2D_Color32(200, 45, 45, 255));
    float a = steerVis * 3.14159f / 180.f;
    float c = cosf(a), s = sinf(a);
    drawRect(cx - 32 * c, cy - 32 * s - 2.5f, 64, 6, C2D_Color32(140, 140, 150, 255));
    drawRect(cx + 32 * s - 2.5f, cy - 32 * c, 6, 64, C2D_Color32(140, 140, 150, 255));

    bool ped = boosting;
    drawRect(220, 120, 55, 80, C2D_Color32(45, 45, 55, 255));
    drawRect(227, 128, 41, 64, ped ? C2D_Color32(230, 50, 50, 255) : C2D_Color32(90, 35, 35, 255));

    if (drifting) {
        drawRect(200, 55, 100, 20, C2D_Color32(200, 80, 20, 255));
    }
}

void drawMenu() {
    C2D_TargetClear(topT, C2D_Color32(15, 15, 30, 255));
    C2D_SceneBegin(topT);
    drawRect(50, 30, 300, 90, C2D_Color32(30, 30, 55, 255));
    drawRect(70, 50, 260, 50, C2D_Color32(200, 40, 40, 255));
    for (int i = 0; i < 8; i++) {
        float y = 140 + i * 12;
        float w = 80 + i * 30;
        drawRect(TOP_W/2 - w/2, y, w, 10, C2D_Color32(50 + i*5, 50, 60, 255));
    }

    C2D_TargetClear(botT, C2D_Color32(18, 18, 28, 255));
    C2D_SceneBegin(botT);
    drawRect(35, 35, 250, 42, C2D_Color32(40, 110, 200, 255));
    drawRect(35, 90, 250, 42, C2D_Color32(40, 150, 90, 255));
    drawRect(35, 145, 250, 42, C2D_Color32(150, 50, 50, 255));
}

void drawGarage() {
    C2D_TargetClear(topT, C2D_Color32(15, 15, 30, 255));
    C2D_SceneBegin(topT);
    drawRect(40, 25, 320, 190, C2D_Color32(30, 30, 50, 255));
    drawRect(TOP_W/2 - 40, 80, 80, 100, C2D_Color32(200, 40, 40, 255));

    C2D_TargetClear(botT, C2D_Color32(18, 18, 28, 255));
    C2D_SceneBegin(botT);
    drawRect(25, 30, 270, 36, C2D_Color32(50, 100, 180, 255));
    drawRect(25, 75, 270, 36, C2D_Color32(40, 140, 90, 255));
    drawRect(25, 120, 270, 36, C2D_Color32(160, 120, 30, 255));
    drawRect(25, 175, 270, 36, C2D_Color32(100, 40, 40, 255));
}

void drawResult() {
    C2D_TargetClear(topT, C2D_Color32(15, 15, 30, 255));
    C2D_SceneBegin(topT);
    drawRect(50, 40, 300, 160, C2D_Color32(35, 35, 60, 255));

    C2D_TargetClear(botT, C2D_Color32(18, 18, 28, 255));
    C2D_SceneBegin(botT);
    drawRect(40, 90, 240, 55, C2D_Color32(40, 150, 80, 255));
}

int main() {
    srand(osGetTime());
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topT = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    botT = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    romfsInit();
    initSnd();

    bool run = true;
    u64 last = osGetTime();
    int boot = 0;

    while (aptMainLoop() && run) {
        u64 now = osGetTime();
        float dt = (now - last) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last = now;

        hidScanInput();
        u32 down = hidKeysDown();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        switch (state) {
        case ST_BOOT:
            drawMenu();
            boot++;
            if (boot > 80) state = ST_MENU;
            break;
        case ST_MENU:
            drawMenu();
            if (down & KEY_A) { sfx(4); reset(); state = ST_DRIVE; }
            if (down & KEY_X) { sfx(4); state = ST_GARAGE; }
            if (down & KEY_START) run = false;
            break;
        case ST_GARAGE:
            drawGarage();
            if (down & KEY_A) { int c = 160 + upSpeed * 100; if (money >= c) { money -= c; upSpeed++; sfx(3); } }
            if (down & KEY_X) { int c = 130 + upHand * 80; if (money >= c) { money -= c; upHand++; sfx(3); } }
            if (down & KEY_Y) { int c = 220 + upRew * 150; if (money >= c) { money -= c; upRew++; sfx(3); } }
            if (down & (KEY_START | KEY_B)) { sfx(4); state = ST_MENU; }
            break;
        case ST_DRIVE:
            update(dt);
            drawTop();
            drawBottom();
            break;
        case ST_RESULT:
            drawResult();
            if (down & KEY_A) {
                if (dist >= target) level++;
                sfx(4); state = ST_MENU;
            }
            break;
        }

        C3D_FrameEnd(0);
    }

    exitSnd();
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
