#ifndef ORB_H
#define ORB_H

#include "raylib.h"
#include <stdbool.h>

#define MAX_ORBS 5

typedef enum {
    ORB_FALLING,        // Boss vừa nhả ra, đang rơi xuống sàn
    ORB_READY,          // Đã chạm sàn, chờ player bắt
    ORB_RETURNING,      // Player đã bắt, đang bay về boss
    ORB_INACTIVE
} OrbState;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Rectangle hitbox;
    OrbState state;
    int damage;
    float radius;
    float pulseTimer;   // Hiệu ứng nhấp nháy khi READY
} Orb;

typedef struct {
    Orb orbs[MAX_ORBS];
} OrbManager;

void InitOrbManager(OrbManager *om);
void SpawnOrb(OrbManager *om, Vector2 spawnPos);
void SpawnOrbToward(OrbManager *om, Vector2 spawnPos, Vector2 playerPos);
void UpdateOrbs(OrbManager *om, Vector2 playerPos, Vector2 bossPos, float groundY, float dt);
void TryCatchOrb(OrbManager *om, Rectangle playerHurtBox, Vector2 bossPos);
void DrawOrbs(OrbManager *om);

#endif // ORB_H