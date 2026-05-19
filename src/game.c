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

Texture2D GetCachedTexture(const char* path) {
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
    // Chuyển ngược thành xuôi
    char* p = path;
    while (*p) {
        if (*p == '\\') *p = '/';
        p++;
    }
    
    // Xóa "../" ở đầu nếu có
    if (strncmp(path, "../", 3) == 0) {
        memmove(path, path + 3, strlen(path + 3) + 1);
    }

    // Xóa "Theforest/" hoặc "Theforest-main/" nếu có trong path để đưa về root
    char* key = strstr(path, "Theforest/");
    if (key) {
        memmove(path, key + 10, strlen(key + 10) + 1);
    } else {
        key = strstr(path, "Theforest-main/");
        if (key) memmove(path, key + 15, strlen(key + 15) + 1);
    }
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
void UpdatePlayer(Player *player, GameMap *map, float deltaTime) {
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

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !player->isAttacking) {
        player->isAttacking = true;
        player->currentFrame = 0;
        player->frameTimer = 0.0f;
        player->freezeTimer = 0.15f;
    }

    if (player->isAttacking) animSpeed = 0.07f;
    
    if (IsKeyPressed(KEY_SPACE) && !player->isJumping) {
        player->velocity.y = jumpForce;
        player->isJumping = true;
    }

    // --- Physics & Collision ---
    player->velocity.y += gravity * deltaTime;
    
    float feetY = player->position.y + 16.0f; 
    float nextFeetY = feetY + player->velocity.y * deltaTime;
    float nextX = player->position.x;

    // Tính toán di chuyển ngang tiềm năng
    float horizontalMove = currentSpeed;
    if (player->isJumping) horizontalMove *= 1.2f;
    if (player->freezeTimer > 0) horizontalMove = 0;

    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) nextX += horizontalMove * deltaTime;
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) nextX -= horizontalMove * deltaTime;

    bool hitGround = false;
    float groundLevel = 100000.0f;
    float playerRadius = 8.0f; // Adjusted for 32px size
    float playerHeight = 28.0f; // Adjusted for 32px size
    float stepUpHeight = 16.0f; 

    for (int i = 0; i < map->layerCount; i++) {
        TMJLayer *layer = &map->layers[i];
        if (!layer->visible || strcmp(layer->type, "objectgroup") != 0) continue;

        for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (obj->texture.id != 0) continue; 

            float objX = obj->x + layer->offsetx;
            float objY = obj->y + layer->offsety + 8.0f; // Lowered by half a block (8px)

            if (TextIsEqual(layer->name, "Portal") || TextIsEqual(layer->name, "portal")) {
                float objLeft = objX;
                float objRight = objX + obj->width;
                float objTop = objY;
                float objBottom = objY + obj->height;
                float headY = feetY - playerHeight;

                if (nextX + playerRadius > objLeft && nextX - playerRadius < objRight &&
                    feetY > objTop && headY < objBottom) {
                    // Trigger map change
                    player->loadNextMap = true;
                }
                continue;
            }

            if (obj->polygonCount > 0) {
                // --- Polygon / Slope Collision ---
                for (int k = 0; k < obj->polygonCount; k++) {
                    Point p1 = obj->polygon[k];
                    Point p2 = obj->polygon[(k + 1) % obj->polygonCount];
                    float x1 = p1.x + objX;
                    float y1 = p1.y + objY;
                    float x2 = p2.x + objX;
                    float y2 = p2.y + objY;
                    float minX = (x1 < x2) ? x1 : x2;
                    float maxX = (x1 > x2) ? x1 : x2;

                    // Use a small buffer (1.0f) to prevent getting stuck at edges
                    if (nextX >= minX - 1.0f && nextX <= maxX + 1.0f) {
                        float slopeY = y1 + (y2 - y1) * (nextX - x1) / (x2 - x1);
                        if (player->velocity.y >= 0 && feetY <= slopeY + 8.0f && nextFeetY >= slopeY - 2.0f) {
                            hitGround = true;
                            if (slopeY < groundLevel) groundLevel = slopeY;
                        }
                    }
                }
            } else {
                // --- Rectangle Collision (Full Solid with Step-Up) ---
                float objLeft = objX;
                float objRight = objX + obj->width;
                float objTop = objY;
                float objBottom = objY + obj->height;

                float headY = feetY - playerHeight;
                float nextHeadY = nextFeetY - playerHeight;

                bool isLadder = (strcmp(obj->name, "ladder") == 0) || (strcmp(layer->name, "ladder") == 0);

                // 1. Horizontal Collision (Walls)
                if (!isLadder && feetY > objTop + 2.0f && headY < objBottom - 2.0f) {
                    if (nextX + playerRadius > objLeft && nextX - playerRadius < objRight) {
                        // Check if we can "step up" this block
                        if (feetY > objTop && feetY <= objTop + stepUpHeight) {
                            // Can step up! Don't block X, but we'll snap to ground later
                        } else {
                            nextX = player->position.x;
                        }
                    }
                }

                // 2. Vertical Collision (Floor & Ceiling)
                if (nextX + playerRadius > objLeft && nextX - playerRadius < objRight) {
                    // Floor detection
                    if (player->velocity.y >= 0 && feetY <= objTop + 8.0f && nextFeetY >= objTop) {
                        hitGround = true;
                        if (objTop < groundLevel) groundLevel = objTop;
                    }
                    // Ceiling detection
                    else if (!isLadder && player->velocity.y < 0 && headY >= objBottom - 5.0f && nextHeadY <= objBottom) {
                        player->velocity.y = 0;
                        nextFeetY = objBottom + playerHeight + 0.1f;
                    }
                }
            }
        }
    }

    // Update Position
    player->position.x = nextX;
    player->isRunning = false;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { player->facingRight = true; player->isRunning = !player->isJumping; }
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) { player->facingRight = false; player->isRunning = !player->isJumping; }

    if (hitGround) {
        player->position.y = groundLevel - 16.0f;
        player->velocity.y = 0;
        player->isJumping = false;
    } else {
        player->position.y = nextFeetY - 16.0f;
    }

    // --- Giới hạn biên dưới bản đồ ---
    float mapBottom = (float)map->mapHeight * map->tileHeight - 64.0f;
    if (player->position.y > mapBottom) {
        player->position.y = mapBottom;
        player->velocity.y = 0;
        player->isJumping = false;
    }

    player->frameTimer += deltaTime;
    if (player->frameTimer >= animSpeed) { 
        player->frameTimer = 0.0f; 
        player->currentFrame++; 
        if (player->isAttacking && player->currentFrame >= 8) {
            player->isAttacking = false;
            player->currentFrame = 0;
        }
        else if (player->isJumping && player->currentFrame > 4) player->currentFrame = 4;
    }
}
void DrawPlayer(Player *player, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, int frameW, int frameH, float scale) {
    float yOffset = 16.0f;
    float spacing = 16.0f; // Restored the 16px spacing between frames!

    Texture2D currentTex = idle;
    int maxFrames = 8; // Idle has 8 frames
    int frameIdx = 0;

    if (player->isAttacking) {
        currentTex = attack;
        maxFrames = 8;
        frameIdx = player->currentFrame % maxFrames;
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
            else { currentTex = walk; maxFrames = 8; } // Previously walk also had 8 frames
        }
        frameIdx = player->currentFrame % maxFrames;
    }

    float frameX = (float)frameIdx * (frameW + spacing);
    Rectangle source = { frameX, 0, (float)frameW, (float)frameH };
    if (player->facingRight) source.width = -source.width;
    Rectangle dest = { player->position.x, player->position.y + yOffset, (float)frameW * scale, (float)frameH * scale };
    DrawTexturePro(currentTex, source, dest, (Vector2){(float)frameW * scale / 2.0f, (float)frameH * scale}, 0.0f, WHITE);
}

