#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240
#define BOTTOM_WIDTH  320
#define BOTTOM_HEIGHT 240

#define VERSION "1.0.0"
#define GITHUB_OWNER "SlabyLol"
#define GITHUB_REPO "TemuDriver3DS"

// Game states
enum GameState {
    STATE_UPDATE_CHECK,
    STATE_MENU,
    STATE_GARAGE,
    STATE_DRIVING,
    STATE_RESULTS,
    STATE_EXIT
};

// Simple car structure
struct Car {
    float x;
    float y;
    float speed;
    float angle; // for drift visual
    u32 color;
    bool drifting;
};

// Obstacle
struct Obstacle {
    float x;
    float y;
    float speed;
    u32 color;
    bool active;
};

#define MAX_OBSTACLES 12
#define NUM_LANES 4
#define LANE_WIDTH 80.0f

GameState state = STATE_UPDATE_CHECK;
Car player;
Obstacle obstacles[MAX_OBSTACLES];
int score = 0;
int money = 0;
int level = 1;
int crashes = 0;
float distance = 0.0f;
float targetDistance = 2000.0f;
float timeElapsed = 0.0f;
float maxTime = 60.0f;
bool updateAvailable = false;
char updateMsg[128] = "Checking for updates...";

// Upgrades
int upgradeSpeed = 0;
int upgradeHandling = 0;
int upgradeReward = 0;
u32 carColor = C2D_Color32(255, 50, 50, 255); // red default

// Save data simple
void loadSave() {
    // In real: read from SD. For now defaults.
    money = 500;
    upgradeSpeed = 0;
    upgradeHandling = 0;
    upgradeReward = 0;
}

void saveSave() {
    // TODO: write to SD
}

void initPlayer() {
    player.x = SCREEN_WIDTH / 2.0f;
    player.y = SCREEN_HEIGHT - 60.0f;
    player.speed = 2.0f + upgradeSpeed * 0.4f;
    player.angle = 0.0f;
    player.color = carColor;
    player.drifting = false;
}

void initObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        obstacles[i].active = false;
    }
}

void spawnObstacle() {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            int lane = rand() % NUM_LANES;
            obstacles[i].x = 40.0f + lane * LANE_WIDTH + LANE_WIDTH / 2.0f;
            obstacles[i].y = -40.0f;
            obstacles[i].speed = 1.5f + (rand() % 100) / 50.0f + level * 0.2f;
            obstacles[i].color = C2D_Color32(50 + rand()%150, 50 + rand()%150, 50 + rand()%150, 255);
            break;
        }
    }
}

bool checkCollision(Car* p, Obstacle* o) {
    float dx = p->x - o->x;
    float dy = p->y - o->y;
    float dist = sqrtf(dx*dx + dy*dy);
    return dist < 35.0f;
}

void resetDriving() {
    initPlayer();
    initObstacles();
    score = 0;
    crashes = 0;
    distance = 0.0f;
    timeElapsed = 0.0f;
    targetDistance = 1500.0f + level * 500.0f;
    maxTime = 45.0f + level * 10.0f;
}

// Simple update check using httpc (simplified - just shows message)
void doUpdateCheck() {
    // In full version: httpcInit, request to api.github.com/repos/.../releases/latest
    // Parse tag_name and compare to VERSION
    // For this prototype we simulate
    static int frames = 0;
    frames++;
    if (frames < 90) {
        snprintf(updateMsg, sizeof(updateMsg), "Checking for updates...");
    } else if (frames < 150) {
        snprintf(updateMsg, sizeof(updateMsg), "You are on latest version %s", VERSION);
        updateAvailable = false;
    } else {
        state = STATE_MENU;
    }
}

