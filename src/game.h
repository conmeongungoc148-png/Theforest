#ifndef GAME_H
#define GAME_H

#include "raylib.h"

// --- Tiled Structures ---
typedef struct {
    float x, y, width, height, rotation;
    bool visible, flipX, flipY;
    Texture2D texture; // Mỗi vật thể có texture riêng (hoặc dùng chung)
    float opacity;
} TMJObject;

typedef struct {
    char name[64];
    char type[32];
    bool visible;
    float opacity;
    TMJObject *objects;
    int objectCount;
} TMJLayer;

typedef struct {
    int mapWidth, mapHeight, tileWidth, tileHeight;
    TMJLayer *layers;
    int layerCount;
} GameMap;

// --- Player Structure ---
typedef struct {
    Vector2 position;
    Vector2 velocity; // Để xử lý trọng lực và quán tính
    Vector2 speed;
    int currentFrame;
    float frameTimer;
    float groundY;    // Mốc mặt đất để va chạm
    bool isRunning;
    bool isSprinting;
    bool isJumping;
    bool isAttacking;
    bool facingRight;
    float freezeTimer; // Thời gian khựng khi tấn công
} Player;

// --- API ---
GameMap LoadMapData(const char *filename);
void UnloadMapData(GameMap *map);

void InitPlayer(Player *player, Vector2 pos);
void UpdatePlayer(Player *player, float deltaTime);
void DrawPlayer(Player *player, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, int frameW, int frameH, float scale);

#endif
