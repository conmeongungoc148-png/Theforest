#include "raylib.h"
#include "player.h"
#include "boss.h"
#include "projectile.h"
#include "orb.h"
#include "collision.h"
#include "gamestate.h"
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define GROUND_Y 620.0f
#define BOSS_X (SCREEN_WIDTH / 2)
#define BOSS_Y 280.0f

static float cameraShake = 0.0f;

static void DrawForestArena(cute_tiled_map_t *map, Texture2D fallbackTiles);

// === Background Music System ===
typedef enum {
    BG_NONE = -1,
    BG_PHRASE12 = 0,
    BG_PHRASE3 = 1,
    BG_PHRASE4 = 2,
    BG_ENDING = 3
} BgTrack;

static Music bgTracks[4];
static BgTrack currentBgTrack = BG_NONE;
static BgTrack pendingBgTrack = BG_NONE;
static float bgVolume = 0.0f;
static float bgVolumeTarget = 1.0f;
static float bgFadeSpeed = 2.0f;  // Volume change per second
static bool fadingOut = false;
static bool endingTriggered = false;
static BossPhase lastPhase = BOSS_PHASE_1;

static BgTrack GetTrackForPhase(BossPhase phase) {
    switch (phase) {
        case BOSS_PHASE_1:
        case BOSS_PHASE_2: return BG_PHRASE12;
        case BOSS_PHASE_3: return BG_PHRASE3;
        case BOSS_PHASE_4: return BG_PHRASE4;
    }
    return BG_PHRASE12;
}

static void SwitchBgTrack(BgTrack newTrack) {
    if (newTrack == currentBgTrack) return;
    pendingBgTrack = newTrack;
    fadingOut = true;
    bgVolumeTarget = 0.0f;
}

static void ResetGame(BossPlayer *player, Boss *boss, ProjectileManager *pm, OrbManager *om) {
    InitBossPlayer(player, (Vector2){200, GROUND_Y}, GROUND_Y);
    InitBoss(boss, (Vector2){BOSS_X, 900.0f}, (Vector2){BOSS_X, BOSS_Y});
    InitProjectileManager(pm);
    InitOrbManager(om);
    cameraShake = 0.0f;
}

static void DrawFallbackArena(void) {
    // Background gradient
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 
        (Color){30, 10, 50, 255},
        (Color){10, 5, 20, 255});

    DrawRectangle(0, (int)GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)GROUND_Y, 
        (Color){60, 40, 80, 255});
    DrawRectangle(0, (int)(GROUND_Y - 4), SCREEN_WIDTH, 4, (Color){150, 80, 180, 255});

    DrawRectangle(0, 0, 20, SCREEN_HEIGHT, (Color){40, 20, 60, 255});
    DrawRectangle(SCREEN_WIDTH - 20, 0, 20, SCREEN_HEIGHT, (Color){40, 20, 60, 255});

    for (int x = 0; x < SCREEN_WIDTH; x += 80) {
        DrawLine(x, (int)GROUND_Y, x, SCREEN_HEIGHT, (Color){80, 50, 100, 100});
    }
}

