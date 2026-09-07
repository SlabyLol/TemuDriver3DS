#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TW 400
#define TH 240
#define MAX_OBS 14
#define MAX_SMK 20
#define SEGS 28

enum State { ST_BOOT, ST_MENU, ST_GARAGE, ST_DRIVE, ST_RESULT };

struct Obs { float z, lane, spd; int typ; bool on; };
struct Smk { float x, y, life; bool on; };

State state = ST_BOOT;
Obs obs[MAX_OBS];
Smk smk[MAX_SMK];

float lane = 0, speed = 0, steerV = 0;
bool drift = false, boost = false;
int money = 1500, level = 1, crash = 0, score = 0;
float dist = 0, target = 2200, timeL = 60;
int uSpd = 0, uHnd = 0, uRew = 0;
float roadOff = 0;

C3D_RenderTarget *topT, *botT;
C2D_TextBuf g_staticBuf, g_dynBuf;
C2D_Text txtTitle, txtStart, txtGarage, txtExit, txtBack;

bool snd = false;
ndspWaveBuf wb[6];
u8* ad[6] = {};

bool loadW(const char* p, int s) {
    FILE* f = fopen(p, "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 44) { fclose(f); return false; }
    u8* b = (u8*)linearAlloc(n); if (!b) { fclose(f); return false; }
    fread(b, 1, n, f); fclose(f);
    ad[s] = b; wb[s].data_vaddr = b + 44; wb[s].nsamples = (n - 44) / 2;
    wb[s].status = NDSP_WBUF_DONE; return true;
}
void sfx(int s, float v = 0.55f) {
    if (!snd || !ad[s]) return;
    ndspChnWaveBufClear(s); wb[s].status = NDSP_WBUF_DONE;
    ndspChnSetInterp(s, NDSP_INTERP_LINEAR); ndspChnSetRate(s, 22050.f);
    ndspChnSetFormat(s, NDSP_FORMAT_MONO_PCM16);
    float m[12] = {v, v}; ndspChnSetMix(s, m); ndspChnWaveBufAdd(s, &wb[s]);
}
void initS() {
    if (ndspInit()) return; ndspSetOutputMode(NDSP_OUTPUT_STEREO); snd = true;
    loadW("romfs:/sfx/engine.wav", 0); loadW("romfs:/sfx/drift.wav", 1);
    loadW("romfs:/sfx/crash.wav", 2); loadW("romfs:/sfx/coin.wav", 3);
    loadW("romfs:/sfx/select.wav", 4); loadW("romfs:/sfx/success.wav", 5);
}

void initText() {
    g_staticBuf = C2D_TextBufNew(2048);
    g_dynBuf = C2D_TextBufNew(2048);
    C2D_TextParse(&txtTitle, g_staticBuf, "TEMU DRIVER 3DS");
    C2D_TextOptimize(&txtTitle);
    C2D_TextParse(&txtStart, g_staticBuf, "A  START DELIVERY");
    C2D_TextOptimize(&txtStart);
    C2D_TextParse(&txtGarage, g_staticBuf, "X  GARAGE / UPGRADES");
    C2D_TextOptimize(&txtGarage);
    C2D_TextParse(&txtExit, g_staticBuf, "START  EXIT");
    C2D_TextOptimize(&txtExit);
    C2D_TextParse(&txtBack, g_staticBuf, "B / START  BACK");
    C2D_TextOptimize(&txtBack);
}

void drawDyn(const char* str, float x, float y, float sc, u32 col) {
    C2D_TextBufClear(g_dynBuf);
    C2D_Text t;
    C2D_TextParse(&t, g_dynBuf, str);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.6f, sc, sc, col);
}