// --- Layer Helpers ---
static int CountLayersRecursive(cJSON* layersJson) {
    int total = 0;
    int count = cJSON_GetArraySize(layersJson);
    for (int i = 0; i < count; i++) {
        cJSON* layerJson = cJSON_GetArrayItem(layersJson, i);
        cJSON* subLayers = cJSON_GetObjectItem(layerJson, "layers");
        if (subLayers) total += CountLayersRecursive(subLayers);
        else total++;
    }
    return total;
}

static void ProcessLayersRecursive(cJSON* layersJson, GameMap* map, int* currentLayerIdx, float parentOffsetX, float parentOffsetY) {
    int count = cJSON_GetArraySize(layersJson);
    for (int i = 0; i < count; i++) {
        cJSON* layerJson = cJSON_GetArrayItem(layersJson, i);
        cJSON* subLayers = cJSON_GetObjectItem(layerJson, "layers");

        float ox = 0, oy = 0;
        cJSON* oxItem = cJSON_GetObjectItem(layerJson, "offsetx");
        if (oxItem) ox = (float)oxItem->valuedouble;
        cJSON* oyItem = cJSON_GetObjectItem(layerJson, "offsety");
        if (oyItem) oy = (float)oyItem->valuedouble;

        if (subLayers) {
            ProcessLayersRecursive(subLayers, map, currentLayerIdx, parentOffsetX + ox, parentOffsetY + oy);
        } else {
            TMJLayer* layer = &map->layers[*currentLayerIdx];
            cJSON* nameItem = cJSON_GetObjectItem(layerJson, "name");
            cJSON* typeItem = cJSON_GetObjectItem(layerJson, "type");
            
            strncpy(layer->name, nameItem ? nameItem->valuestring : "", 63);
            strncpy(layer->type, typeItem ? typeItem->valuestring : "", 31);
            
            layer->visible = cJSON_IsTrue(cJSON_GetObjectItem(layerJson, "visible"));
            cJSON* opItem = cJSON_GetObjectItem(layerJson, "opacity");
            layer->opacity = opItem ? (float)opItem->valuedouble : 1.0f;
            layer->offsetx = parentOffsetX + ox;
            layer->offsety = parentOffsetY + oy;

            if (strcmp(layer->type, "objectgroup") == 0) {
                cJSON* objects = cJSON_GetObjectItem(layerJson, "objects");
                layer->objectCount = cJSON_GetArraySize(objects);
                layer->objects = (TMJObject*)calloc(layer->objectCount, sizeof(TMJObject));
                for (int j = 0; j < layer->objectCount; j++) {
                    cJSON* objJson = cJSON_GetArrayItem(objects, j);
                    TMJObject* obj = &layer->objects[j];
                    cJSON* nameItem = cJSON_GetObjectItem(objJson, "name");
                    strncpy(obj->name, nameItem && nameItem->valuestring ? nameItem->valuestring : "", 63);
                    obj->x = (float)cJSON_GetObjectItem(objJson, "x")->valuedouble;
                    obj->y = (float)cJSON_GetObjectItem(objJson, "y")->valuedouble;
                    obj->width = (float)cJSON_GetObjectItem(objJson, "width")->valuedouble;
                    obj->height = (float)cJSON_GetObjectItem(objJson, "height")->valuedouble;
                    obj->rotation = (float)cJSON_GetObjectItem(objJson, "rotation")->valuedouble;
                    obj->visible = cJSON_IsTrue(cJSON_GetObjectItem(objJson, "visible"));
                    cJSON* objOp = cJSON_GetObjectItem(objJson, "opacity");
                    obj->opacity = objOp ? (float)objOp->valuedouble : 1.0f;
                    
                    cJSON* polygon = cJSON_GetObjectItem(objJson, "polygon");
                    if (polygon) {
                        obj->polygonCount = cJSON_GetArraySize(polygon);
                        obj->polygon = (Point*)malloc(obj->polygonCount * sizeof(Point));
                        for (int k = 0; k < obj->polygonCount; k++) {
                            cJSON* p = cJSON_GetArrayItem(polygon, k);
                            obj->polygon[k].x = (float)cJSON_GetObjectItem(p, "x")->valuedouble;
                            obj->polygon[k].y = (float)cJSON_GetObjectItem(p, "y")->valuedouble;
                        }
                    }

                    cJSON* gidItem = cJSON_GetObjectItem(objJson, "gid");
                    if (gidItem) {
                        unsigned int rawGid = (unsigned int)gidItem->valuedouble;
                        obj->flipX = (rawGid & 0x80000000);
                        obj->flipY = (rawGid & 0x40000000);
                        int gid = rawGid & 0x1FFFFFFF;
                        for (int k = map->tilesetCount - 1; k >= 0; k--) {
                            if (gid >= map->tilesets[k].firstgid) {
                                if (map->tilesets[k].texture.id != 0) obj->texture = map->tilesets[k].texture;
                                break;
                            }
                        }
                    }
                }
            } else if (strcmp(layer->type, "tilelayer") == 0) {
                layer->width = cJSON_GetObjectItem(layerJson, "width")->valueint;
                layer->height = cJSON_GetObjectItem(layerJson, "height")->valueint;
                cJSON* data = cJSON_GetObjectItem(layerJson, "data");
                int dataSize = cJSON_GetArraySize(data);
                layer->data = (int*)malloc(dataSize * sizeof(int));
                for (int j = 0; j < dataSize; j++) {
                    layer->data[j] = cJSON_GetArrayItem(data, j)->valueint;
                }
            }
            (*currentLayerIdx)++;
        }
    }
}

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

    // --- Load Tilesets ---
    cJSON* tilesetsJson = cJSON_GetObjectItem(root, "tilesets");
    map.tilesetCount = cJSON_GetArraySize(tilesetsJson);
    map.tilesets = (TMJTileset*)calloc(map.tilesetCount, sizeof(TMJTileset));
    for (int i = 0; i < map.tilesetCount; i++) {
        cJSON* tsJson = cJSON_GetArrayItem(tilesetsJson, i);
        map.tilesets[i].firstgid = cJSON_GetObjectItem(tsJson, "firstgid")->valueint;
        
        cJSON* twItem = cJSON_GetObjectItem(tsJson, "tilewidth");
        map.tilesets[i].tileWidth = twItem ? twItem->valueint : map.tileWidth;
        cJSON* thItem = cJSON_GetObjectItem(tsJson, "tileheight");
        map.tilesets[i].tileHeight = thItem ? thItem->valueint : map.tileHeight;
        cJSON* spItem = cJSON_GetObjectItem(tsJson, "spacing");
        map.tilesets[i].spacing = spItem ? spItem->valueint : 0;
        cJSON* mgItem = cJSON_GetObjectItem(tsJson, "margin");
        map.tilesets[i].margin = mgItem ? mgItem->valueint : 0;
        cJSON* colItem = cJSON_GetObjectItem(tsJson, "columns");
        map.tilesets[i].columns = colItem ? colItem->valueint : 0;

        cJSON* imgItem = cJSON_GetObjectItem(tsJson, "image");
        if (!imgItem) {
            // Handle tilesets with tiles containing images (like the background tileset)
            cJSON* tiles = cJSON_GetObjectItem(tsJson, "tiles");
            if (tiles && cJSON_GetArraySize(tiles) > 0) {
                imgItem = cJSON_GetObjectItem(cJSON_GetArrayItem(tiles, 0), "image");
            }
        }
        if (imgItem) {
            strncpy(map.tilesets[i].imagePath, imgItem->valuestring, 255);
            char fullPath[256];
            if (strstr(map.tilesets[i].imagePath, "/") || strstr(map.tilesets[i].imagePath, "\\")) {
                strncpy(fullPath, map.tilesets[i].imagePath, 255);
            } else {
                snprintf(fullPath, 255, "assets/%s", map.tilesets[i].imagePath);
            }
            FixTiledPath(fullPath);
            map.tilesets[i].texture = GetCachedTexture(fullPath);
        }
    }

    // --- Load Layers (Recursive for Groups) ---
    cJSON* layersJson = cJSON_GetObjectItem(root, "layers");
    map.layerCount = CountLayersRecursive(layersJson);
    map.layers = (TMJLayer*)calloc(map.layerCount, sizeof(TMJLayer));
    int currentIdx = 0;
    ProcessLayersRecursive(layersJson, &map, &currentIdx, 0.0f, 0.0f);

    cJSON_Delete(root); free(jsonStr); return map;
}

void UnloadMapData(GameMap *map) {
    ClearCache(); 
    for (int i = 0; i < map->layerCount; i++) {
        if (map->layers[i].objects) {
            for (int j = 0; j < map->layers[i].objectCount; j++) {
                if (map->layers[i].objects[j].polygon) free(map->layers[i].objects[j].polygon);
            }
            free(map->layers[i].objects);
        }
        if (map->layers[i].data) free(map->layers[i].data);
    }
    if (map->layers) free(map->layers);
    if (map->tilesets) free(map->tilesets);
}
