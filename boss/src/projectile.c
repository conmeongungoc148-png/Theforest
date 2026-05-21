#include "projectile.h"
#include <math.h>

#define PROJECTILE_RADIUS 12.0f
#define PROJECTILE_LIFETIME 5.0f

void InitProjectileManager(ProjectileManager *pm) {
    pm->count = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        pm->projectiles[i].active = false;
    }
}

void SpawnProjectile(ProjectileManager *pm, Vector2 pos, Vector2 vel, int damage) {
    // Tìm slot trống
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->projectiles[i].active) {
            pm->projectiles[i].position = pos;
            pm->projectiles[i].velocity = vel;
            pm->projectiles[i].damage = damage;
            pm->projectiles[i].active = true;
            pm->projectiles[i].lifetime = PROJECTILE_LIFETIME;
            pm->projectiles[i].radius = PROJECTILE_RADIUS;
            pm->projectiles[i].hitbox = (Rectangle){
                pos.x - PROJECTILE_RADIUS,
                pos.y - PROJECTILE_RADIUS,
                PROJECTILE_RADIUS * 2,
                PROJECTILE_RADIUS * 2
            };
            pm->count++;
            return;
        }
    }
}

void UpdateProjectiles(ProjectileManager *pm, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->projectiles[i].active) continue;

        Projectile *p = &pm->projectiles[i];

        // Di chuyển
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;

        // Cập nhật hitbox
        p->hitbox.x = p->position.x - p->radius;
        p->hitbox.y = p->position.y - p->radius;

        // Giảm lifetime
        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            p->active = false;
            pm->count--;
        }

        // Hủy nếu ra ngoài màn hình
        if (p->position.x < -100 || p->position.x > 2100 ||
            p->position.y < -100 || p->position.y > 1200) {
            p->active = false;
            pm->count--;
        }
    }
}

void DrawProjectiles(ProjectileManager *pm) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->projectiles[i].active) continue;

        Projectile *p = &pm->projectiles[i];

        // Vẽ projectile: vòng tròn đỏ + glow effect
        DrawCircleV(p->position, p->radius + 4, (Color){200, 0, 50, 100});
        DrawCircleV(p->position, p->radius, (Color){255, 50, 100, 255});
        DrawCircleV(p->position, p->radius * 0.5f, (Color){255, 200, 200, 255});
    }
}