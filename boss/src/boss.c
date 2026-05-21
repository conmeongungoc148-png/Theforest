#include "boss.h"
#include "collision.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Lazy-loaded sprites for visual effects
static Texture2D explosionTex = {0};
static Texture2D laserTex = {0};
static bool spritesLoaded = false;

// === ATOM BOMB GLOBAL ===
// Player.c reads this flag to force-kill cat khi atom bomb trigger
int gAtomBombActive = 0;

#define ATOM_TAUNT_TIME 270.0f   // 4:30 - boss bắt đầu trash talk
#define ATOM_TRIGGER_TIME 300.0f // 5:00 - bom nổ
#define EXPLOSION_FRAMES 12
#define EXPLOSION_FW 96
#define EXPLOSION_FH 96
#define LASER_FRAMES 12
#define LASER_COLS 4
#define LASER_FW 512
#define LASER_FH 655

static void LoadBossSprites(void) {
    if (spritesLoaded) return;
    explosionTex = LoadTexture("assets/explosion/Explosion.png");
    laserTex = LoadTexture("assets/laser/texture_laser.png");
    spritesLoaded = true;
}

#define PROJECTILE_SPEED_BASE 380.0f
#define INTRO_DURATION 15.5f
#define ROAR_DURATION 2.0f
#define LASER_CHARGE_TIME 2.0f   // 2s cảnh báo nhấp nháy
#define LASER_DURATION 5.0f      // Max duration (thường tắt sớm khi chạm mép)
#define LASER_EXTEND_SPEED 400.0f // Tốc độ laser kéo dài từ lockPos theo direction
#define LASER_MIN_PLAYER_X 200.0f // Player phải ở trong vùng này mới ra laser
#define LASER_MAX_PLAYER_X 1080.0f
#define SLAM_WARNING_TIME 2.0f   // Tăng từ 1.2 → 2.0 cho player thời gian phản ứng
#define SLAM_DURATION 1.0f
#define HAZARD_WARNING_TIME 1.0f
#define ORB_CHARGE_TIME 1.5f
#define CLAW_WARNING_TIME 0.8f
#define CLAW_DURATION 0.6f

// Vị trí tay boss (relative to boss center) - dùng để spawn orb/projectile
#define LEFT_HAND_OFFSET_X (-100.0f)
#define LEFT_HAND_OFFSET_Y (50.0f)
#define RIGHT_HAND_OFFSET_X (100.0f)
#define RIGHT_HAND_OFFSET_Y (50.0f)

void InitBoss(Boss *boss, Vector2 startPos, Vector2 targetPos) {
    boss->position = startPos;  // Bắt đầu từ dưới màn hình
    boss->targetY = targetPos.y;
    boss->hp = BOSS_MAX_HP;
    boss->maxHp = BOSS_MAX_HP;
    boss->phase = BOSS_PHASE_1;
    boss->state = BOSS_INTRO;
    boss->defeated = false;

    // Intro
    boss->introTimer = 0.0f;
    boss->roarTimer = 0.0f;

    // Attack system
    boss->attackTimer = 0.0f;
    boss->attackInterval = 2.2f;
    boss->nextAttack = ATTACK_PROJECTILE;
    boss->lastAttack = ATTACK_PROJECTILE;
    for (int i = 0; i < 7; i++) boss->attackCooldowns[i] = 0.0f;

    // Trash talk
    boss->tauntTimer = 0.0f;
    boss->tauntText = "";

    // Orb system
    boss->orbTimer = 0.0f;
    boss->orbInterval = 6.0f;
    boss->orbActive = false;
    boss->orbChargeTimer = 0.0f;
    boss->orbSpawnPos = (Vector2){0, 0};

    // Laser
    boss->laserActive = false;
    boss->laserChargeTime = 0.0f;
    boss->laserDuration = 0.0f;
    boss->laserStart = (Vector2){0, 0};
    boss->laserEnd = (Vector2){0, 0};
    boss->laserDirection = (Vector2){1, 0};

    // Slam
    boss->slamActive = false;
    boss->slamWarningTime = 0.0f;
    boss->slamTimer = 0.0f;
    boss->slamPos = (Vector2){0, 0};
    boss->shockwaveRadius = 0.0f;

    // Hazard
    boss->hazardCount = 0;
    for (int i = 0; i < 5; i++) {
        boss->hazardPositions[i] = (Vector2){0, 0};
        boss->hazardWarningTime[i] = 0.0f;
        boss->hazardActive[i] = false;
    }
    
    // Claw
    boss->clawActive = false;
    boss->clawZone = CLAW_ZONE_LEFT;
    boss->clawWarningTime = 0.0f;
    boss->clawDuration = 0.0f;

    // Animation
    boss->currentFrame = 0;
    boss->frameTimer = 0.0f;
    boss->animSpeed = 0.1f;

    // Visual
    boss->shakeTimer = 0.0f;
    boss->shakeIntensity = 0.0f;
    boss->scale = 3.0f;  // Boss to full screen
    
    // Death
    boss->deathTimer = 0.0f;
    boss->deathFlashTimer = 0.0f;

    // Atom bomb
    boss->fightTimer = 0.0f;
    boss->atomTimer = 0.0f;
    boss->atomTriggered = false;
    gAtomBombActive = 0;  // Reset flag khi init/restart

    // Hurtbox
    boss->hurtBox = (Rectangle){
        targetPos.x - 100, targetPos.y - 100, 200, 200
    };
}

// === TRASH TALK SYSTEM ===
static const char *TAUNTS_ATK[] = {"HET TRON DI!", "CHAM QUA!", "HAHAHA!", "MEOW~", "YEU THE?"};
static const char *TAUNTS_HIT[] = {"DAU!", "ARGH!", "DUOC LAM!", "..."};
static const char *TAUNTS_P4[] = {"MAY CHET CHAC!", "FINAL FORM!", "KHONG THE NE!", "HET DUONG CHAY!"};

static void TriggerTaunt(Boss *boss, const char **pool, int poolSize, int chance) {
    if (rand() % 100 >= chance) return;
    if (boss->tauntTimer > 0.5f) return;
    boss->tauntText = pool[rand() % poolSize];
    boss->tauntTimer = 2.5f;
}