void spObs() {
    for (int i = 0; i < MAX_OBS; i++) if (!obs[i].on) {
        obs[i].on = true; obs[i].z = 90 + rand() % 50;
        obs[i].lane = (rand() % 5) - 2.0f;
        obs[i].spd = 0.7f + (rand() % 60) / 100.f + level * 0.12f;
        obs[i].typ = rand() % 4; break;
    }
}
void spSmk() {
    for (int i = 0; i < MAX_SMK; i++) if (!smk[i].on) {
        smk[i].on = true;
        smk[i].x = TW/2 + lane * 30 + (rand()%30-15);
        smk[i].y = TH - 40 - (rand()%15);
        smk[i].life = 0.5f + (rand()%20)*0.02f; break;
    }
}
void reset() {
    lane = 0; speed = 1.2f; steerV = 0; drift = boost = false;
    for (int i = 0; i < MAX_OBS; i++) obs[i].on = false;
    for (int i = 0; i < MAX_SMK; i++) smk[i].on = false;
    dist = 0; crash = 0; score = 0;
    target = 2000 + level * 550; timeL = 55 + level * 7; roadOff = 0;
}

void upd(float dt) {
    hidScanInput(); u32 h = hidKeysHeld(), d = hidKeysDown();
    circlePosition c; hidCircleRead(&c);
    float st = c.dx / 150.f; if (st > 1) st = 1; if (st < -1) st = -1;
    steerV += (st * 65.f - steerV) * 0.22f;

    boost = h & (KEY_A | KEY_R);
    drift = (h & (KEY_Y | KEY_B | KEY_L)) && fabsf(st) > 0.2f;

    float maxS = 4.5f + uSpd * 0.5f;
    float tgt = boost ? maxS : maxS * 0.65f;
    if (speed < tgt) speed += (boost ? 4.0f : 2.0f) * dt;
    else speed -= 2.0f * dt;
    if (speed < 0.8f) speed = 0.8f;

    float hand = 2.4f + uHnd * 0.55f;
    if (drift) {
        lane += st * hand * 1.9f * dt;
        if (rand() % 2 == 0) spSmk();
        static float t = 0; t += dt; if (t > 0.25f) { sfx(1, 0.3f); t = 0; }
    } else lane += st * hand * dt;
    if (lane < -2.0f) lane = -2.0f; if (lane > 2.0f) lane = 2.0f;

    dist += speed * 20.f * dt; timeL -= dt; roadOff += speed * 25.f * dt;

    static float sp = 0; sp += dt;
    if (sp > fmaxf(0.45f, 1.1f - level * 0.05f)) { spObs(); sp = 0; }

    for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
        obs[i].z -= (speed + obs[i].spd) * 14.f * dt;
        if (obs[i].z < 2.0f) {
            if (fabsf(obs[i].lane - lane) < 0.75f) {
                crash++; sfx(2, 0.75f); speed *= 0.35f;
            } else score += 15;
            obs[i].on = false;
        }
    }
    for (int i = 0; i < MAX_SMK; i++) if (smk[i].on) {
        smk[i].life -= dt; smk[i].y += 40 * dt;
        if (smk[i].life <= 0) smk[i].on = false;
    }

    if (dist >= target) {
        float b = fmaxf(0, timeL) * 7.f * (1.f + uRew * 0.25f);
        int r = (int)(180 + level * 80 + b - crash * 50); if (r < 40) r = 40;
        money += r; score = r; sfx(5, 0.85f); state = ST_RESULT;
    } else if (timeL <= 0 || crash >= 8) {
        int r = 25 - crash * 4; if (r < 0) r = 0;
        money += r; score = r; state = ST_RESULT;
    }
    if (d & KEY_START) state = ST_MENU;
}

void proj(float ln, float z, float* x, float* y, float* sc) {
    if (z < 1.0f) z = 1.0f;
    float p = 280.f / z;
    *x = TW / 2.f + ln * 48.f * (p / 10.f);
    *y = 55.f + (TH - 80.f) * (1.f - 1.f / (1.f + z * 0.035f));
    *sc = p * 0.85f;
    if (*sc > 100) *sc = 100; if (*sc < 3) *sc = 3;
}

void R(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x, y, 0.5f, w, h, c); }

