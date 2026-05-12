#include "game.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Texture Cache ---
#define MAX_CACHE 128
static struct {
    char path[256];
    Texture2D texture;
} TextureCache[MAX_CACHE];
static int CacheCount = 0;

static Texture2D GetCachedTexture(const char* path) {
    for (int i = 0; i < CacheCount; i++) {
        if (strcmp(TextureCache[i].path, path) == 0) return TextureCache[i].texture;
    }
    if (CacheCount < MAX_CACHE) {
        Texture2D tex = LoadTexture(path);
        if (tex.id != 0) {
            strncpy(TextureCache[CacheCount].path, path, 255);
            TextureCache[CacheCount].texture = tex;
            CacheCount++;
        }
        return tex;
    }
    return LoadTexture(path);
}

static void ClearCache() {
    for (int i = 0; i < CacheCount; i++) UnloadTexture(TextureCache[i].texture);
    CacheCount = 0;
}

// --- Helpers ---
static char* ReadFileText(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* data = (char*)malloc(size + 1);
    fread(data, 1, size, file);
    data[size] = '\0';
    fclose(file);
    return data;
}

static void FixTiledPath(char* path) {
    char* p = path;
    while ((p = strstr(p, "\\/"))) memmove(p, p + 1, strlen(p));
    char* key = strstr(path, "Theforest/");
    if (key) memmove(path, key + 10, strlen(key + 10) + 1);
}

void InitPlayer(Player *player, Vector2 pos) {
    player->position = pos;
    player->velocity = (Vector2){0, 0};
    player->speed = (Vector2){250.0f, 0.0f};
    player->groundY = pos.y;
    player->facingRight = true;
    player->isJumping = false;
    player->isAttacking = false;
    player->freezeTimer = 0.0f;
    player->currentFrame = 0;
    player->frameTimer = 0.0f;
}
void UpdatePlayer(Player *player, float deltaTime) {
    // Tinh chỉnh vật lý để chạm đất vào khoảng 0.45s
    const float gravity = 4000.0f;
    const float jumpForce = -900.0f;
    
    player->isRunning = false; 
    player->isSprinting = false;
    float currentSpeed = player->speed.x;
    float animSpeed = 0.1f; 

    if (player->freezeTimer > 0) player->freezeTimer -= deltaTime;

    if (IsKeyDown(KEY_LEFT_SHIFT)) { 
        currentSpeed *= 1.8f; 
        player->isSprinting = true; 
        animSpeed = 0.07f; 
    }

    // Tấn công (Chuột trái)
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !player->isAttacking) {
        player->isAttacking = true;
        player->currentFrame = 0;
        player->frameTimer = 0.0f;
        player->freezeTimer = 0.15f; // Khựng 0.15s
    }

    if (player->isAttacking) {
        animSpeed = 0.07f; // Nhanh hơn: 70ms mỗi frame
    }
    
    // Nhảy (Cho phép nhảy khi đang tấn công hoặc ngược lại)
    if (IsKeyPressed(KEY_SPACE) && !player->isJumping) {
        player->velocity.y = jumpForce;
        player->isJumping = true;
        if (!player->isAttacking) {
            player->currentFrame = 0;
            player->frameTimer = 0.0f;
        }
    }

    // Trọng lực và Hoạt ảnh nhảy
    if (player->isJumping) {
        player->velocity.y += gravity * deltaTime;
        player->position.y += player->velocity.y * deltaTime;
        
        // Va chạm đất
        if (player->position.y >= player->groundY) {
            player->position.y = player->groundY;
            player->velocity.y = 0;
            player->isJumping = false;
        }
        if (!player->isAttacking) animSpeed = 0.1f; 
    }

    // Di chuyển ngang
    float horizontalMove = currentSpeed;
    if (player->isJumping) horizontalMove *= 1.2f;
    if (player->freezeTimer > 0) horizontalMove = 0; // Khựng lại

    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { 
        player->position.x += horizontalMove * deltaTime; 
        if (!player->isJumping && !player->isAttacking) player->isRunning = true; 
        player->facingRight = true; 
    }
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) { 
        player->position.x -= horizontalMove * deltaTime; 
        if (!player->isJumping && !player->isAttacking) player->isRunning = true; 
        player->facingRight = false; 
    }

    player->frameTimer += deltaTime;
    if (player->frameTimer >= animSpeed) { 
        player->frameTimer = 0.0f; 
        player->currentFrame++; 
        
        if (player->isAttacking) {
            if (player->currentFrame >= 8) {
                player->isAttacking = false;
                player->currentFrame = 0;
            }
        }
        else if (player->isJumping) {
            if (player->currentFrame > 4) player->currentFrame = 4;
        }
    }
}
void DrawPlayer(Player *player, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, int frameW, int frameH, float scale) {
    Texture2D currentTex = idle; 
    int maxFrames = 8; 
    float spacing = 16.0f;
    int frameIdx = 0;

    if (player->isAttacking) {
        currentTex = attack;
        maxFrames = 8;
        frameIdx = player->currentFrame % 8;
    }
    else if (player->isJumping) {
        currentTex = jump;
        maxFrames = 5; 
        int animFrame = player->currentFrame % maxFrames;
        if (animFrame == 0) frameIdx = 0;
        else if (animFrame >= 1 && animFrame <= 3) frameIdx = 1;
        else frameIdx = 2;
    }
    else {
        if (player->isRunning) {
            if (player->isSprinting) { currentTex = run; maxFrames = 8; }
            else { currentTex = walk; maxFrames = 12; }
        }
        frameIdx = player->currentFrame % maxFrames;
    }

    float frameX = (float)frameIdx * (frameW + spacing);
    float yOffset = 16.0f;
    Rectangle source = { frameX, 0, (float)frameW, (float)frameH };
    if (player->facingRight) source.width = -source.width;
    Rectangle dest = { player->position.x, player->position.y + yOffset, (float)frameW * scale, (float)frameH * scale };
    DrawTexturePro(currentTex, source, dest, (Vector2){(float)frameW * scale / 2.0f, (float)frameH * scale}, 0.0f, WHITE);
}