int main(void) {
    srand((unsigned int)time(NULL));

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Boss Fight - AGIS");
    InitAudioDevice();
    SetTargetFPS(60);

    // Load intro music (chính xác 15.5s)
    Music introMusic = LoadMusicStream("assets/start.ogg");
    introMusic.looping = false;
    bool introMusicStarted = false;

    // Load background music tracks
    bgTracks[BG_PHRASE12] = LoadMusicStream("assets/backgroundsound/phrase1and2.ogg");
    bgTracks[BG_PHRASE3] = LoadMusicStream("assets/backgroundsound/phrase3.ogg");
    bgTracks[BG_PHRASE4] = LoadMusicStream("assets/backgroundsound/phrase4.ogg");
    bgTracks[BG_ENDING] = LoadMusicStream("assets/backgroundsound/ending.ogg");
    
    // Set looping (ending không loop)
    bgTracks[BG_PHRASE12].looping = true;
    bgTracks[BG_PHRASE3].looping = true;
    bgTracks[BG_PHRASE4].looping = true;
    bgTracks[BG_ENDING].looping = false;

    // Load laugh SFX (chạy khi hiện chữ AGIS / ROAR)
    Sound laughSfx = LoadSound("assets/laugh.ogg");
    bool laughPlayed = false;

    // Load damage SFX (khi boss bị sát thương)
    Sound damageSfx = LoadSound("assets/damage.ogg");

    // Load thêm SFX
    Sound alarmSfx = LoadSound("assets/alarm.ogg");   // Warning (claw/hazard/laser)
    Sound hitsSfx = LoadSound("assets/hits.ogg");      // Player bắt orb
    Sound slashSfx = LoadSound("assets/slash.ogg");    // Claw slash

    Texture2D texAgis    = LoadTexture("assets/sprites/agis.png");
    Texture2D texCatIdle = LoadTexture("assets/sprites/cat/IDLE.png");
    Texture2D texCatWalk = LoadTexture("assets/sprites/cat/WALK.png");
    Texture2D texCatRun  = LoadTexture("assets/sprites/cat/RUN.png");
    Texture2D texCatJump = LoadTexture("assets/sprites/cat/JUMP.png");
    Texture2D texCatAttack = LoadTexture("assets/sprites/cat/ATTACK 1.png");
    Texture2D texCatHurt = LoadTexture("assets/sprites/cat/HURT.png");
    Texture2D texForestTiles = LoadTexture("LAMO/Final/Tiles.png");
    cute_tiled_map_t *forestMap = MapLoad("assets/forest/back1.tmj");

    BossPlayer player;
    Boss boss;
    ProjectileManager pm;
    OrbManager om;

    ResetGame(&player, &boss, &pm, &om);

    // === Camera2D với zoom ===
    Camera2D camera = { 0 };
    camera.target = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
    camera.offset = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    GameState gameState = STATE_PLAYING;

    // Track previous warning states để detect transition (chỉ play alarm 1 lần/event)
    bool prevClawActive = false;
    bool prevLaserActive = false;
    int prevHazardCount = 0;
    bool clawSlashPlayed = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // === UPDATE MUSIC STREAMS ===
        UpdateMusicStream(introMusic);
        if (currentBgTrack != BG_NONE) {
            UpdateMusicStream(bgTracks[currentBgTrack]);
        }

        // Bắt đầu phát intro music
        if (boss.state == BOSS_INTRO && !introMusicStarted) {
            PlayMusicStream(introMusic);
            introMusicStarted = true;
        }

        // Phát tiếng cười khi vào ROAR
        if (boss.state == BOSS_ROAR && !laughPlayed) {
            PlaySound(laughSfx);
            laughPlayed = true;
        }

        // === BACKGROUND MUSIC SYSTEM ===
        
        // Bắt đầu bg music khi vào FIGHTING lần đầu
        if (boss.state == BOSS_FIGHTING && currentBgTrack == BG_NONE && !fadingOut) {
            BgTrack startTrack = GetTrackForPhase(boss.phase);
            currentBgTrack = startTrack;
            PlayMusicStream(bgTracks[currentBgTrack]);
            bgVolume = 0.0f;
            bgVolumeTarget = 1.0f;
            SetMusicVolume(bgTracks[currentBgTrack], bgVolume);
            lastPhase = boss.phase;
        }

        // Detect phase change → switch music với fade
        if (boss.state == BOSS_FIGHTING && currentBgTrack != BG_NONE) {
            if (boss.phase != lastPhase) {
                BgTrack desiredTrack = GetTrackForPhase(boss.phase);
                if (desiredTrack != currentBgTrack && !fadingOut) {
                    SwitchBgTrack(desiredTrack);
                }
                lastPhase = boss.phase;
            }
        }

        // Detect DYING → switch to ending music
        if (boss.state == BOSS_DYING && !endingTriggered) {
            if (currentBgTrack != BG_ENDING && !fadingOut) {
                SwitchBgTrack(BG_ENDING);
            }
            endingTriggered = true;
        }

        // Volume fade logic
        float volumeStep = bgFadeSpeed * dt;
        if (bgVolume < bgVolumeTarget) {
            bgVolume += volumeStep;
            if (bgVolume > bgVolumeTarget) bgVolume = bgVolumeTarget;
        } else if (bgVolume > bgVolumeTarget) {
            bgVolume -= volumeStep;
            if (bgVolume < bgVolumeTarget) bgVolume = bgVolumeTarget;
        }

        if (currentBgTrack != BG_NONE) {
            SetMusicVolume(bgTracks[currentBgTrack], bgVolume);
        }

        // Handle fade-out complete: switch to pending track
        if (fadingOut && bgVolume <= 0.001f) {
            if (currentBgTrack != BG_NONE) {
                StopMusicStream(bgTracks[currentBgTrack]);
            }
            currentBgTrack = pendingBgTrack;
            pendingBgTrack = BG_NONE;
            fadingOut = false;
            if (currentBgTrack != BG_NONE) {
                SetMusicVolume(bgTracks[currentBgTrack], 0.0f);
                PlayMusicStream(bgTracks[currentBgTrack]);
                bgVolumeTarget = 1.0f;
            }
        }

        // Ending music finished → trigger boss defeated
        if (currentBgTrack == BG_ENDING && !fadingOut && bgVolume > 0.5f) {
            float played = GetMusicTimePlayed(bgTracks[BG_ENDING]);
            float total = GetMusicTimeLength(bgTracks[BG_ENDING]);
            if (total > 0 && played >= total - 0.2f) {
                boss.defeated = true;
                boss.state = BOSS_DEFEATED;
            }
        }

        // === UPDATE ===
        if (gameState == STATE_PLAYING) {
            UpdateBoss(&boss, player.position, &pm, &om, dt, &cameraShake);

            // === Camera Zoom & Target ===
            if (boss.state == BOSS_INTRO) {
                float progress = boss.introTimer / 15.5f;
                if (progress > 1.0f) progress = 1.0f;
                
                float targetZoom = 1.0f + 0.8f * progress;
                camera.zoom += (targetZoom - camera.zoom) * 3.0f * dt;
                
                Vector2 targetPos = boss.position;
                camera.target.x += (targetPos.x - camera.target.x) * 3.0f * dt;
                camera.target.y += (targetPos.y - camera.target.y) * 3.0f * dt;
            }
            else if (boss.state == BOSS_ROAR) {
                camera.zoom += (1.6f - camera.zoom) * 3.0f * dt;
                camera.target.x += (boss.position.x - camera.target.x) * 3.0f * dt;
                camera.target.y += (boss.position.y - camera.target.y) * 3.0f * dt;
            }
            else {
                camera.zoom += (1.0f - camera.zoom) * 2.0f * dt;
                Vector2 normalTarget = { SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
                camera.target.x += (normalTarget.x - camera.target.x) * 2.0f * dt;
                camera.target.y += (normalTarget.y - camera.target.y) * 2.0f * dt;
            }

            camera.offset.x = SCREEN_WIDTH/2.0f + ((float)(rand() % 21 - 10) / 10.0f) * cameraShake;
            camera.offset.y = SCREEN_HEIGHT/2.0f + ((float)(rand() % 21 - 10) / 10.0f) * cameraShake;

            // Check win/lose
            if (boss.defeated && boss.deathTimer >= 4.0f) gameState = STATE_WIN;
            if (player.hp <= 0) gameState = STATE_LOSE;

            if (boss.state == BOSS_FIGHTING) {
                UpdateBossPlayer(&player, dt);
                UpdateProjectiles(&pm, dt);
                UpdateOrbs(&om, player.position, boss.position, GROUND_Y, dt);

                // === ALARM SOUND khi xuất hiện warning ===
                if (boss.clawActive && !prevClawActive) {
                    PlaySound(alarmSfx);
                }
                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0 && !clawSlashPlayed) {
                    StopSound(alarmSfx);
                    PlaySound(slashSfx);
                    clawSlashPlayed = true;
                }
                if (!boss.clawActive) clawSlashPlayed = false;
                prevClawActive = boss.clawActive;

                if (boss.laserActive && !prevLaserActive) {
                    PlaySound(alarmSfx);
                }
                if (prevLaserActive && boss.laserActive && boss.laserChargeTime <= 0) {
                    StopSound(alarmSfx);
                }
                prevLaserActive = boss.laserActive;

                if (boss.hazardCount > 0 && prevHazardCount == 0) {
                    PlaySound(alarmSfx);
                }
                prevHazardCount = boss.hazardCount;
                bool anyHazardActive = false;
                for (int i = 0; i < boss.hazardCount; i++) {
                    if (boss.hazardActive[i] || boss.hazardWarningTime[i] > 0) {
                        anyHazardActive = true;
                        break;
                    }
                }
                if (!anyHazardActive && prevHazardCount > 0) {
                    StopSound(alarmSfx);
                    prevHazardCount = 0;
                }

                // Bắt orb → hits.ogg
                {
                    bool wasReady[MAX_ORBS];
                    for (int i = 0; i < MAX_ORBS; i++) {
                        wasReady[i] = (om.orbs[i].state == ORB_READY);
                    }
                    TryCatchOrb(&om, player.hurtBox, boss.position);
                    for (int i = 0; i < MAX_ORBS; i++) {
                        if (wasReady[i] && om.orbs[i].state == ORB_RETURNING) {
                            PlaySound(hitsSfx);
                        }
                    }
                }

                for (int i = 0; i < MAX_PROJECTILES; i++) {
                    if (!pm.projectiles[i].active) continue;
                    if (CheckCollision(pm.projectiles[i].hitbox, player.hurtBox)) {
                        PlayerTakeDamage(&player, pm.projectiles[i].damage);
                        pm.projectiles[i].active = false;
                        pm.count--;
                    }
                }

                for (int i = 0; i < MAX_ORBS; i++) {
                    if (om.orbs[i].state != ORB_RETURNING) continue;
                    if (CheckCollision(om.orbs[i].hitbox, boss.hurtBox)) {
                        BossTakeDamage(&boss, om.orbs[i].damage);
                        PlaySound(damageSfx);
                        om.orbs[i].state = ORB_INACTIVE;
                        boss.orbActive = false;
                    }
                }

                if (boss.laserActive && boss.laserChargeTime <= 0) {
                    if (CheckPlayerInLaser(player.position, boss.laserStart, boss.laserEnd, 20.0f)) {
                        PlayerTakeDamage(&player, 1);
                    }
                }

                if (boss.slamActive && boss.shockwaveRadius > 50.0f) {
                    if (CheckPlayerInShockwave(player.position, boss.slamPos, boss.shockwaveRadius)) {
                        PlayerTakeDamage(&player, 2);
                    }
                }

                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0) {
                    if (CheckPlayerInClawZone(player.position, boss.clawZone)) {
                        PlayerTakeDamage(&player, 2);
                    }
                }

                for (int i = 0; i < boss.hazardCount; i++) {
                    if (!boss.hazardActive[i]) continue;
                    Rectangle hazardRect = {
                        boss.hazardPositions[i].x - 15,
                        boss.hazardPositions[i].y - 80,
                        30, 80
                    };
                    if (CheckCollision(hazardRect, player.hurtBox)) {
                        PlayerTakeDamage(&player, 1);
                    }
                }

                if (player.position.x < 30) player.position.x = 30;
                if (player.position.x > SCREEN_WIDTH - 30) player.position.x = SCREEN_WIDTH - 30;
            }

        } else {
            if (IsKeyPressed(KEY_R)) {
                ResetGame(&player, &boss, &pm, &om);
                gameState = STATE_PLAYING;
                camera.zoom = 1.0f;
                camera.target = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
                
                // Reset music system
                StopMusicStream(introMusic);
                if (currentBgTrack != BG_NONE) {
                    StopMusicStream(bgTracks[currentBgTrack]);
                }
                currentBgTrack = BG_NONE;
                pendingBgTrack = BG_NONE;
                bgVolume = 0.0f;
                bgVolumeTarget = 1.0f;
                fadingOut = false;
                endingTriggered = false;
                lastPhase = BOSS_PHASE_1;
                
                introMusicStarted = false;
                laughPlayed = false;
                prevClawActive = false;
                prevLaserActive = false;
                prevHazardCount = 0;
                clawSlashPlayed = false;
            }
        }

        // === DRAW ===
        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode2D(camera);
        DrawForestArena(forestMap, texForestTiles);
        DrawBoss(&boss, texAgis, 0);
        DrawOrbs(&om);
        DrawProjectiles(&pm);
        DrawBossPlayer(&player, texCatIdle, texCatWalk, texCatRun, texCatJump, texCatAttack, texCatHurt);
        EndMode2D();

        // === UI ===
        if (boss.state == BOSS_FIGHTING || boss.state == BOSS_DYING || boss.state == BOSS_DEFEATED) {
            DrawUI(player.hp, boss.hp, boss.maxHp);

            const char *phaseText = "Phase 1";
            if (boss.phase == BOSS_PHASE_2) phaseText = "Phase 2 - Enraged";
            if (boss.phase == BOSS_PHASE_3) phaseText = "Phase 3 - Danger!";
            if (boss.phase == BOSS_PHASE_4) phaseText = "Phase 4 - FINAL!";
            DrawText(phaseText, SCREEN_WIDTH / 2 - 80, 65, 18, YELLOW);
        }

        if (boss.state == BOSS_INTRO) {
            float alpha = boss.introTimer / 3.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            DrawText("Something approaches...", SCREEN_WIDTH/2 - 150, 80, 24, 
                (Color){255, 100, 100, (unsigned char)(alpha * 200)});
            
            DrawRectangle(0, 0, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
            DrawRectangle(0, SCREEN_HEIGHT - 60, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
            
            float pct = boss.introTimer / 15.5f;
            if (pct > 1.0f) pct = 1.0f;
            DrawRectangle(20, SCREEN_HEIGHT - 40, (int)(200 * pct), 4, (Color){255, 100, 100, 150});
        }

        if (boss.state == BOSS_ROAR) {
            DrawText("A G I S", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 50, 48, 
                (Color){255, 50, 50, 255});
            DrawRectangle(0, 0, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
            DrawRectangle(0, SCREEN_HEIGHT - 60, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
        }

        if (boss.state == BOSS_FIGHTING) {
            DrawText("A/D: Move  |  Space: Jump  |  Catch yellow orbs to damage boss!", 
                20, SCREEN_HEIGHT - 30, 16, (Color){200, 200, 200, 200});
        }

        if (boss.state == BOSS_DYING) {
            DrawText("FINAL BLOW!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 36, 
                (Color){255, 255, 100, 255});
        }

        if (gameState == STATE_WIN) DrawWinScreen();
        if (gameState == STATE_LOSE) DrawLoseScreen();

        DrawFPS(SCREEN_WIDTH - 90, 10);
        EndDrawing();
    }

    UnloadTexture(texAgis);
    UnloadTexture(texCatIdle);
    UnloadTexture(texCatWalk);
    UnloadTexture(texCatRun);
    UnloadTexture(texCatJump);
    UnloadTexture(texCatAttack);
    UnloadTexture(texCatHurt);
    UnloadTexture(texForestTiles);
    if (forestMap) MapUnload(forestMap);

    UnloadMusicStream(introMusic);
    for (int i = 0; i < 4; i++) {
        UnloadMusicStream(bgTracks[i]);
    }
    UnloadSound(laughSfx);
    UnloadSound(damageSfx);
    UnloadSound(alarmSfx);
    UnloadSound(hitsSfx);
    UnloadSound(slashSfx);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

static void DrawForestArena(cute_tiled_map_t *map, Texture2D fallbackTiles) {
    if (!map) {
        DrawFallbackArena();
        return;
    }

    float offsetY = GROUND_Y - MapGetGroundY(map);
    cute_tiled_layer_t *layer = map->layers;
    while (layer) {
        if (layer->visible) {
            MapDrawLayerEx(map, layer->name.ptr, 0.0f, offsetY, fallbackTiles);
        }
        layer = layer->next;
    }

    DrawRectangle(0, (int)GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)GROUND_Y,
        (Color){8, 10, 16, 70});
}
