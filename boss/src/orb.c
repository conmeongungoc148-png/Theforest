#include "orb.h"
#include "collision.h"
#include <math.h>

// Lazy-loaded fire sprite for orb falling effect (shipfire06, 5 frames 58x72)
static Texture2D orbFireTex = {0};
static bool orbFireLoaded = false;
#define ORB_FIRE_FRAMES 5
#define ORB_FIRE_FW 58
#define ORB_FIRE_FH 72

#define ORB_RADIUS 12.0f  // Nhỏ hơn (từ 20 -> 12)
#define ORB_RETURN_SPEED 500.0f
#define ORB_DAMAGE 10
#define GRAVITY 800.0f

void InitOrbManager(OrbManager *om) {
    for (int i = 0; i < MAX_ORBS; i++) {
        om->orbs[i].state = ORB_INACTIVE;
        om->orbs[i].radius = ORB_RADIUS;
        om->orbs[i].damage = ORB_DAMAGE;
        om->orbs[i].pulseTimer = 0.0f;
    }
}

void SpawnOrb(OrbManager *om, Vector2 spawnPos) {
    // Tìm slot trống
    for (int i = 0; i < MAX_ORBS; i++) {
        if (om->orbs[i].state == ORB_INACTIVE) {
            om->orbs[i].position = spawnPos;
            // Velocity ban đầu = 0 (sẽ rơi thẳng xuống với gravity)
            om->orbs[i].velocity = (Vector2){0, 0};
            om->orbs[i].state = ORB_FALLING;
            om->orbs[i].hitbox = (Rectangle){
                spawnPos.x - ORB_RADIUS,
                spawnPos.y - ORB_RADIUS,
                ORB_RADIUS * 2,
                ORB_RADIUS * 2
            };
            return;
        }
    }
}

void SpawnOrbToward(OrbManager *om, Vector2 spawnPos, Vector2 playerPos) {
    // Tìm slot trống
    for (int i = 0; i < MAX_ORBS; i++) {
        if (om->orbs[i].state == ORB_INACTIVE) {
            om->orbs[i].position = spawnPos;
            // Bắn theo hướng player với vận tốc ban đầu (parabolic arc)
            Vector2 dir = {
                playerPos.x - spawnPos.x,
                playerPos.y - spawnPos.y
            };
            float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (dist > 1.0f) {
                dir.x /= dist;
                dir.y /= dist;
            }
            // Velocity hướng về player, sau đó gravity sẽ kéo xuống
            float speed = 300.0f;
            om->orbs[i].velocity = (Vector2){dir.x * speed, dir.y * speed * 0.5f};
            om->orbs[i].state = ORB_FALLING;
            om->orbs[i].hitbox = (Rectangle){
                spawnPos.x - ORB_RADIUS,
                spawnPos.y - ORB_RADIUS,
                ORB_RADIUS * 2,
                ORB_RADIUS * 2
            };
            return;
        }
    }
}

void UpdateOrbs(OrbManager *om, Vector2 playerPos, Vector2 bossPos, float groundY, float dt) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *orb = &om->orbs[i];
        if (orb->state == ORB_INACTIVE) continue;

        orb->pulseTimer += dt;

        switch (orb->state) {
            case ORB_FALLING:
                // Rơi thẳng xuống với gravity (không tracking player)
                orb->velocity.y += GRAVITY * dt;
                orb->position.x += orb->velocity.x * dt;
                orb->position.y += orb->velocity.y * dt;
                
                // Chạm sàn -> chuyển sang READY
                if (orb->position.y >= groundY) {
                    orb->position.y = groundY;
                    orb->velocity = (Vector2){0, 0};
                    orb->state = ORB_READY;
                }
                break;

            case ORB_READY:
                // Nằm yên trên sàn, chờ player bắt
                break;

            case ORB_RETURNING:
                // Bay về boss
                {
                    Vector2 dir = {
                        bossPos.x - orb->position.x,
                        bossPos.y - orb->position.y
                    };
                    dir = NormalizeVec(dir);
                    orb->position.x += dir.x * ORB_RETURN_SPEED * dt;
                    orb->position.y += dir.y * ORB_RETURN_SPEED * dt;

                    // Kiểm tra đã chạm boss chưa
                    float dist = Distance(orb->position, bossPos);
                    if (dist < 50.0f) {
                        orb->state = ORB_INACTIVE; // Đã hit boss, hủy orb
                    }
                }
                break;

            case ORB_INACTIVE:
                break;
        }

        // Cập nhật hitbox
        orb->hitbox.x = orb->position.x - orb->radius;
        orb->hitbox.y = orb->position.y - orb->radius;
    }
}

void TryCatchOrb(OrbManager *om, Rectangle playerHurtBox, Vector2 bossPos) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *orb = &om->orbs[i];
        // Player bắt orb khi nó READY (nằm trên sàn)
        if (orb->state != ORB_READY) continue;

        // Check collision với player
        if (CheckCollision(orb->hitbox, playerHurtBox)) {
            orb->state = ORB_RETURNING;
            orb->velocity = (Vector2){0, 0}; // Reset velocity
        }
    }
}

void DrawOrbs(OrbManager *om) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *orb = &om->orbs[i];
        if (orb->state == ORB_INACTIVE) continue;

        Color color;
        float pulseScale = 1.0f;

        switch (orb->state) {
            case ORB_FALLING:
                // Orb đang rơi - vẽ fire sprite animation phía dưới
                pulseScale = 1.0f + sinf(orb->pulseTimer * 8.0f) * 0.1f;
                color = (Color){255, 220, 100, 255};
                // Fire effect (lazy load)
                if (!orbFireLoaded) {
                    orbFireTex = LoadTexture("assets/shipfire/orb_fire_sheet.png");
                    orbFireLoaded = true;
                }
                if (orbFireTex.id > 0) {
                    int fireFrame = ((int)(orb->pulseTimer * 12.0f)) % ORB_FIRE_FRAMES;
                    Rectangle fireSrc = { (float)(fireFrame * ORB_FIRE_FW), 0, (float)ORB_FIRE_FW, (float)ORB_FIRE_FH };
                    Rectangle fireDst = { orb->position.x - 15, orb->position.y - 10, 30, 40 };
                    DrawTexturePro(orbFireTex, fireSrc, fireDst, (Vector2){0,0}, 0, WHITE);
                }
                break;
            case ORB_READY:
                // Nằm trên sàn - màu vàng sáng, pulse mạnh
                pulseScale = 1.0f + sinf(orb->pulseTimer * 5.0f) * 0.25f;
                color = (Color){255, 255, 0, 255};
                break;
            case ORB_RETURNING:
                // Đang bay về boss - màu cyan
                color = (Color){0, 255, 255, 255};
                pulseScale = 1.0f + sinf(orb->pulseTimer * 10.0f) * 0.15f;
                break;
            default:
                color = WHITE;
        }

        // Vẽ orb nhỏ hơn với glow effect
        DrawCircleV(orb->position, orb->radius * pulseScale + 4, (Color){color.r, color.g, color.b, 60});
        DrawCircleV(orb->position, orb->radius * pulseScale, color);
        DrawCircleV(orb->position, orb->radius * pulseScale * 0.6f, (Color){255, 255, 255, 180});
        
        // Trail effect khi RETURNING
        if (orb->state == ORB_RETURNING) {
            for (int j = 1; j <= 3; j++) {
                float alpha = 1.0f - (float)j * 0.3f;
                DrawCircleV(orb->position, orb->radius * (1.0f - (float)j * 0.2f), 
                    (Color){0, 255, 255, (unsigned char)(alpha * 120)});
            }
        }
    }
}