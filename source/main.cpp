#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

#define VERSION "1.1.0"
#define MAX_OBSTACLES 14
#define MAX_SMOKE 20
#define NUM_LANES 4

enum GameState {
    STATE_BOOT,
    STATE_MENU,
    STATE_GARAGE,
    STATE_DRIVING,
    STATE_RESULTS
};

struct Car {
    float x, y;
    float speed;
    float angle;
    bool drifting;
    int colorIdx;
};

struct Obstacle {
    float x, y;
    float speed;
    int type;
    bool active;
};

struct Particle {
    float x, y;
    float vx, vy;
    float life;
    bool active;
};

GameState state = STATE_BOOT;
Car player;
Obstacle obstacles[MAX_OBSTACLES];
Particle smokes[MAX_SMOKE];

int money = 800;
int level = 1;
int crashes = 0;
int score = 0;
float distance = 0.0f;
float targetDist = 1800.0f;
float timeLeft = 50.0f;
float roadOffset = 0.0f;

int upgSpeed = 0;
int upgHandling = 0;
int upgReward = 0;
int carColor = 0;

C3D_RenderTarget* top;
C3D_RenderTarget* bottom;
C2D_SpriteSheet sheet = nullptr;
C2D_Sprite sprPlayer, sprCars[4], sprRoad, sprSmoke, sprPackage, sprCoin, sprLogo, sprBg, sprCrash;

bool soundReady = false;
ndspWaveBuf waveBufs[8];
u8* audioData[8] = {nullptr};
int sfxEngine = 0, sfxDrift = 1, sfxCrash = 2, sfxCoin = 3, sfxSelect = 4, sfxSuccess = 5;

bool loadWav(const char* path, int slot) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 44) { fclose(f); return false; }
    u8* buf = (u8*)linearAlloc(sz);
    if (!buf) { fclose(f); return false; }
    fread(buf, 1, sz, f);
    fclose(f);
    audioData[slot] = buf;
    waveBufs[slot].data_vaddr = buf + 44;
    waveBufs[slot].nsamples = (sz - 44) / 2;
    waveBufs[slot].status = NDSP_WBUF_DONE;
    return true;
}

void playSfx(int slot, float vol = 0.7f) {
    if (!soundReady || slot < 0 || slot > 5 || !audioData[slot]) return;
    ndspChnWaveBufClear(slot);
    waveBufs[slot].status = NDSP_WBUF_DONE;
    ndspChnSetInterp(slot, NDSP_INTERP_LINEAR);
    ndspChnSetRate(slot, 22050.0f);
    ndspChnSetFormat(slot, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {vol, vol, 0,0,0,0,0,0,0,0,0,0};
    ndspChnSetMix(slot, mix);
    ndspChnWaveBufAdd(slot, &waveBufs[slot]);
}

void initSound() {
    if (ndspInit() != 0) return;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    soundReady = true;
    loadWav("romfs:/sfx/engine.wav", 0);
    loadWav("romfs:/sfx/drift.wav", 1);
    loadWav("romfs:/sfx/crash.wav", 2);
    loadWav("romfs:/sfx/coin.wav", 3);
    loadWav("romfs:/sfx/select.wav", 4);
    loadWav("romfs:/sfx/success.wav", 5);
}

void exitSound() {
    for (int i = 0; i < 8; i++) if (audioData[i]) linearFree(audioData[i]);
    ndspExit();
}

void initSprites() {
    sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (!sheet) return;
    C2D_SpriteFromSheet(&sprPlayer, sheet, 0);
    C2D_SpriteFromSheet(&sprCars[0], sheet, 1);
    C2D_SpriteFromSheet(&sprCars[1], sheet, 2);
    C2D_SpriteFromSheet(&sprCars[2], sheet, 3);
    C2D_SpriteFromSheet(&sprCars[3], sheet, 4);
}

void spawnObstacle() {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            int lane = rand() % NUM_LANES;
            obstacles[i].x = 50.0f + lane * 75.0f + 37.0f;
            obstacles[i].y = -50.0f;
            obstacles[i].speed = 1.8f + (rand() % 80) / 40.0f + level * 0.25f;
            obstacles[i].type = rand() % 4;
            break;
        }
    }
}