static void UpdatePhase(Boss *boss) {
    float hpPercent = (float)boss->hp / boss->maxHp;
    BossPhase newPhase = boss->phase;

    if (hpPercent > 0.7f) newPhase = BOSS_PHASE_1;
    else if (hpPercent > 0.4f) newPhase = BOSS_PHASE_2;
    else if (hpPercent > 0.15f) newPhase = BOSS_PHASE_3;
    else newPhase = BOSS_PHASE_4;

    if (newPhase != boss->phase) {
        BossPhase oldPhase = boss->phase;
        boss->phase = newPhase;
        switch (newPhase) {
            case BOSS_PHASE_1:
                boss->attackInterval = 2.2f;
                boss->orbInterval = 6.0f;
                break;
            case BOSS_PHASE_2:
                boss->attackInterval = 1.6f;
                boss->orbInterval = 5.5f;
                break;
            case BOSS_PHASE_3:
                boss->attackInterval = 1.1f;
                boss->orbInterval = 5.0f;
                break;
            case BOSS_PHASE_4:
                boss->attackInterval = 1.5f;  // Giảm spam từ 0.8 → 1.5 (vẫn nhanh nhưng không ngộp)
                boss->orbInterval = 4.5f;
                break;
        }
        // Phase 4 → trash talk 100% (final form moment)
        if (newPhase == BOSS_PHASE_4 && oldPhase != BOSS_PHASE_4) {
            boss->tauntText = TAUNTS_P4[rand() % 4];
            boss->tauntTimer = 3.5f;
        }
    }
}

// Cooldown per attack type (seconds)
static const float ATTACK_COOLDOWN_TABLE[7] = {
    0.5f,   // PROJECTILE - cho phép spam nhẹ
    4.0f,   // LASER - chiêu nguy hiểm, cooldown lâu
    3.0f,   // SLAM
    5.0f,   // HAZARD - chiếm sàn lâu
    3.0f,   // CLAW
    6.0f,   // BARRAGE - Phase 4 chiêu mạnh
    5.0f    // RAIN
};

static AttackType RollAttack(Boss *boss) {
    int roll = rand() % 100;
    switch (boss->phase) {
        case BOSS_PHASE_1:
            return ATTACK_PROJECTILE;
        case BOSS_PHASE_2:
            if (roll < 40) return ATTACK_PROJECTILE;
            else if (roll < 70) return ATTACK_LASER;
            else return ATTACK_SLAM;
        case BOSS_PHASE_3:
            if (roll < 20) return ATTACK_PROJECTILE;
            else if (roll < 40) return ATTACK_LASER;
            else if (roll < 55) return ATTACK_SLAM;
            else if (roll < 75) return ATTACK_HAZARD;
            else return ATTACK_CLAW;
        case BOSS_PHASE_4:
            if (roll < 12) return ATTACK_PROJECTILE;
            else if (roll < 24) return ATTACK_LASER;
            else if (roll < 36) return ATTACK_SLAM;
            else if (roll < 48) return ATTACK_HAZARD;
            else if (roll < 60) return ATTACK_CLAW;
            else if (roll < 80) return ATTACK_BARRAGE;
            else return ATTACK_RAIN;
    }
    return ATTACK_PROJECTILE;
}

static Vector2 lastPlayerPosForLaser = {640, 620}; // Track player pos cho laser check

static AttackType ChooseAttack(Boss *boss) {
    // Anti-spam: re-roll nếu trùng lastAttack hoặc đang cooldown (max 5 lần)
    for (int attempt = 0; attempt < 5; attempt++) {
        AttackType atk = RollAttack(boss);
        // Rule 1: không cho trùng chiêu vừa dùng (trừ PROJECTILE ở Phase 1)
        if (atk == boss->lastAttack && boss->phase != BOSS_PHASE_1) continue;
        // Rule 2: kiểm tra cooldown
        if (boss->attackCooldowns[(int)atk] > 0) continue;
        // Rule 3: LASER chỉ ra khi player ở vùng giữa (không gần mép)
        if (atk == ATTACK_LASER) {
            if (lastPlayerPosForLaser.x < LASER_MIN_PLAYER_X || lastPlayerPosForLaser.x > LASER_MAX_PLAYER_X) {
                continue;
            }
        }
        return atk;
    }
    // Fallback: trả về PROJECTILE (luôn available)
    return ATTACK_PROJECTILE;
}

static void StartClawAttack(Boss *boss, Vector2 playerPos) {
    boss->clawActive = true;
    boss->clawWarningTime = CLAW_WARNING_TIME;
    boss->clawDuration = 0.0f;  // Sẽ active sau warning
    
    // 70% chọn zone player đang đứng (predict), 30% random
    int roll = rand() % 100;
    if (roll < 70) {
        // Predict zone player
        if (playerPos.x < 440.0f) boss->clawZone = CLAW_ZONE_LEFT;
        else if (playerPos.x < 840.0f) boss->clawZone = CLAW_ZONE_MIDDLE;
        else boss->clawZone = CLAW_ZONE_RIGHT;
    } else {
        // Random zone (player có thể né bằng cách đoán)
        boss->clawZone = (ClawZone)(rand() % 3);
    }
}

static void DoProjectileAttack(Boss *boss, Vector2 playerPos, ProjectileManager *pm) {
    // Spawn từ tay trái hoặc phải
    int hand = rand() % 2;
    Vector2 spawnPos = {
        boss->position.x + (hand == 0 ? LEFT_HAND_OFFSET_X : RIGHT_HAND_OFFSET_X) * boss->scale / 3.0f,
        boss->position.y + (hand == 0 ? LEFT_HAND_OFFSET_Y : RIGHT_HAND_OFFSET_Y) * boss->scale / 3.0f
    };

    Vector2 dir = NormalizeVec((Vector2){
        playerPos.x - spawnPos.x,
        playerPos.y - spawnPos.y
    });

    float speed = PROJECTILE_SPEED_BASE;

    switch (boss->phase) {
        case BOSS_PHASE_1:
            SpawnProjectile(pm, spawnPos, (Vector2){dir.x * speed, dir.y * speed}, 1);
            break;
        case BOSS_PHASE_2:
            speed *= 1.1f;
            SpawnProjectile(pm, spawnPos, (Vector2){dir.x * speed, dir.y * speed}, 1);
            {
                float angle = atan2f(dir.y, dir.x);
                float aL = angle - 0.3f, aR = angle + 0.3f;
                SpawnProjectile(pm, spawnPos, (Vector2){cosf(aL)*speed, sinf(aL)*speed}, 1);
                SpawnProjectile(pm, spawnPos, (Vector2){cosf(aR)*speed, sinf(aR)*speed}, 1);
            }
            break;
        case BOSS_PHASE_3:
            speed *= 1.2f;
            {
                float baseAngle = atan2f(dir.y, dir.x);
                for (int i = -2; i <= 2; i++) {
                    float a = baseAngle + i * 0.2f;
                    SpawnProjectile(pm, spawnPos, (Vector2){cosf(a)*speed, sinf(a)*speed}, 1);
                }
            }
            break;
        case BOSS_PHASE_4:
            // Phase 4: 7 viên fan + speed cao hơn (final phase chaos)
            speed *= 1.35f;
            {
                float baseAngle = atan2f(dir.y, dir.x);
                for (int i = -3; i <= 3; i++) {
                    float a = baseAngle + i * 0.18f;
                    SpawnProjectile(pm, spawnPos, (Vector2){cosf(a)*speed, sinf(a)*speed}, 1);
                }
            }
            break;
    }
}