void drawTop() {
    C2D_TargetClear(topT, C2D_Color32(70, 150, 230, 255));
    C2D_SceneBegin(topT);
    R(0, 0, TW, 50, C2D_Color32(100, 180, 255, 255));
    R(0, 50, TW, 40, C2D_Color32(140, 200, 245, 255));
    R(0, 95, TW, TH - 95, C2D_Color32(40, 120, 40, 255));

    for (int i = SEGS; i >= 0; i--) {
        float z1 = 1.5f + i * 3.2f, z2 = 1.5f + (i + 1) * 3.2f;
        float x1l, y1, s1, x1r, x2l, y2, s2, x2r;
        proj(-2.1f, z1, &x1l, &y1, &s1); proj(2.1f, z1, &x1r, &y1, &s1);
        proj(-2.1f, z2, &x2l, &y2, &s2); proj(2.1f, z2, &x2r, &y2, &s2);
        float midY = (y1 + y2) * 0.5f, hh = fabsf(y2 - y1) + 1.5f;
        float L = (x1l + x2l) * 0.5f, RR = (x1r + x2r) * 0.5f;
        u32 col = ((i + (int)(roadOff / 20)) % 2 == 0) ? C2D_Color32(55, 55, 65, 255) : C2D_Color32(45, 45, 55, 255);
        R(L, midY - hh * 0.5f, RR - L, hh, col);
        if ((i + (int)(roadOff / 15)) % 2 == 0) R(TW * 0.5f - 2.5f, midY - hh * 0.35f, 5, hh * 0.5f, C2D_Color32(255, 230, 60, 255));
        R(L, midY - hh * 0.5f, 3, hh, C2D_Color32(240, 240, 240, 255));
        R(RR - 3, midY - hh * 0.5f, 3, hh, C2D_Color32(240, 240, 240, 255));
    }

    u32 cc[4] = { C2D_Color32(40, 110, 220, 255), C2D_Color32(20, 160, 110, 255),
                  C2D_Color32(230, 170, 30, 255), C2D_Color32(160, 50, 200, 255) };
    for (int i = 0; i < MAX_OBS; i++) if (obs[i].on) {
        float sx, sy, sc; proj(obs[i].lane, obs[i].z, &sx, &sy, &sc);
        float w = sc * 0.75f, h = sc * 1.05f;
        R(sx - w * 0.5f, sy - h, w, h, cc[obs[i].typ % 4]);
        R(sx - w * 0.32f, sy - h * 0.8f, w * 0.64f, h * 0.28f, C2D_Color32(170, 220, 255, 255));
    }
    for (int i = 0; i < MAX_SMK; i++) if (smk[i].on) {
        u8 a = (u8)(smk[i].life * 140);
        R(smk[i].x - 12, smk[i].y - 12, 24, 24, C2D_Color32(190, 190, 190, a));
    }
    R(TW/2 - 85, TH - 58, 170, 58, C2D_Color32(190, 35, 35, 255));
    R(TW/2 - 70, TH - 48, 140, 20, C2D_Color32(30, 30, 40, 255));
    C2D_DrawCircleSolid(TW/2.f, TH - 12.f, 0.6f, 18, C2D_Color32(25, 25, 30, 255));
    C2D_DrawCircleSolid(TW/2.f, TH - 12.f, 0.65f, 6, C2D_Color32(180, 35, 35, 255));
}