void spawnSmoke(float x, float y) {
    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smokes[i].active) {
            smokes[i].active = true;
            smokes[i].x = x + (rand() % 20 - 10);
            smokes[i].y = y + 20;
            smokes[i].vx = (rand() % 20 - 10) * 0.1f;
            smokes[i].vy = 0.5f + (rand() % 10) * 0.1f;
            smokes[i].life = 0.6f + (rand() % 20) * 0.02f;
            break;
        }
    }
}

void resetLevel() {
    player.x = TOP_W / 2.0f;
    player.y = TOP_H - 70.0f;
    player.speed = 2.2f + upgSpeed * 0.35f;
    player.angle = 0;
    player.drifting = false;
    for (int i = 0; i < MAX_OBSTACLES; i++) obstacles[i].active = false;
    for (int i = 0; i < MAX_SMOKE; i++) smokes[i].active = false;
    distance = 0; crashes = 0; score = 0;
    targetDist = 1600.0f + level * 450.0f;
    timeLeft = 42.0f + level * 8.0f;
    roadOffset = 0;
}

bool collides(float x1, float y1, float x2, float y2, float r = 32.0f) {
    float dx = x1 - x2, dy = y1 - y2;
    return (dx*dx + dy*dy) < r*r;
}

void updateDriving(float dt) {
    hidScanInput();
    u32 kHeld = hidKeysHeld();
    u32 kDown = hidKeysDown();
    circlePosition cpos;
    hidCircleRead(&cpos);

    float steer = cpos.dx / 150.0f;
    if (steer > 1.0f) steer = 1.0f;
    if (steer < -1.0f) steer = -1.0f;

    bool wantDrift = (kHeld & (KEY_Y | KEY_B)) != 0;
    player.drifting = wantDrift && fabsf(steer) > 0.25f;

    float hand = 2.8f + upgHandling * 0.55f;
    if (player.drifting) {
        player.x += steer * hand * 2.4f * dt * 60.0f;
        player.angle = steer * 22.0f;
        if (rand() % 3 == 0) spawnSmoke(player.x, player.y);
        static float driftTimer = 0;
        driftTimer += dt;
        if (driftTimer > 0.35f) { playSfx(sfxDrift, 0.45f); driftTimer = 0; }
    } else {
        player.x += steer * hand * dt * 60.0f;
        player.angle *= 0.88f;
    }

    if (player.x < 35) player.x = 35;
    if (player.x > TOP_W - 35) player.x = TOP_W - 35;

    float fwd = player.speed * (player.drifting ? 1.18f : 1.0f);
    distance += fwd * dt * 60.0f;
    roadOffset += fwd * dt * 60.0f;
    timeLeft -= dt;

    static float spawnT = 0;
    spawnT += dt;
    float rate = fmaxf(0.45f, 1.15f - level * 0.06f);
    if (spawnT > rate) { spawnObstacle(); spawnT = 0; }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        obstacles[i].y += (obstacles[i].speed + fwd * 0.35f) * dt * 60.0f;
        if (obstacles[i].y > TOP_H + 60) { obstacles[i].active = false; score += 8; }
        if (collides(player.x, player.y, obstacles[i].x, obstacles[i].y, 38.0f)) {
            obstacles[i].active = false;
            crashes++;
            playSfx(sfxCrash, 0.8f);
            player.x += (player.x > obstacles[i].x) ? 18.0f : -18.0f;
        }
    }

    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smokes[i].active) continue;
        smokes[i].x += smokes[i].vx;
        smokes[i].y += smokes[i].vy;
        smokes[i].life -= dt;
        if (smokes[i].life <= 0) smokes[i].active = false;
    }

    if (distance >= targetDist) {
        float bonus = fmaxf(0, timeLeft) * 4.5f * (1.0f + upgReward * 0.18f);
        int reward = (int)(120 + level * 55 + bonus - crashes * 35);
        if (reward < 15) reward = 15;
        money += reward; score = reward;
        playSfx(sfxSuccess, 0.9f);
        state = STATE_RESULTS;
    } else if (timeLeft <= 0 || crashes >= 7) {
        int reward = (15 - crashes * 4) > 0 ? (15 - crashes * 4) : 0;
        money += reward; score = reward;
        state = STATE_RESULTS;
    }

    if (kDown & KEY_START) state = STATE_MENU;
}