// Laser: gốc tại boss, lock vị trí player TRÊN PLATFORM (y=620, không lock trên không trung)
// Sau warning hết → tính direction từ vị trí player project xuống sàn → bắn cố định
// Tự tắt khi đầu xa chạm cạnh map
static void StartLaserAttack(Boss *boss, Vector2 playerPos) {
    boss->laserActive = true;
    boss->laserChargeTime = LASER_CHARGE_TIME;
    boss->laserDuration = LASER_DURATION;
    boss->laserStart = boss->position;
    // Project player xuống platform (y=620) — dù player đang nhảy thì lock vẫn ở mặt sàn
    boss->laserEnd = (Vector2){ playerPos.x, 620.0f };
    boss->laserDirection = (Vector2){1, 0};  // Sẽ được lock khi warning kết thúc
}

static void StartSlamAttack(Boss *boss, Vector2 playerPos) {
    boss->slamActive = true;
    boss->slamWarningTime = SLAM_WARNING_TIME;  // Warning trước khi đập
    boss->slamTimer = 0.0f;  // Chưa bắt đầu shockwave
    boss->slamPos = (Vector2){playerPos.x, 620.0f};  // Vị trí target (player hiện tại)
    boss->shockwaveRadius = 0.0f;
}

static void StartHazardAttack(Boss *boss) {
    boss->hazardCount = 3 + rand() % 3;  // 3-5 hazards
    for (int i = 0; i < boss->hazardCount; i++) {
        boss->hazardPositions[i] = (Vector2){
            100.0f + (float)(rand() % 1080),  // Random X trên sàn
            620.0f  // Trên sàn
        };
        boss->hazardWarningTime[i] = HAZARD_WARNING_TIME;
        boss->hazardActive[i] = false;
    }
}

// === PHASE 4 ONLY: BARRAGE - 360° radial burst ===
static void DoBarrageAttack(Boss *boss, ProjectileManager *pm) {
    // Bắn 12 projectile theo 360° xung quanh boss (cả 2 tay)
    Vector2 leftHand = {
        boss->position.x + LEFT_HAND_OFFSET_X * boss->scale / 3.0f,
        boss->position.y + LEFT_HAND_OFFSET_Y * boss->scale / 3.0f
    };
    Vector2 rightHand = {
        boss->position.x + RIGHT_HAND_OFFSET_X * boss->scale / 3.0f,
        boss->position.y + RIGHT_HAND_OFFSET_Y * boss->scale / 3.0f
    };
    
    float speed = PROJECTILE_SPEED_BASE * 1.1f;
    float twoPi = 6.2831853f;
    
    // 6 viên từ tay trái (offset góc 0)
    for (int i = 0; i < 6; i++) {
        float angle = (twoPi / 12.0f) * i;
        SpawnProjectile(pm, leftHand,
            (Vector2){cosf(angle) * speed, sinf(angle) * speed}, 1);
    }
    // 6 viên từ tay phải (offset góc 30°)
    for (int i = 0; i < 6; i++) {
        float angle = (twoPi / 12.0f) * i + (twoPi / 24.0f);
        SpawnProjectile(pm, rightHand,
            (Vector2){cosf(angle) * speed, sinf(angle) * speed}, 1);
    }
    
    // Shake hiệu ứng nổ
    boss->shakeTimer = 0.3f;
    boss->shakeIntensity = 15.0f;
}

// === PHASE 4 ONLY: RAIN - Mưa projectile từ trên trời ===
static void DoRainAttack(Boss *boss, ProjectileManager *pm) {
    // Spawn 6 projectile rơi từ y=0 với vận tốc xuống nhẹ
    // Spread đều theo X để player phải di chuyển nhiều
    float screenW = 1200.0f;
    float spacing = screenW / 6.0f;
    float speed = 280.0f;  // Chậm hơn để player có thời gian né
    
    for (int i = 0; i < 6; i++) {
        // X position with random offset (không hoàn toàn đều)
        float xOffset = (float)(rand() % 60 - 30);  // ±30px
        float x = 80.0f + spacing * i + spacing * 0.5f + xOffset;
        
        Vector2 spawnPos = {x, -20.0f};
        
        // Vận tốc xuống + tilt nhẹ về phía center
        float tiltX = (640.0f - x) * 0.05f;  // Drift về center
        Vector2 vel = {tiltX, speed};
        
        SpawnProjectile(pm, spawnPos, vel, 1);
    }
    
    boss->shakeTimer = 0.2f;
    boss->shakeIntensity = 10.0f;
}