void updateDriving(float dt) {
    hidScanInput();
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    circlePosition pos;
    hidCircleRead(&pos);

    // Steering with Circle Pad
    float steer = pos.dx / 156.0f; // -1 to 1 approx
    if (steer > 1.0f) steer = 1.0f;
    if (steer < -1.0f) steer = -1.0f;

    bool wantDrift = (kHeld & KEY_Y) || (kHeld & KEY_B);
    player.drifting = wantDrift && fabsf(steer) > 0.3f;

    float handling = 2.5f + upgradeHandling * 0.5f;
    if (player.drifting) {
        // Drift: faster lateral but more risk
        player.x += steer * handling * 2.2f * dt * 60.0f;
        player.angle = steer * 25.0f;
    } else {
        player.x += steer * handling * dt * 60.0f;
        player.angle *= 0.85f;
    }

    // Clamp to road
    if (player.x < 30.0f) player.x = 30.0f;
    if (player.x > SCREEN_WIDTH - 30.0f) player.x = SCREEN_WIDTH - 30.0f;

    // Move forward (scroll world)
    float forward = player.speed * (player.drifting ? 1.15f : 1.0f);
    distance += forward * dt * 60.0f;
    timeElapsed += dt;

    // Spawn obstacles
    static float spawnTimer = 0.0f;
    spawnTimer += dt;
    float spawnRate = 1.2f - level * 0.05f;
    if (spawnRate < 0.4f) spawnRate = 0.4f;
    if (spawnTimer > spawnRate) {
        spawnObstacle();
        spawnTimer = 0.0f;
    }

    // Update obstacles
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) {
            obstacles[i].y += (obstacles[i].speed + forward * 0.3f) * dt * 60.0f;
            if (obstacles[i].y > SCREEN_HEIGHT + 50.0f) {
                obstacles[i].active = false;
                score += 10;
            }
            if (checkCollision(&player, &obstacles[i])) {
                obstacles[i].active = false;
                crashes++;
                // small knockback
                player.x += (player.x > obstacles[i].x ? 15.0f : -15.0f);
            }
        }
    }

    // Win / lose
    if (distance >= targetDistance) {
        // Calculate reward
        float timeBonus = (maxTime - timeElapsed);
        if (timeBonus < 0) timeBonus = 0;
        int base = 100 + level * 50;
        int crashPenalty = crashes * 40;
        int reward = (int)(base + timeBonus * 5.0f * (1.0f + upgradeReward * 0.2f) - crashPenalty);
        if (reward < 10) reward = 10;
        money += reward;
        score = reward;
        state = STATE_RESULTS;
    }
    if (timeElapsed > maxTime || crashes >= 8) {
        // Fail
        int reward = 20 - crashes * 5;
        if (reward < 0) reward = 0;
        money += reward;
        score = reward;
        state = STATE_RESULTS;
    }

    if (kDown & KEY_START) {
        state = STATE_MENU;
    }
}

void drawRect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x - w/2, y - h/2, 0.5f, w, h, color);
}

void renderDriving() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT), C2D_Color32(40, 40, 50, 255));
    C2D_SceneBegin(C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT));

    // Road
    C2D_DrawRectSolid(20, 0, 0.4f, SCREEN_WIDTH - 40, SCREEN_HEIGHT, C2D_Color32(60, 60, 70, 255));
    // Lane lines
    for (int i = 1; i < NUM_LANES; i++) {
        float lx = 20 + i * LANE_WIDTH;
        for (int y = 0; y < SCREEN_HEIGHT; y += 30) {
            C2D_DrawRectSolid(lx - 2, (int)(y + fmodf(distance, 30.0f)), 0.45f, 4, 15, C2D_Color32(200, 200, 100, 255));
        }
    }

    // Obstacles
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) {
            drawRect(obstacles[i].x, obstacles[i].y, 40, 55, obstacles[i].color);
        }
    }

    // Player car (simple rotated look with angle)
    float px = player.x;
    float py = player.y;
    // body
    C2D_DrawRectSolid(px - 18, py - 25, 0.6f, 36, 50, player.color);
    // windows
    C2D_DrawRectSolid(px - 12, py - 10, 0.65f, 24, 18, C2D_Color32(100, 180, 255, 255));
    if (player.drifting) {
        // drift smoke
        C2D_DrawRectSolid(px - 25, py + 20, 0.55f, 12, 8, C2D_Color32(180, 180, 180, 180));
        C2D_DrawRectSolid(px + 13, py + 20, 0.55f, 12, 8, C2D_Color32(180, 180, 180, 180));
    }

    // HUD top
    char buf[64];
    snprintf(buf, sizeof(buf), "Dist: %.0f / %.0f", distance, targetDistance);
    // We use console on bottom mostly

    C3D_FrameEnd(0);

    // Bottom screen UI
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT), C2D_Color32(20, 20, 30, 255));
    C2D_SceneBegin(C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT));

    // Simple text via printf to console would be better, but for pure citro2d we skip detailed text
    // For prototype we rely on console on bottom

    C3D_FrameEnd(0);
}

// Console based UI for simplicity (bottom screen)
PrintConsole topConsole, bottomConsole;