void drawDriving() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(top, C2D_Color32(30, 30, 45, 255));
    C2D_SceneBegin(top);

    C2D_DrawRectSolid(30, 0, 0.1f, TOP_W - 60, TOP_H, C2D_Color32(45, 45, 55, 255));

    for (int i = 1; i < NUM_LANES; i++) {
        float lx = 30 + i * ((TOP_W - 60) / (float)NUM_LANES);
        for (int yy = -40; yy < TOP_H + 40; yy += 36) {
            float y = fmodf(yy + roadOffset, 36.0f) - 20;
            C2D_DrawRectSolid(lx - 2, y, 0.2f, 4, 18, C2D_Color32(220, 220, 100, 255));
        }
    }

    C2D_DrawRectSolid(0, 0, 0.15f, 30, TOP_H, C2D_Color32(80, 40, 40, 255));
    C2D_DrawRectSolid(TOP_W - 30, 0, 0.15f, 30, TOP_H, C2D_Color32(80, 40, 40, 255));

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        if (sheet) {
            C2D_Sprite* s = &sprCars[obstacles[i].type % 4];
            C2D_SpriteSetPos(s, obstacles[i].x - 24, obstacles[i].y - 32);
            C2D_DrawSprite(s);
        } else {
            u32 cols[] = {C2D_Color32(70,120,180,255), C2D_Color32(40,160,120,255),
                          C2D_Color32(200,160,50,255), C2D_Color32(150,70,200,255)};
            C2D_DrawRectSolid(obstacles[i].x - 20, obstacles[i].y - 28, 0.4f, 40, 56, cols[obstacles[i].type % 4]);
        }
    }

    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smokes[i].active) continue;
        u8 a = (u8)(smokes[i].life * 180);
        C2D_DrawRectSolid(smokes[i].x - 8, smokes[i].y - 8, 0.3f, 16, 16, C2D_Color32(180, 180, 180, a));
    }

    if (sheet) {
        C2D_SpriteSetPos(&sprPlayer, player.x - 24, player.y - 32);
        C2D_SpriteSetRotationDegrees(&sprPlayer, player.angle);
        C2D_DrawSprite(&sprPlayer);
    } else {
        C2D_DrawRectSolid(player.x - 18, player.y - 26, 0.5f, 36, 52, C2D_Color32(230, 60, 60, 255));
        C2D_DrawRectSolid(player.x - 12, player.y - 12, 0.55f, 24, 16, C2D_Color32(120, 190, 255, 255));
    }

    if (player.drifting) {
        C2D_DrawRectSolid(player.x - 30, player.y + 22, 0.35f, 14, 8, C2D_Color32(200, 200, 200, 160));
        C2D_DrawRectSolid(player.x + 16, player.y + 22, 0.35f, 14, 8, C2D_Color32(200, 200, 200, 160));
    }

    C2D_TargetClear(bottom, C2D_Color32(15, 15, 25, 255));
    C2D_SceneBegin(bottom);

    float prog = distance / targetDist; if (prog > 1) prog = 1;
    C2D_DrawRectSolid(20, 20, 0.4f, 280, 18, C2D_Color32(40, 40, 50, 255));
    C2D_DrawRectSolid(20, 20, 0.5f, 280 * prog, 18, C2D_Color32(50, 200, 100, 255));

    C3D_FrameEnd(0);
}

void printHeader() {
    printf("\x1b[1;1H");
    printf("  ==============================\n");
    printf("       TEMU DRIVER 3DS\n");
    printf("  ==============================\n\n");
}