void UpdateBoss(Boss *boss, Vector2 playerPos, ProjectileManager *pm, OrbManager *om, float dt, float *cameraShake) {
    // === DEATH SEQUENCE ===
    if (boss->defeated) {
        boss->deathTimer += dt;
        
        // Vẫn chạy animation khi chết (giật nhanh hơn)
        boss->frameTimer += dt;
        if (boss->frameTimer >= 0.05f) {
            boss->frameTimer = 0;
            boss->currentFrame = (boss->currentFrame + 1) % BOSS_TOTAL_FRAMES;
        }
        
        // Camera shake mạnh trong 2s đầu
        if (boss->deathTimer < 2.0f) {
            float intensity = 1.0f - (boss->deathTimer / 2.0f);
            *cameraShake = sinf(boss->deathTimer * 30.0f) * 15.0f * intensity;
        } else {
            *cameraShake = 0;
        }
        
        // Flash effect (3 lần đầu)
        if (boss->deathTimer < 1.5f) {
            boss->deathFlashTimer = sinf(boss->deathTimer * 15.0f) * 0.5f + 0.5f;
        } else {
            boss->deathFlashTimer = 0;
        }
        
        // Boss từ từ rơi xuống
        if (boss->deathTimer < 3.0f) {
            float fallProgress = boss->deathTimer / 3.0f;
            boss->position.y += 30.0f * dt * fallProgress;
        }
        
        return;
    }

    // Animation idle (luôn chạy)
    boss->frameTimer += dt;
    if (boss->frameTimer >= boss->animSpeed) {
        boss->frameTimer = 0;
        boss->currentFrame = (boss->currentFrame + 1) % BOSS_TOTAL_FRAMES;
    }

    // === INTRO SEQUENCE ===
    if (boss->state == BOSS_INTRO) {
        boss->introTimer += dt;
        float progress = boss->introTimer / INTRO_DURATION;
        if (progress > 1.0f) progress = 1.0f;
        
        // Boss rise QUICKLY trong 3s đầu (20% intro), sau đó stay visible với floating effect
        float startY = 900.0f;
        if (progress < 0.20f) {
            // Phase 1 (0-3s): rise nhanh từ dưới lên
            float riseProgress = progress / 0.20f;
            float eased = 1.0f - powf(1.0f - riseProgress, 3.0f);  // ease-out cubic
            boss->position.y = startY + (boss->targetY - startY) * eased;
        } else {
            // Phase 2 (3-15s): boss visible, floating nhẹ + tăng intensity
            boss->position.y = boss->targetY + sinf(boss->introTimer * 2.0f) * 8.0f;
        }
        
        // Camera shake tăng dần (mạnh dần khi gần ROAR)
        float shakeIntensity = 2.0f + progress * 6.0f;
        *cameraShake = sinf(boss->introTimer * 8.0f) * shakeIntensity;
        
        // Animation tăng tốc dần (boss "thức dậy")
        boss->animSpeed = 0.25f - progress * 0.18f;  // 0.25 → 0.07
        
        if (progress >= 1.0f) {
            boss->state = BOSS_ROAR;
            boss->roarTimer = ROAR_DURATION;
            boss->animSpeed = 0.02f;  // Animation x5 nhanh hơn khi ROAR
        }
        return;
    }

    // === ROAR SEQUENCE ===
    if (boss->state == BOSS_ROAR) {
        boss->roarTimer -= dt;
        // Screen shake mạnh
        *cameraShake = sinf(boss->roarTimer * 30.0f) * 8.0f * (boss->roarTimer / ROAR_DURATION);
        
        if (boss->roarTimer <= 0) {
            boss->state = BOSS_FIGHTING;
            *cameraShake = 0;
            boss->animSpeed = 0.1f;  // Trả về tốc độ bình thường khi vào fighting
        }
        return;
    }

    // === DYING: Boss đứng yên, chờ ending music ===
    if (boss->state == BOSS_DYING) {
        // Chỉ chạy animation idle, không attack
        return;
    }

    // === FIGHTING ===
    UpdatePhase(boss);

    // === ATOM BOMB TIMER (5 phút force end) ===
    boss->fightTimer += dt;

    // 4:30 - boss bắt đầu trash talk warning
    if (boss->fightTimer >= ATOM_TAUNT_TIME && boss->fightTimer < ATOM_TAUNT_TIME + dt) {
        boss->tauntText = "MAY GA VCL!";
        boss->tauntTimer = 5.0f;
    }

    // 5:00 - trigger atom bomb
    if (boss->fightTimer >= ATOM_TRIGGER_TIME && !boss->atomTriggered) {
        boss->atomTriggered = true;
        boss->atomTimer = 0.0f;
        gAtomBombActive = 1;  // Force-kill player
        *cameraShake = 30.0f;
    }

    // Atom bomb animation timer
    if (boss->atomTriggered) {
        boss->atomTimer += dt;
        // Camera shake mạnh trong 2s đầu của atom
        if (boss->atomTimer < 2.0f) {
            *cameraShake = 30.0f * (1.0f - boss->atomTimer / 2.0f);
        }
    }

    // Shake effect (khi bị đánh)
    if (boss->shakeTimer > 0) {
        boss->shakeTimer -= dt;
        if (boss->shakeTimer < 0) boss->shakeTimer = 0;
    }

    // Track player pos cho laser check
    lastPlayerPosForLaser = playerPos;

    // --- Attack timer ---
    boss->attackTimer += dt;
    if (boss->attackTimer >= boss->attackInterval && !boss->laserActive && !boss->slamActive && !boss->clawActive) {
        boss->attackTimer = 0;
        AttackType atk = ChooseAttack(boss);
        
        switch (atk) {
            case ATTACK_PROJECTILE:
                DoProjectileAttack(boss, playerPos, pm);
                break;
            case ATTACK_LASER:
                StartLaserAttack(boss, playerPos);
                break;
            case ATTACK_SLAM:
                StartSlamAttack(boss, playerPos);
                break;
            case ATTACK_HAZARD:
                StartHazardAttack(boss);
                break;
            case ATTACK_CLAW:
                StartClawAttack(boss, playerPos);
                break;
            case ATTACK_BARRAGE:
                DoBarrageAttack(boss, pm);
                break;
            case ATTACK_RAIN:
                DoRainAttack(boss, pm);
                break;
        }
        // Anti-spam: ghi nhận chiêu vừa dùng + set cooldown
        boss->lastAttack = atk;
        boss->attackCooldowns[(int)atk] = ATTACK_COOLDOWN_TABLE[(int)atk];
        // Trash talk khi tấn công (30% chance)
        TriggerTaunt(boss, TAUNTS_ATK, 5, 30);
    }

    // Decrement taunt timer
    if (boss->tauntTimer > 0) {
        boss->tauntTimer -= dt;
        if (boss->tauntTimer < 0) boss->tauntTimer = 0;
    }

    // Decrement cooldowns mỗi frame
    for (int i = 0; i < 7; i++) {
        if (boss->attackCooldowns[i] > 0) {
            boss->attackCooldowns[i] -= dt;
            if (boss->attackCooldowns[i] < 0) boss->attackCooldowns[i] = 0;
        }
    }
    
    // --- Update Claw ---
    if (boss->clawActive) {
        if (boss->clawWarningTime > 0) {
            boss->clawWarningTime -= dt;
            if (boss->clawWarningTime <= 0) {
                // Bắt đầu cào
                boss->clawDuration = CLAW_DURATION;
                boss->shakeTimer = 0.4f;
                boss->shakeIntensity = 20.0f;
            }
        } else if (boss->clawDuration > 0) {
            boss->clawDuration -= dt;
            if (boss->clawDuration <= 0) {
                boss->clawActive = false;
            }
        }
    }

    // --- Update Laser ---
    // Workflow: trigger → lock vị trí player → warning đứng yên 2s → lấy hướng player cuối → bắn cố định
    if (boss->laserActive) {
        // Gốc 1 luôn là vị trí boss
        boss->laserStart = boss->position;

        if (boss->laserChargeTime > 0) {
            // WARNING phase: laserEnd ĐỨNG YÊN tại vị trí đã lock (set trong StartLaserAttack)
            // KHÔNG track player — player có 2s để chạy
            float prev = boss->laserChargeTime;
            boss->laserChargeTime -= dt;

            // Khi warning vừa kết thúc → lấy vị trí player project xuống PLATFORM (y=620)
            // → tính direction từ lockPos về groundPlayer (vẫn trên cùng mặt sàn)
            // → laser sẽ đi NGANG dọc platform, không bay vào không trung
            if (prev > 0 && boss->laserChargeTime <= 0) {
                Vector2 groundPlayer = { playerPos.x, 620.0f };
                Vector2 d = {
                    groundPlayer.x - boss->laserEnd.x,
                    groundPlayer.y - boss->laserEnd.y
                };
                float len = sqrtf(d.x*d.x + d.y*d.y);
                if (len < 1.0f) {
                    // Player đứng đúng vị trí lock → mặc định bắn ngang phải
                    boss->laserDirection = (Vector2){1.0f, 0.0f};
                } else {
                    boss->laserDirection = (Vector2){d.x / len, d.y / len};
                }
            }
        } else {
            // FIRING phase: laserEnd bắt đầu từ lockPos, kéo dài DẦN theo direction
            // KHÔNG teleport — laser lan ra từ vị trí cảnh báo về hướng player
            boss->laserDuration -= dt;

            // Kéo dài laserEnd theo direction mỗi frame
            boss->laserEnd.x += boss->laserDirection.x * LASER_EXTEND_SPEED * dt;
            boss->laserEnd.y += boss->laserDirection.y * LASER_EXTEND_SPEED * dt;

            // Tắt khi laserEnd chạm GẦN SÁT mép map
            if (boss->laserEnd.x <= 50.0f || boss->laserEnd.x >= 1230.0f ||
                boss->laserEnd.y <= 10.0f || boss->laserEnd.y >= 710.0f) {
                boss->laserActive = false;
            }

            // Hoặc hết duration (safety)
            if (boss->laserDuration <= 0) {
                boss->laserActive = false;
            }
        }
    }

    // --- Update Slam ---
    if (boss->slamActive) {
        if (boss->slamWarningTime > 0) {
            // Warning phase: hiện indicator, chưa gây damage
            boss->slamWarningTime -= dt;
            if (boss->slamWarningTime <= 0) {
                // Warning hết → bắt đầu shockwave
                boss->slamTimer = SLAM_DURATION;
                boss->shockwaveRadius = 0.0f;
                boss->shakeTimer = 0.4f;
                boss->shakeIntensity = 25.0f;
            }
        } else {
            // Shockwave expanding
            boss->slamTimer -= dt;
            float progress = 1.0f - (boss->slamTimer / SLAM_DURATION);
            boss->shockwaveRadius = progress * 300.0f;
            
            if (boss->slamTimer <= 0) {
                boss->slamActive = false;
                boss->shockwaveRadius = 0;
            }
        }
    }

    // --- Update Hazards ---
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardWarningTime[i] > 0) {
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] <= 0) {
                boss->hazardActive[i] = true;
            }
        } else if (boss->hazardActive[i]) {
            // Hazard active for 2 seconds then disappear
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] < -2.0f) {
                boss->hazardActive[i] = false;
            }
        }
    }

    // --- Orb system (1 orb at a time) ---
    if (!boss->orbActive) {
        boss->orbTimer += dt;
        if (boss->orbTimer >= boss->orbInterval) {
            boss->orbTimer = 0;
            boss->orbChargeTimer = ORB_CHARGE_TIME;
            // Chọn tay spawn
            int hand = rand() % 2;
            boss->orbSpawnPos = (Vector2){
                boss->position.x + (hand == 0 ? LEFT_HAND_OFFSET_X : RIGHT_HAND_OFFSET_X) * boss->scale / 3.0f,
                boss->position.y + (hand == 0 ? LEFT_HAND_OFFSET_Y : RIGHT_HAND_OFFSET_Y) * boss->scale / 3.0f
            };
        }
    }

    // Orb charge animation
    if (boss->orbChargeTimer > 0) {
        boss->orbChargeTimer -= dt;
        if (boss->orbChargeTimer <= 0) {
            // Spawn orb - bắn theo hướng player rồi rơi xuống (parabolic)
            SpawnOrbToward(om, boss->orbSpawnPos, playerPos);
            boss->orbActive = true;
        }
    }

    // Cập nhật hurtbox
    boss->hurtBox.x = boss->position.x - 100;
    boss->hurtBox.y = boss->position.y - 100;
}