void drawBot() {
    C2D_TargetClear(botT, C2D_Color32(12, 12, 22, 255));
    C2D_SceneBegin(botT);

    float p = dist / target; if (p > 1) p = 1;
    R(12, 8, 296, 14, C2D_Color32(35, 35, 50, 255));
    R(12, 8, 296 * p, 14, C2D_Color32(30, 190, 90, 255));

    float tp = timeL / (55.f + level * 7); if (tp > 1) tp = 1; if (tp < 0) tp = 0;
    R(12, 28, 296, 10, C2D_Color32(35, 35, 50, 255));
    R(12, 28, 296 * tp, 10, C2D_Color32(230, 175, 30, 255));

    float sp = speed / (4.5f + uSpd * 0.5f); if (sp > 1) sp = 1;
    R(12, 44, 120, 10, C2D_Color32(35, 35, 50, 255));
    R(12, 44, 120 * sp, 10, C2D_Color32(60, 140, 240, 255));

    char buf[64];
    snprintf(buf, sizeof(buf), "DIST %.0f / %.0f", dist, target);
    drawDyn(buf, 14, 58, 0.4f, C2D_Color32(255, 255, 255, 255));
    snprintf(buf, sizeof(buf), "TIME %.0fs  LV%d  $%d", timeL, level, money);
    drawDyn(buf, 14, 72, 0.38f, C2D_Color32(220, 220, 220, 255));
    snprintf(buf, sizeof(buf), "CRASH %d", crash);
    drawDyn(buf, 200, 44, 0.4f, C2D_Color32(255, 100, 100, 255));

    float cx = 90, cy = 150;
    C2D_DrawCircleSolid(cx, cy, 0.4f, 50, C2D_Color32(30, 30, 40, 255));
    C2D_DrawCircleSolid(cx, cy, 0.45f, 38, C2D_Color32(50, 50, 60, 255));
    C2D_DrawCircleSolid(cx, cy, 0.5f, 11, C2D_Color32(200, 40, 40, 255));
    float a = steerV * 0.017453f, co = cosf(a), si = sinf(a);
    R(cx - 32 * co, cy - 32 * si - 2.5f, 64, 5, C2D_Color32(150, 150, 160, 255));
    R(cx + 32 * si - 2.5f, cy - 32 * co, 5, 64, C2D_Color32(150, 150, 160, 255));

    R(210, 115, 55, 80, C2D_Color32(40, 40, 50, 255));
    R(217, 122, 41, 66, boost ? C2D_Color32(230, 45, 45, 255) : C2D_Color32(85, 30, 30, 255));
    if (drift) {
        R(180, 55, 120, 16, C2D_Color32(220, 90, 20, 255));
        drawDyn("DRIFT", 200, 55, 0.4f, C2D_Color32(255, 255, 255, 255));
    }
    drawDyn("A/R GAS   Y/B DRIFT", 12, 215, 0.35f, C2D_Color32(180, 180, 180, 255));
}

void drawMenu() {
    C2D_TargetClear(topT, C2D_Color32(12, 12, 25, 255));
    C2D_SceneBegin(topT);
    R(40, 25, 320, 70, C2D_Color32(25, 25, 50, 255));
    C2D_DrawText(&txtTitle, C2D_WithColor | C2D_AlignCenter, 200, 40, 0.6f, 0.7f, 0.7f, C2D_Color32(255, 60, 60, 255));
    for (int i = 0; i < 10; i++) {
        float y = 110 + i * 11, w = 50 + i * 24;
        R(TW/2 - w/2, y, w, 9, C2D_Color32(45 + i * 3, 45, 55, 255));
        if (i % 2 == 0) R(TW/2 - 2, y, 4, 7, C2D_Color32(240, 220, 50, 255));
    }

    C2D_TargetClear(botT, C2D_Color32(15, 15, 25, 255));
    C2D_SceneBegin(botT);
    R(30, 30, 260, 42, C2D_Color32(35, 100, 190, 255));
    C2D_DrawText(&txtStart, C2D_WithColor, 45, 38, 0.6f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
    R(30, 90, 260, 42, C2D_Color32(30, 140, 80, 255));
    C2D_DrawText(&txtGarage, C2D_WithColor, 45, 98, 0.6f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
    R(30, 150, 260, 42, C2D_Color32(140, 40, 40, 255));
    C2D_DrawText(&txtExit, C2D_WithColor, 45, 158, 0.6f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));

    char buf[48];
    snprintf(buf, sizeof(buf), "Money $%d  Level %d", money, level);
    drawDyn(buf, 30, 210, 0.45f, C2D_Color32(200, 200, 200, 255));
}

void drawGarage() {
    C2D_TargetClear(topT, C2D_Color32(12, 12, 25, 255));
    C2D_SceneBegin(topT);
    R(30, 20, 340, 200, C2D_Color32(25, 25, 45, 255));
    R(TW/2 - 45, 70, 90, 110, C2D_Color32(200, 35, 35, 255));
    drawDyn("GARAGE", 160, 30, 0.6f, C2D_Color32(255, 255, 255, 255));

    C2D_TargetClear(botT, C2D_Color32(15, 15, 25, 255));
    C2D_SceneBegin(botT);
    char buf[64];
    snprintf(buf, sizeof(buf), "A  SPEED Lv%d  $%d", uSpd, 170 + uSpd * 110);
    R(20, 25, 280, 36, C2D_Color32(45, 95, 170, 255));
    drawDyn(buf, 30, 32, 0.42f, C2D_Color32(255, 255, 255, 255));
    snprintf(buf, sizeof(buf), "X  HANDLING Lv%d  $%d", uHnd, 140 + uHnd * 85);
    R(20, 75, 280, 36, C2D_Color32(35, 130, 80, 255));
    drawDyn(buf, 30, 82, 0.42f, C2D_Color32(255, 255, 255, 255));
    snprintf(buf, sizeof(buf), "Y  REWARD Lv%d  $%d", uRew, 240 + uRew * 160);
    R(20, 125, 280, 36, C2D_Color32(150, 110, 25, 255));
    drawDyn(buf, 30, 132, 0.42f, C2D_Color32(255, 255, 255, 255));
    R(20, 180, 280, 36, C2D_Color32(90, 35, 35, 255));
    C2D_DrawText(&txtBack, C2D_WithColor, 30, 188, 0.6f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));
}