void initConsoles() {
    consoleInit(GFX_TOP, &topConsole);
    consoleInit(GFX_BOTTOM, &bottomConsole);
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // For text we use console
    consoleInit(GFX_BOTTOM, NULL);
    // Top for game when driving, but for menus use console too for simplicity

    loadSave();
    initPlayer();

    // Simple state machine with console UI for menus (easier without font rendering)
    bool running = true;
    u64 lastTime = osGetTime();

    while (aptMainLoop() && running) {
        u64 now = osGetTime();
        float dt = (now - lastTime) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastTime = now;

        hidScanInput();
        u32 kDown = hidKeysDown();

        switch (state) {
            case STATE_UPDATE_CHECK:
                consoleClear();
                printf("\n\n\n");
                printf("   ==========================\n");
                printf("      TEMU DRIVER 3DS\n");
                printf("   ==========================\n\n");
                printf("   %s\n\n", updateMsg);
                printf("   Version: %s\n", VERSION);
                doUpdateCheck();
                break;

            case STATE_MENU:
                consoleClear();
                printf("\n\n");
                printf("   ==========================\n");
                printf("      TEMU DRIVER 3DS\n");
                printf("   ==========================\n\n");
                printf("   Money: $%d\n", money);
                printf("   Level: %d\n\n", level);
                printf("   A : Start Delivery\n");
                printf("   X : Garage / Upgrades\n");
                printf("   START : Exit\n\n");
                printf("   Controls in game:\n");
                printf("   Circle Pad : Steer Left/Right\n");
                printf("   Y or B    : Drift\n");
                printf("   START     : Abort to menu\n");
                if (kDown & KEY_A) {
                    resetDriving();
                    state = STATE_DRIVING;
                    // Switch to graphics mode-ish
                }
                if (kDown & KEY_X) {
                    state = STATE_GARAGE;
                }
                if (kDown & KEY_START) {
                    running = false;
                }
                break;

            case STATE_GARAGE:
                consoleClear();
                printf("\n\n");
                printf("   ===== GARAGE / UPGRADES =====\n\n");
                printf("   Money: $%d\n\n", money);
                printf("   1. Speed Lv%d   (A) $%d\n", upgradeSpeed, 150 + upgradeSpeed * 100);
                printf("   2. Handling Lv%d (X) $%d\n", upgradeHandling, 120 + upgradeHandling * 80);
                printf("   3. Reward  Lv%d (Y) $%d\n", upgradeReward, 200 + upgradeReward * 150);
                printf("\n   B : Change Car Color\n");
                printf("   START : Back to Menu\n");
                if (kDown & KEY_A) {
                    int cost = 150 + upgradeSpeed * 100;
                    if (money >= cost) {
                        money -= cost;
                        upgradeSpeed++;
                        player.speed = 2.0f + upgradeSpeed * 0.4f;
                    }
                }
                if (kDown & KEY_X) {
                    int cost = 120 + upgradeHandling * 80;
                    if (money >= cost) {
                        money -= cost;
                        upgradeHandling++;
                    }
                }
                if (kDown & KEY_Y) {
                    int cost = 200 + upgradeReward * 150;
                    if (money >= cost) {
                        money -= cost;
                        upgradeReward++;
                    }
                }
                if (kDown & KEY_B) {
                    // Cycle colors
                    static int colIdx = 0;
                    u32 colors[] = {
                        C2D_Color32(255, 50, 50, 255),
                        C2D_Color32(50, 100, 255, 255),
                        C2D_Color32(50, 200, 50, 255),
                        C2D_Color32(255, 200, 50, 255),
                        C2D_Color32(200, 50, 200, 255),
                        C2D_Color32(255, 255, 255, 255)
                    };
                    colIdx = (colIdx + 1) % 6;
                    carColor = colors[colIdx];
                    player.color = carColor;
                }
                if (kDown & KEY_START) {
                    state = STATE_MENU;
                    saveSave();
                }
                break;

            case STATE_DRIVING: {
                // For prototype we use mixed: clear console and draw simple
                // To keep it simple and reliable, we use console representation
                consoleClear();
                printf("TEMU DELIVERY - Level %d\n", level);
                printf("Dist: %.0f/%.0f  Time: %.1f/%.0f\n", distance, targetDistance, timeElapsed, maxTime);
                printf("Crashes: %d   Money: $%d\n", crashes, money);
                printf("--------------------------------\n");
                // Simple ASCII road
                char road[21];
                for (int row = 0; row < 12; row++) {
                    memset(road, ' ', 20);
                    road[20] = 0;
                    // player
                    if (row == 10) {
                        int px = (int)((player.x / SCREEN_WIDTH) * 18) + 1;
                        if (px < 1) px = 1;
                        if (px > 18) px = 18;
                        road[px] = player.drifting ? 'D' : 'P';
                    }
                    // obstacles
                    for (int i = 0; i < MAX_OBSTACLES; i++) {
                        if (obstacles[i].active) {
                            int oy = (int)((obstacles[i].y / SCREEN_HEIGHT) * 12);
                            if (oy == row) {
                                int ox = (int)((obstacles[i].x / SCREEN_WIDTH) * 18) + 1;
                                if (ox >= 1 && ox <= 18) road[ox] = 'C';
                            }
                        }
                    }
                    printf("|%s|\n", road);
                }
                printf("--------------------------------\n");
                printf("Pad: Steer | Y/B: Drift | START: Quit\n");
                if (player.drifting) printf("*** DRIFTING ***\n");

                updateDriving(dt);
                break;
            }

            case STATE_RESULTS:
                consoleClear();
                printf("\n\n");
                printf("   ===== DELIVERY RESULT =====\n\n");
                printf("   Reward: $%d\n", score);
                printf("   Crashes: %d\n", crashes);
                printf("   Time: %.1fs\n\n", timeElapsed);
                printf("   Total Money: $%d\n\n", money);
                if (distance >= targetDistance && crashes < 8) {
                    printf("   SUCCESS! Level up possible.\n");
                    if (kDown & KEY_A) {
                        level++;
                        state = STATE_MENU;
                    }
                } else {
                    printf("   Failed delivery.\n");
                }
                printf("\n   A : Continue\n");
                if (kDown & KEY_A) {
                    state = STATE_MENU;
                    saveSave();
                }
                break;

            default:
                running = false;
                break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