// --- Robust Map Loader ---
GameMap LoadMapData(const char *filename) {
    GameMap map = {0};
    char* jsonStr = ReadFileText(filename);
    if (!jsonStr) return map;
    cJSON* root = cJSON_Parse(jsonStr);
    if (!root) { free(jsonStr); return map; }

    map.mapWidth = cJSON_GetObjectItem(root, "width")->valueint;
    map.mapHeight = cJSON_GetObjectItem(root, "height")->valueint;
    map.tileWidth = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    map.tileHeight = cJSON_GetObjectItem(root, "tileheight")->valueint;

    cJSON* layers = cJSON_GetObjectItem(root, "layers");
    map.layerCount = cJSON_GetArraySize(layers);
    map.layers = (TMJLayer*)calloc(map.layerCount, sizeof(TMJLayer));

    cJSON* tilesets = cJSON_GetObjectItem(root, "tilesets");

    for (int i = 0; i < map.layerCount; i++) {
        cJSON* layerJson = cJSON_GetArrayItem(layers, i);
        TMJLayer* layer = &map.layers[i];
        strncpy(layer->name, cJSON_GetObjectItem(layerJson, "name")->valuestring, 63);
        strncpy(layer->type, cJSON_GetObjectItem(layerJson, "type")->valuestring, 31);
        layer->visible = cJSON_IsTrue(cJSON_GetObjectItem(layerJson, "visible"));
        layer->opacity = (float)cJSON_GetObjectItem(layerJson, "opacity")->valuedouble;

        if (strcmp(layer->type, "objectgroup") == 0) {
            cJSON* objects = cJSON_GetObjectItem(layerJson, "objects");
            layer->objectCount = cJSON_GetArraySize(objects);
            layer->objects = (TMJObject*)calloc(layer->objectCount, sizeof(TMJObject));
            
            for (int j = 0; j < layer->objectCount; j++) {
                cJSON* objJson = cJSON_GetArrayItem(objects, j);
                TMJObject* obj = &layer->objects[j];
                obj->x = (float)cJSON_GetObjectItem(objJson, "x")->valuedouble;
                obj->y = (float)cJSON_GetObjectItem(objJson, "y")->valuedouble;
                obj->width = (float)cJSON_GetObjectItem(objJson, "width")->valuedouble;
                obj->height = (float)cJSON_GetObjectItem(objJson, "height")->valuedouble;
                obj->rotation = (float)cJSON_GetObjectItem(objJson, "rotation")->valuedouble;
                obj->visible = cJSON_IsTrue(cJSON_GetObjectItem(objJson, "visible"));
                obj->opacity = (float)cJSON_GetObjectItem(objJson, "opacity")->valuedouble;
                
                cJSON* gidItem = cJSON_GetObjectItem(objJson, "gid");
                if (gidItem) {
                    unsigned int rawGid = (unsigned int)gidItem->valuedouble;
                    obj->flipX = (rawGid & 0x80000000);
                    obj->flipY = (rawGid & 0x40000000);
                    int gid = rawGid & 0x1FFFFFFF;

                    // Duyệt tìm GID trong TẤT CẢ tileset (không dùng tilecount giới hạn)
                    for (int k = 0; k < cJSON_GetArraySize(tilesets); k++) {
                        cJSON* ts = cJSON_GetArrayItem(tilesets, k);
                        int firstgid = cJSON_GetObjectItem(ts, "firstgid")->valueint;
                        cJSON* tiles = cJSON_GetObjectItem(ts, "tiles");
                        
                        if (tiles && gid >= firstgid) {
                            for (int l = 0; l < cJSON_GetArraySize(tiles); l++) {
                                cJSON* tile = cJSON_GetArrayItem(tiles, l);
                                if (cJSON_GetObjectItem(tile, "id")->valueint == (gid - firstgid)) {
                                    char imgPath[256];
                                    strncpy(imgPath, cJSON_GetObjectItem(tile, "image")->valuestring, 255);
                                    FixTiledPath(imgPath);
                                    obj->texture = GetCachedTexture(imgPath); // Sử dụng Cache
                                    goto next_obj;
                                }
                            }
                        }
                    }
                }
                next_obj:;
            }
        }
    }
    cJSON_Delete(root); free(jsonStr); return map;
}

void UnloadMapData(GameMap *map) {
    ClearCache(); // Giải phóng toàn bộ texture trong cache
    for (int i = 0; i < map->layerCount; i++) {
        if (map->layers[i].objects) free(map->layers[i].objects);
    }
    if (map->layers) free(map->layers);
}