int main(int argc, char** argv) {
    srand(osGetTime());

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    romfsInit();
    initSound();
    initSprites();

    consoleInit(GFX_BOTTOM, NULL);

    bool running = true;
    u64 last = osGetTime();
    int bootFrames = 0;

    while (aptMainLoop() && running) {
        u64 now = osGetTime();
        float dt = (now - last) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last = now;

        hidScanInput();
        u32 kDown = hidKeysDown();

        switch (state) {
        case STATE_BOOT:
            consoleClear();
            printHeader();
            printf("  Checking for updates...\n\n");
            printf("  Version %s\n\n", VERSION);
            printf("  Loading assets & sound...\n");
            bootFrames++;
            if (bootFrames > 140) state = STATE_MENU;
            break;

        case STATE_MENU:
            consoleClear();
            printHeader();
            printf("  Money : $%d\n", money);
            printf("  Level : %d\n\n", level);
            printf("  A     Start Delivery\n");
            printf("  X     Garage / Upgrades\n");
            printf("  START Exit\n\n");
            printf("  In-game:\n");
            printf("  Circle Pad  Steer\n");
            printf("  Y / B       Drift\n");
            printf("  START       Abort\n");
            if (kDown & KEY_A) { playSfx(sfxSelect); resetLevel(); state = STATE_DRIVING; }
            if (kDown & KEY_X) { playSfx(sfxSelect); state = STATE_GARAGE; }
            if (kDown & KEY_START) running = false;
            break;

        case STATE_GARAGE:
            consoleClear();
            printHeader();
            printf("  ===== GARAGE =====\n\n");
            printf("  Money: $%d\n\n", money);
            printf("  Speed    Lv%d  (A) $%d\n", upgSpeed, 140 + upgSpeed * 90);
            printf("  Handling Lv%d  (X) $%d\n", upgHandling, 110 + upgHandling * 70);
            printf("  Reward   Lv%d  (Y) $%d\n", upgReward, 180 + upgReward * 130);
            printf("\n  B  Cycle car color\n");
            printf("  START  Back\n");
            if (kDown & KEY_A) {
                int c = 140 + upgSpeed * 90;
                if (money >= c) { money -= c; upgSpeed++; playSfx(sfxCoin); }
            }
            if (kDown & KEY_X) {
                int c = 110 + upgHandling * 70;
                if (money >= c) { money -= c; upgHandling++; playSfx(sfxCoin); }
            }
            if (kDown & KEY_Y) {
                int c = 180 + upgReward * 130;
                if (money >= c) { money -= c; upgReward++; playSfx(sfxCoin); }
            }
            if (kDown & KEY_B) { carColor = (carColor + 1) % 5; playSfx(sfxSelect); }
            if (kDown & KEY_START) { playSfx(sfxSelect); state = STATE_MENU; }
            break;

        case STATE_DRIVING:
            updateDriving(dt);
            drawDriving();
            consoleClear();
            printf("\n  TEMU DELIVERY  Lv%d\n", level);
            printf("  Dist %.0f / %.0f\n", distance, targetDist);
            printf("  Time  %.1fs\n", timeLeft);
            printf("  Crash %d   $%d\n", crashes, money);
            printf("\n  Pad=Steer  Y/B=Drift\n");
            if (player.drifting) printf("  >>> DRIFTING <<<\n");
            break;

        case STATE_RESULTS:
            consoleClear();
            printHeader();
            printf("  ===== RESULT =====\n\n");
            printf("  Reward : $%d\n", score);
            printf("  Crashes: %d\n", crashes);
            printf("  Money  : $%d\n\n", money);
            if (distance >= targetDist) {
                printf("  SUCCESS!\n");
                printf("  A = Next Level\n");
                if (kDown & KEY_A) { level++; playSfx(sfxSelect); state = STATE_MENU; }
            } else {
                printf("  Failed.\n");
                printf("  A = Menu\n");
                if (kDown & KEY_A) { playSfx(sfxSelect); state = STATE_MENU; }
            }
            break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    if (sheet) C2D_SpriteSheetFree(sheet);
    exitSound();
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
