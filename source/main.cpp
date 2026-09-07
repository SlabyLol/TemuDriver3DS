#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TOP_W 400
#define TOP_H 240

#define VERSION "1.2.0"
#define MAX_OBSTACLES 14
#define MAX_SMOKE 24
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
};

struct Obstacle {
    float x, y;
    float speed;
    int type;
    bool active;
};

struct Particle {
    float x, y, vx, vy, life;
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

C3D_RenderTarget* topTarget = nullptr;
C3D_RenderTarget* bottomTarget = nullptr;

bool soundReady = false;
ndspWaveBuf waveBufs[6];
u8* audioData[6] = {nullptr};

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

void playSfx(int slot, float vol = 0.65f) {
    if (!soundReady || slot < 0 || slot > 5 || !audioData[slot]) return;
    ndspChnWaveBufClear(slot);
    waveBufs[slot].status = NDSP_WBUF_DONE;
    ndspChnSetInterp(slot, NDSP_INTERP_LINEAR);
    ndspChnSetRate(slot, 22050.0f);
    ndspChnSetFormat(slot, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {vol, vol};
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
    for (int i = 0; i < 6; i++) if (audioData[i]) linearFree(audioData[i]);
    ndspExit();
}

void spawnObstacle() {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            int lane = rand() % NUM_LANES;
            obstacles[i].x = 55.0f + lane * 72.0f + 36.0f;
            obstacles[i].y = -55.0f;
            obstacles[i].speed = 1.9f + (rand() % 90) / 45.0f + level * 0.22f;
            obstacles[i].type = rand() % 4;
            break;
        }
    }
}

void spawnSmoke(float x, float y) {
    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smokes[i].active) {
            smokes[i].active = true;
            smokes[i].x = x + (rand() % 24 - 12);
            smokes[i].y = y + 18;
            smokes[i].vx = (rand() % 30 - 15) * 0.08f;
            smokes[i].vy = 0.4f + (rand() % 15) * 0.08f;
            smokes[i].life = 0.5f + (rand() % 25) * 0.02f;
            break;
        }
    }
}

void resetLevel() {
    player.x = TOP_W / 2.0f;
    player.y = TOP_H - 68.0f;
    player.speed = 2.3f + upgSpeed * 0.38f;
    player.angle = 0.0f;
    player.drifting = false;

    for (int i = 0; i < MAX_OBSTACLES; i++) obstacles[i].active = false;
    for (int i = 0; i < MAX_SMOKE; i++) smokes[i].active = false;

    distance = 0.0f;
    crashes = 0;
    score = 0;
    targetDist = 1600.0f + level * 450.0f;
    timeLeft = 42.0f + level * 8.0f;
    roadOffset = 0.0f;
}

bool collides(float x1, float y1, float x2, float y2, float r) {
    float dx = x1 - x2, dy = y1 - y2;
    return (dx*dx + dy*dy) < (r*r);
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
    player.drifting = wantDrift && fabsf(steer) > 0.22f;

    float hand = 2.9f + upgHandling * 0.55f;
    if (player.drifting) {
        player.x += steer * hand * 2.5f * dt * 60.0f;
        player.angle = steer * 24.0f;
        if ((rand() % 2) == 0) spawnSmoke(player.x, player.y);
        static float driftTimer = 0.0f;
        driftTimer += dt;
        if (driftTimer > 0.32f) {
            playSfx(1, 0.4f);
            driftTimer = 0.0f;
        }
    } else {
        player.x += steer * hand * dt * 60.0f;
        player.angle *= 0.86f;
    }

    if (player.x < 38.0f) player.x = 38.0f;
    if (player.x > TOP_W - 38.0f) player.x = TOP_W - 38.0f;

    float fwd = player.speed * (player.drifting ? 1.2f : 1.0f);
    distance += fwd * dt * 60.0f;
    roadOffset += fwd * dt * 60.0f;
    timeLeft -= dt;

    static float spawnT = 0.0f;
    spawnT += dt;
    float rate = fmaxf(0.42f, 1.1f - level * 0.055f);
    if (spawnT > rate) {
        spawnObstacle();
        spawnT = 0.0f;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        obstacles[i].y += (obstacles[i].speed + fwd * 0.38f) * dt * 60.0f;
        if (obstacles[i].y > TOP_H + 70.0f) {
            obstacles[i].active = false;
            score += 8;
        }
        if (collides(player.x, player.y, obstacles[i].x, obstacles[i].y, 36.0f)) {
            obstacles[i].active = false;
            crashes++;
            playSfx(2, 0.75f);
            player.x += (player.x > obstacles[i].x) ? 20.0f : -20.0f;
        }
    }

    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smokes[i].active) continue;
        smokes[i].x += smokes[i].vx;
        smokes[i].y += smokes[i].vy;
        smokes[i].life -= dt;
        if (smokes[i].life <= 0.0f) smokes[i].active = false;
    }

    if (distance >= targetDist) {
        float bonus = fmaxf(0.0f, timeLeft) * 4.8f * (1.0f + upgReward * 0.2f);
        int reward = (int)(130 + level * 60 + bonus - crashes * 38);
        if (reward < 20) reward = 20;
        money += reward;
        score = reward;
        playSfx(5, 0.85f);
        state = STATE_RESULTS;
    } else if (timeLeft <= 0.0f || crashes >= 7) {
        int reward = 12 - crashes * 3;
        if (reward < 0) reward = 0;
        money += reward;
        score = reward;
        state = STATE_RESULTS;
    }

    if (kDown & KEY_START) state = STATE_MENU;
}