void DrawBoss(Boss *boss, Texture2D spriteSheet, float cameraOffsetY) {
    // Lazy load sprites on first draw
    LoadBossSprites();

    // === DEATH ANIMATION ===
    if (boss->defeated) {
        // Boss vẫn hiện nhưng fade out + flash + explosions
        float deathProgress = boss->deathTimer / 4.0f;
        if (deathProgress > 1.0f) deathProgress = 1.0f;
        
        // Shake cực mạnh
        float shakeX = (float)(rand() % 40 - 20) * (1.0f - deathProgress);
        float shakeY = (float)(rand() % 40 - 20) * (1.0f - deathProgress);
        
        int frameIdx = boss->currentFrame;
        Rectangle source = { (float)frameIdx * BOSS_FRAME_W, 0, (float)BOSS_FRAME_W, (float)BOSS_FRAME_H };
        float destW = (float)BOSS_FRAME_W * boss->scale;
        float destH = (float)BOSS_FRAME_H * boss->scale;
        Rectangle dest = {
            boss->position.x + shakeX,
            boss->position.y + shakeY,
            destW,
            destH
        };
        Vector2 origin = { destW / 2.0f, destH / 2.0f };
        
        // Flash effect: trắng/đỏ trong 1.5s đầu
        Color tint = WHITE;
        if (boss->deathTimer < 1.5f) {
            float flash = boss->deathFlashTimer;
            tint = (Color){
                255,
                (unsigned char)(50 + flash * 150),
                (unsigned char)(50 + flash * 150),
                (unsigned char)((1.0f - deathProgress) * 255)
            };
        } else {
            // Fade out
            tint.a = (unsigned char)((1.0f - deathProgress) * 255);
            tint.r = 255;
            tint.g = (unsigned char)(100 + (1.0f - deathProgress) * 155);
            tint.b = (unsigned char)(100 + (1.0f - deathProgress) * 155);
        }
        
        DrawTexturePro(spriteSheet, source, dest, origin, 0.0f, tint);
        
        // Explosion particles - nhiều vòng tròn lan rộng
        for (int ring = 0; ring < 5; ring++) {
            float ringOffset = ring * 0.3f;
            float ringTime = boss->deathTimer - ringOffset;
            if (ringTime > 0 && ringTime < 2.0f) {
                float radius = ringTime * 200.0f;
                float alpha = 1.0f - (ringTime / 2.0f);
                DrawCircleLines((int)boss->position.x, (int)boss->position.y, radius,
                    (Color){255, 200, 50, (unsigned char)(alpha * 200)});
                DrawCircleLines((int)boss->position.x, (int)boss->position.y, radius * 0.95f,
                    (Color){255, 100, 0, (unsigned char)(alpha * 150)});
            }
        }
        
        // Particles bay ra (random sparks)
        for (int p = 0; p < 30; p++) {
            float angle = (float)p * (3.14159f * 2.0f / 30.0f) + boss->deathTimer;
            float dist = boss->deathTimer * 300.0f + (float)(rand() % 50);
            float alpha = 1.0f - deathProgress;
            Vector2 pp = {
                boss->position.x + cosf(angle) * dist,
                boss->position.y + sinf(angle) * dist
            };
            DrawCircleV(pp, 4.0f + (float)(rand() % 4),
                (Color){255, (unsigned char)(150 + rand() % 105), 0, (unsigned char)(alpha * 230)});
        }
        
        // Explosion sprite animation (12 frames, loop during death)
        if (explosionTex.id > 0 && boss->deathTimer < 3.0f) {
            int expFrame = ((int)(boss->deathTimer * 10.0f)) % EXPLOSION_FRAMES;
            Rectangle expSrc = { (float)(expFrame * EXPLOSION_FW), 0, (float)EXPLOSION_FW, (float)EXPLOSION_FH };
            float expScale = 3.0f + boss->deathTimer * 2.0f;
            Rectangle expDst = {
                boss->position.x - expScale * EXPLOSION_FW / 2.0f,
                boss->position.y - expScale * EXPLOSION_FH / 2.0f,
                expScale * EXPLOSION_FW,
                expScale * EXPLOSION_FH
            };
            float expAlpha = 1.0f - deathProgress;
            DrawTexturePro(explosionTex, expSrc, expDst, (Vector2){0,0}, 0,
                (Color){255, 255, 255, (unsigned char)(expAlpha * 255)});
        }

        // Big explosion flash trong 0.5s đầu
        if (boss->deathTimer < 0.5f) {
            float flashAlpha = 1.0f - (boss->deathTimer / 0.5f);
            DrawCircleV(boss->position, 200.0f + boss->deathTimer * 400.0f,
                (Color){255, 255, 200, (unsigned char)(flashAlpha * 255)});
        }
        
        // Whitewash overlay khi explosion (full screen)
        if (boss->deathTimer < 0.3f) {
            float wAlpha = 1.0f - (boss->deathTimer / 0.3f);
            DrawRectangle(-2000, -2000, 5000, 5000, 
                (Color){255, 255, 255, (unsigned char)(wAlpha * 200)});
        }
        
        return;
    }

    float shakeX = 0, shakeY = 0;
    if (boss->shakeTimer > 0) {
        shakeX = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
        shakeY = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
    }

    int frameIdx = boss->currentFrame;
    float frameX = (float)frameIdx * BOSS_FRAME_W;

    Rectangle source = { frameX, 0, (float)BOSS_FRAME_W, (float)BOSS_FRAME_H };
    
    float destW = (float)BOSS_FRAME_W * boss->scale;
    float destH = (float)BOSS_FRAME_H * boss->scale;
    Rectangle dest = {
        boss->position.x + shakeX,
        boss->position.y + shakeY + cameraOffsetY,
        destW,
        destH
    };

    Vector2 origin = { destW / 2.0f, destH / 2.0f };

    // Tint theo phase
    Color tint = WHITE;
    if (boss->phase == BOSS_PHASE_2) tint = (Color){255, 200, 200, 255};
    if (boss->phase == BOSS_PHASE_3) tint = (Color){255, 100, 100, 255};
    if (boss->phase == BOSS_PHASE_4) tint = (Color){200, 50, 255, 255};  // Purple rage

    // Intro fade-in
    if (boss->state == BOSS_INTRO) {
        float alpha = boss->introTimer / INTRO_DURATION;
        if (alpha > 1.0f) alpha = 1.0f;
        tint.a = (unsigned char)(alpha * 255);
    }

    // Roar flash effect
    if (boss->state == BOSS_ROAR) {
        if ((int)(boss->roarTimer * 10) % 2 == 0) {
            tint = (Color){255, 255, 255, 255};
        } else {
            tint = (Color){255, 50, 50, 255};
        }
    }

    DrawTexturePro(spriteSheet, source, dest, origin, 0.0f, tint);

    // --- Draw Taunt Text (above boss) ---
    if (boss->tauntTimer > 0 && boss->state == BOSS_FIGHTING) {
        float alpha = (boss->tauntTimer > 1.0f) ? 1.0f : boss->tauntTimer;
        int textW = MeasureText(boss->tauntText, 20);
        DrawText(boss->tauntText,
            (int)(boss->position.x - textW/2),
            (int)(boss->position.y - 200),
            20, (Color){255, 255, 100, (unsigned char)(alpha * 230)});
    }

    // --- Draw Orb Charge Effect ---
    if (boss->orbChargeTimer > 0) {
        float progress = 1.0f - (boss->orbChargeTimer / ORB_CHARGE_TIME);
        float radius = 5.0f + progress * 20.0f;
        Color orbColor = (Color){255, 255, 0, (unsigned char)(progress * 200)};
        DrawCircleV(boss->orbSpawnPos, radius, orbColor);
        // Particles around charge
        for (int i = 0; i < 6; i++) {
            float angle = (float)i * (3.14159f * 2.0f / 6.0f) + boss->frameTimer * 5.0f;
            float dist = 30.0f * (1.0f - progress);
            Vector2 p = {
                boss->orbSpawnPos.x + cosf(angle) * dist,
                boss->orbSpawnPos.y + sinf(angle) * dist
            };
            DrawCircleV(p, 3.0f, (Color){255, 200, 0, (unsigned char)(progress * 150)});
        }
    }

    // --- Draw Laser ---
    if (boss->laserActive) {
        if (boss->laserChargeTime > 0) {
            // Charging: thin red line blinking
            float alpha = (sinf(boss->laserChargeTime * 20.0f) + 1.0f) * 0.5f;
            Color c = (Color){255, 0, 0, (unsigned char)(alpha * 150)};
            DrawLineEx(boss->laserStart, boss->laserEnd, 3.0f, c);
            // Warning circle at end
            DrawCircleV(boss->laserEnd, 20.0f, (Color){255, 0, 0, (unsigned char)(alpha * 100)});
        } else {
            // Firing: thick beam
            float pulse = (sinf(boss->laserDuration * 15.0f) + 1.0f) * 0.5f;
            float width = 15.0f + pulse * 10.0f;
            DrawLineEx(boss->laserStart, boss->laserEnd, width, (Color){255, 50, 50, 230});
            DrawLineEx(boss->laserStart, boss->laserEnd, width * 0.5f, (Color){255, 200, 200, 200});
            // Glow at start
            DrawCircleV(boss->laserStart, 20.0f, (Color){255, 100, 100, 150});
        }
    }

    // --- Draw Slam Warning + Shockwave ---
    if (boss->slamActive) {
        if (boss->slamWarningTime > 0) {
            // WARNING PHASE: vòng tròn nhấp nháy tại vị trí sẽ đập
            float blink = (sinf(boss->slamWarningTime * 12.0f) + 1.0f) * 0.5f;
            float warningProgress = 1.0f - (boss->slamWarningTime / SLAM_WARNING_TIME);
            float radius = 50.0f + warningProgress * 250.0f;  // Vòng tròn mở rộng dần
            
            // Vòng tròn warning đỏ
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, radius,
                (Color){255, 50, 0, (unsigned char)(blink * 200)});
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, radius * 0.6f,
                (Color){255, 100, 0, (unsigned char)(blink * 150)});
            
            // Dấu X tại center
            float crossSize = 20.0f + warningProgress * 15.0f;
            DrawLineEx(
                (Vector2){boss->slamPos.x - crossSize, boss->slamPos.y - crossSize},
                (Vector2){boss->slamPos.x + crossSize, boss->slamPos.y + crossSize},
                3.0f, (Color){255, 255, 0, (unsigned char)(blink * 255)});
            DrawLineEx(
                (Vector2){boss->slamPos.x + crossSize, boss->slamPos.y - crossSize},
                (Vector2){boss->slamPos.x - crossSize, boss->slamPos.y + crossSize},
                3.0f, (Color){255, 255, 0, (unsigned char)(blink * 255)});
            
            // Text "!"
            DrawText("!", (int)boss->slamPos.x - 8, (int)boss->slamPos.y - 50, 40,
                (Color){255, 255, 0, (unsigned char)(blink * 255)});
        } else {
            // ACTIVE PHASE: shockwave expanding
            float alpha = boss->slamTimer / SLAM_DURATION;
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, boss->shockwaveRadius, 
                (Color){255, 150, 0, (unsigned char)(alpha * 255)});
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, boss->shockwaveRadius * 0.7f, 
                (Color){255, 200, 50, (unsigned char)(alpha * 200)});
            // Impact point
            DrawCircleV(boss->slamPos, 15.0f * alpha, (Color){255, 100, 0, (unsigned char)(alpha * 200)});
        }
    }

    // --- Draw Claw Attack ---
    if (boss->clawActive) {
        // Xác định vùng X theo zone
        float zoneX = 20.0f, zoneW = 420.0f;
        if (boss->clawZone == CLAW_ZONE_MIDDLE) { zoneX = 440.0f; zoneW = 400.0f; }
        else if (boss->clawZone == CLAW_ZONE_RIGHT) { zoneX = 840.0f; zoneW = 420.0f; }
        
        if (boss->clawWarningTime > 0) {
            // Warning: vùng nhấp nháy đỏ/vàng
            float blink = (sinf(boss->clawWarningTime * 12.0f) + 1.0f) * 0.5f;
            DrawRectangle((int)zoneX, 0, (int)zoneW, 720,
                (Color){255, 50, 0, (unsigned char)(blink * 60)});
            // Claw marks preview
            for (int c = 0; c < 3; c++) {
                float cx = zoneX + zoneW * 0.2f + c * zoneW * 0.3f;
                DrawLineEx((Vector2){cx, 100}, (Vector2){cx - 30, 600}, 4.0f,
                    (Color){255, 100, 0, (unsigned char)(blink * 120)});
            }
            DrawText("!", (int)(zoneX + zoneW/2 - 10), 300, 60, 
                (Color){255, 255, 0, (unsigned char)(blink * 255)});
        } else if (boss->clawDuration > 0) {
            // Active: vết cào mạnh
            float intensity = boss->clawDuration / CLAW_DURATION;
            DrawRectangle((int)zoneX, 0, (int)zoneW, 720,
                (Color){255, 0, 0, (unsigned char)(intensity * 100)});
            // Claw slash lines
            for (int c = 0; c < 4; c++) {
                float cx = zoneX + zoneW * 0.15f + c * zoneW * 0.25f;
                float offset = (1.0f - intensity) * 50.0f;
                DrawLineEx((Vector2){cx + offset, 50}, (Vector2){cx - 40 + offset, 650}, 8.0f,
                    (Color){255, 200, 200, (unsigned char)(intensity * 230)});
                DrawLineEx((Vector2){cx + offset, 50}, (Vector2){cx - 40 + offset, 650}, 3.0f,
                    (Color){255, 255, 255, (unsigned char)(intensity * 200)});
            }
        }
    }

    // --- Draw Atom Bomb Visual (overlay khi triggered) ---
    if (boss->atomTriggered) {
        float t = boss->atomTimer;
        // Whitewash flash full screen trong 0.5s đầu
        if (t < 0.5f) {
            float wAlpha = 1.0f - (t / 0.5f);
            DrawRectangle(-2000, -2000, 5000, 5000,
                (Color){255, 255, 255, (unsigned char)(wAlpha * 255)});
        }
        // Mushroom cloud explosion (Explosion.png scale to fullscreen)
        if (explosionTex.id > 0 && t < 4.0f) {
            int expFrame = ((int)(t * 6.0f)) % EXPLOSION_FRAMES;
            Rectangle expSrc = { (float)(expFrame * EXPLOSION_FW), 0, (float)EXPLOSION_FW, (float)EXPLOSION_FH };
            float scale = 8.0f + t * 3.0f;
            Rectangle expDst = {
                640.0f - scale * EXPLOSION_FW / 2.0f,
                360.0f - scale * EXPLOSION_FH / 2.0f,
                scale * EXPLOSION_FW,
                scale * EXPLOSION_FH
            };
            DrawTexturePro(explosionTex, expSrc, expDst, (Vector2){0,0}, 0,
                (Color){255, 200, 100, 255});
        }
        // Trash talk text giữa màn
        DrawText("ATOMIC ANNIHILATION", 640 - MeasureText("ATOMIC ANNIHILATION", 48)/2,
            300, 48, (Color){255, 50, 50, 255});
        DrawText("MAY GA VCL!", 640 - MeasureText("MAY GA VCL!", 32)/2,
            370, 32, (Color){255, 255, 100, 255});
    }

    // --- Draw Fight Timer (countdown đến atom) ---
    if (boss->state == BOSS_FIGHTING && boss->fightTimer < ATOM_TRIGGER_TIME) {
        float remaining = ATOM_TRIGGER_TIME - boss->fightTimer;
        int mins = (int)(remaining / 60);
        int secs = (int)remaining % 60;
        char timerBuf[16];
        snprintf(timerBuf, sizeof(timerBuf), "%02d:%02d", mins, secs);
        Color timerColor = (remaining < 30.0f) ? (Color){255, 50, 50, 255} : (Color){255, 255, 100, 200};
        DrawText(timerBuf, 1280/2 - 30, 90, 24, timerColor);
    }

    // --- Draw Hazard Warnings & Obstacles ---
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardWarningTime[i] > 0 && !boss->hazardActive[i]) {
            // Warning: blinking indicator
            float blink = sinf(boss->hazardWarningTime[i] * 10.0f);
            if (blink > 0) {
                DrawRectangle((int)boss->hazardPositions[i].x - 20, 
                    (int)boss->hazardPositions[i].y - 60, 40, 60,
                    (Color){255, 255, 0, 100});
                DrawText("!", (int)boss->hazardPositions[i].x - 5, 
                    (int)boss->hazardPositions[i].y - 55, 30, RED);
            }
        } else if (boss->hazardActive[i]) {
            // Active hazard: dangerous pillar
            DrawRectangle((int)boss->hazardPositions[i].x - 15, 
                (int)boss->hazardPositions[i].y - 80, 30, 80,
                (Color){255, 50, 50, 200});
            // Electric effect
            float spark = sinf(boss->hazardWarningTime[i] * 20.0f);
            DrawRectangle((int)boss->hazardPositions[i].x - 20, 
                (int)boss->hazardPositions[i].y - 80, 40, 5,
                (Color){255, 255, (unsigned char)(128 + spark * 127), 200});
        }
    }
}