void drawResult() {
    C2D_TargetClear(topT, C2D_Color32(12, 12, 25, 255));
    C2D_SceneBegin(topT);
    R(40, 35, 320, 170, C2D_Color32(30, 30, 55, 255));
    char buf[64];
    snprintf(buf, sizeof(buf), "REWARD  $%d", score);
    drawDyn(buf, 120, 60, 0.7f, C2D_Color32(100, 255, 120, 255));
    snprintf(buf, sizeof(buf), "Crashes %d", crash);
    drawDyn(buf, 140, 100, 0.5f, C2D_Color32(255, 150, 150, 255));
    snprintf(buf, sizeof(buf), "Total Money $%d", money);
    drawDyn(buf, 110, 140, 0.5f, C2D_Color32(255, 255, 255, 255));

    C2D_TargetClear(botT, C2D_Color32(15, 15, 25, 255));
    C2D_SceneBegin(botT);
    R(35, 90, 250, 50, C2D_Color32(35, 140, 70, 255));
    drawDyn("A  CONTINUE", 90, 105, 0.55f, C2D_Color32(255, 255, 255, 255));
}

int main() {
    srand(osGetTime());
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topT = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    botT = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    romfsInit(); initS(); initText();

    bool run = true; u64 last = osGetTime(); int boot = 0;
    while (aptMainLoop() && run) {
        u64 now = osGetTime(); float dt = (now - last) / 1000.f; if (dt > 0.05f) dt = 0.05f; last = now;
        hidScanInput(); u32 d = hidKeysDown();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        switch (state) {
        case ST_BOOT: drawMenu(); if (++boot > 60) state = ST_MENU; break;
        case ST_MENU:
            drawMenu();
            if (d & KEY_A) { sfx(4); reset(); state = ST_DRIVE; }
            if (d & KEY_X) { sfx(4); state = ST_GARAGE; }
            if (d & KEY_START) run = false;
            break;
        case ST_GARAGE:
            drawGarage();
            if (d & KEY_A) { int c = 170 + uSpd * 110; if (money >= c) { money -= c; uSpd++; sfx(3); } }
            if (d & KEY_X) { int c = 140 + uHnd * 85; if (money >= c) { money -= c; uHnd++; sfx(3); } }
            if (d & KEY_Y) { int c = 240 + uRew * 160; if (money >= c) { money -= c; uRew++; sfx(3); } }
            if (d & (KEY_START | KEY_B)) { sfx(4); state = ST_MENU; }
            break;
        case ST_DRIVE: upd(dt); drawTop(); drawBot(); break;
        case ST_RESULT:
            drawResult();
            if (d & KEY_A) { if (dist >= target) level++; sfx(4); state = ST_MENU; }
            break;
        }
        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(g_staticBuf); C2D_TextBufDelete(g_dynBuf);
    for (int i = 0; i < 6; i++) if (ad[i]) linearFree(ad[i]);
    ndspExit(); romfsExit(); C2D_Fini(); C3D_Fini(); gfxExit();
    return 0;
}