void drawTop() {
    C2D_TargetClear(topTarget, C2D_Color32(28, 28, 42, 255));
    C2D_SceneBegin(topTarget);

    // Road
    C2D_DrawRectSolid(32, 0, 0.1f, TOP_W - 64, TOP_H, C2D_Color32(48, 48, 58, 255));

    // Lane lines
    for (int i = 1; i < NUM_LANES; i++) {
        float lx = 32.0f + i * ((TOP_W - 64.0f) / NUM_LANES);
        for (int yy = -50; yy < TOP_H + 50; yy += 38) {
            float y = fmodf((float)yy + roadOffset, 38.0f) - 22.0f;
            C2D_DrawRectSolid(lx - 2.0f, y, 0.2f, 4.0f, 16.0f, C2D_Color32(230, 230, 90, 255));
        }
    }

    // Barriers
    C2D_DrawRectSolid(0, 0, 0.15f, 32, TOP_H, C2D_Color32(90, 35, 35, 255));
    C2D_DrawRectSolid(TOP_W - 32, 0, 0.15f, 32, TOP_H, C2D_Color32(90, 35, 35, 255));

    // Obstacles
    u32 carCols[4] = {
        C2D_Color32(65, 130, 200, 255),
        C2D_Color32(40, 170, 130, 255),
        C2D_Color32(210, 170, 45, 255),
        C2D_Color32(160, 70, 210, 255)
    };
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) continue;
        float ox = obstacles[i].x;
        float oy = obstacles[i].y;
        C2D_DrawRectSolid(ox - 18, oy - 26, 0.4f, 36, 52, carCols[obstacles[i].type % 4]);
        C2D_DrawRectSolid(ox - 12, oy - 12, 0.45f, 24, 14, C2D_Color32(140, 200, 255, 255));
    }

    // Smoke
    for (int i = 0; i < MAX_SMOKE; i++) {
        if (!smokes[i].active) continue;
        u8 a = (u8)(smokes[i].life * 170.0f);
        C2D_DrawRectSolid(smokes[i].x - 7, smokes[i].y - 7, 0.3f, 14, 14, C2D_Color32(190, 190, 190, a));
    }

    // Player
    float px = player.x;
    float py = player.y;
    C2D_DrawRectSolid(px - 18, py - 26, 0.5f, 36, 52, C2D_Color32(230, 55, 55, 255));
    C2D_DrawRectSolid(px - 12, py - 12, 0.55f, 24, 14, C2D_Color32(130, 200, 255, 255));

    if (player.drifting) {
        C2D_DrawRectSolid(px - 28, py + 20, 0.35f, 12, 7, C2D_Color32(200, 200, 200, 150));
        C2D_DrawRectSolid(px + 16, py + 20, 0.35f, 12, 7, C2D_Color32(200, 200, 200, 150));
    }
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

    topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    consoleInit(GFX_BOTTOM, NULL);

    romfsInit();
    initSound();

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
            printf("  Loading...\n");
            bootFrames++;
            if (bootFrames > 120) state = STATE_MENU;
            break;

        case STATE_MENU:
            consoleClear();
            printHeader();
            printf("  Money : $%d\n", money);
            printf("  Level : %d\n\n", level);
            printf("  A     Start Delivery\n");
            printf("  X     Garage / Upgrades\n");
            printf("  START Exit\n\n");
            printf("  Controls:\n");
            printf("  Circle Pad  = Steer\n");
            printf("  Y or B      = Drift\n");
            printf("  START       = Abort\n");
            if (kDown & KEY_A) {
                playSfx(4);
                resetLevel();
                state = STATE_DRIVING;
            }
            if (kDown & KEY_X) {
                playSfx(4);
                state = STATE_GARAGE;
            }
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
            printf("\n  START  Back\n");
            if (kDown & KEY_A) {
                int c = 140 + upgSpeed * 90;
                if (money >= c) { money -= c; upgSpeed++; playSfx(3); }
            }
            if (kDown & KEY_X) {
                int c = 110 + upgHandling * 70;
                if (money >= c) { money -= c; upgHandling++; playSfx(3); }
            }
            if (kDown & KEY_Y) {
                int c = 180 + upgReward * 130;
                if (money >= c) { money -= c; upgReward++; playSfx(3); }
            }
            if (kDown & KEY_START) {
                playSfx(4);
                state = STATE_MENU;
            }
            break;

        case STATE_DRIVING: {
            updateDriving(dt);

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            drawTop();
            C3D_FrameEnd(0);

            consoleClear();
            printf("\n  TEMU DELIVERY  Lv%d\n", level);
            printf("  Dist %.0f / %.0f\n", distance, targetDist);
            printf("  Time  %.1fs\n", timeLeft);
            printf("  Crash %d   $%d\n", crashes, money);
            printf("\n  Pad=Steer  Y/B=Drift\n");
            if (player.drifting) printf("  >>> DRIFTING <<<\n");
            break;
        }

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
                if (kDown & KEY_A) {
                    level++;
                    playSfx(4);
                    state = STATE_MENU;
                }
            } else {
                printf("  Failed.\n");
                printf("  A = Menu\n");
                if (kDown & KEY_A) {
                    playSfx(4);
                    state = STATE_MENU;
                }
            }
            break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    exitSound();
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