void BossTakeDamage(Boss *boss, int damage) {
    boss->hp -= damage;
    if (boss->hp <= 0) {
        boss->hp = 0;
        boss->state = BOSS_DYING;
    }
    boss->shakeTimer = 0.3f;
    boss->shakeIntensity = 30.0f;
    // Trash talk khi bị đánh (50% chance)
    TriggerTaunt(boss, TAUNTS_HIT, 4, 50);
}

bool CheckPlayerInShockwave(Vector2 playerPos, Vector2 slamPos, float radius) {
    float dx = playerPos.x - slamPos.x;
    float dy = playerPos.y - slamPos.y;
    float dist = sqrtf(dx*dx + dy*dy);
    // Player bị damage nếu trong vùng shockwave (ring)
    return (dist < radius && dist > radius - 40.0f);
}

bool CheckPlayerInLaser(Vector2 playerPos, Vector2 laserStart, Vector2 laserEnd, float width) {
    // Tính khoảng cách từ player đến đường laser
    Vector2 d = { laserEnd.x - laserStart.x, laserEnd.y - laserStart.y };
    float len = sqrtf(d.x*d.x + d.y*d.y);
    if (len < 1.0f) return false;
    
    d.x /= len; d.y /= len;
    Vector2 toPlayer = { playerPos.x - laserStart.x, playerPos.y - laserStart.y };
    float proj = toPlayer.x * d.x + toPlayer.y * d.y;
    
    if (proj < 0 || proj > len) return false;
    
    float perpDist = fabsf(toPlayer.x * (-d.y) + toPlayer.y * d.x);
    return perpDist < width / 2.0f;
}

bool CheckPlayerInClawZone(Vector2 playerPos, ClawZone zone) {
    // Zones: LEFT (20-440), MIDDLE (440-840), RIGHT (840-1260)
    switch (zone) {
        case CLAW_ZONE_LEFT:
            return playerPos.x >= 20.0f && playerPos.x < 440.0f;
        case CLAW_ZONE_MIDDLE:
            return playerPos.x >= 440.0f && playerPos.x < 840.0f;
        case CLAW_ZONE_RIGHT:
            return playerPos.x >= 840.0f && playerPos.x <= 1260.0f;
    }
    return false;
}
