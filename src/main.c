#include "camera.h"
#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define VIRTUAL_HEIGHT 760
#define VIRTUAL_WIDTH 1351

Vector2 FindSpawnPoint(GameMap *map, Vector2 defaultPos) {
    for (int i = 0; i < map->layerCount; i++) {
        if (strcmp(map->layers[i].type, "objectgroup") == 0) {
            for (int j = 0; j < map->layers[i].objectCount; j++) {
                TMJObject *obj = &map->layers[i].objects[j];
                if (TextIsEqual(obj->name, "spawn") || TextIsEqual(obj->name, "Spawn") ||
                    TextIsEqual(map->layers[i].name, "spawn") || TextIsEqual(map->layers[i].name, "Spawn")) {
                    float spawnX = obj->x + map->layers[i].offsetx;
                    float spawnY = obj->y + map->layers[i].offsety + obj->height - 64.0f;
                    TraceLog(LOG_INFO, "[SPAWN] Found spawn point at: %.2f, %.2f", spawnX, spawnY);
                    return (Vector2){spawnX, spawnY};
                }
            }
        }
    }
    TraceLog(LOG_WARNING, "[SPAWN] Spawn point NOT found! Using default: %.2f, %.2f", defaultPos.x, defaultPos.y);
    return defaultPos;
}

void GetMapBackgroundBounds(GameMap *map, float *minX, float *maxX, float *minY, float *maxY) {
    *minX = 0.0f;
    *maxX = (float)map->mapWidth * map->tileWidth;
    *minY = 0.0f;
    *maxY = (float)map->mapHeight * map->tileHeight;

    bool foundBackground = false;
    float bgMinX = 100000.0f;
    float bgMaxX = -100000.0f;
    float bgMinY = 100000.0f;
    float bgMaxY = -100000.0f;

    for (int i = 0; i < map->layerCount; i++) {
        TMJLayer *layer = &map->layers[i];
        if (!layer->visible) continue;

        char lowerName[64];
        strncpy(lowerName, layer->name, 63);
        lowerName[63] = '\0';
        for (int c = 0; lowerName[c]; c++) {
            if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') {
                lowerName[c] = lowerName[c] - 'A' + 'a';
            }
        }

        if (strstr(lowerName, "background") != NULL) {
            if (strcmp(layer->type, "objectgroup") == 0) {
                for (int j = 0; j < layer->objectCount; j++) {
                    TMJObject *obj = &layer->objects[j];
                    float objX = obj->x + layer->offsetx;
                    float objY = obj->y + layer->offsety;
                    
                    float top, bottom, left, right;
                    if (obj->texture.id != 0) {
                        top = objY - obj->height;
                        bottom = objY;
                    } else {
                        top = objY;
                        bottom = objY + obj->height;
                    }
                    left = objX;
                    right = objX + obj->width;

                    if (left < bgMinX) bgMinX = left;
                    if (right > bgMaxX) bgMaxX = right;
                    if (top < bgMinY) bgMinY = top;
                    if (bottom > bgMaxY) bgMaxY = bottom;
                    foundBackground = true;
                }
            }
        }
    }

    if (foundBackground) {
        if (bgMinX < 0.0f) bgMinX = 0.0f;
        if (bgMaxX > *maxX) bgMaxX = *maxX;
        if (bgMinY < 0.0f) bgMinY = 0.0f;
        if (bgMaxY > *maxY) bgMaxY = *maxY;

        *minX = bgMinX;
        *maxX = bgMaxX;
        *minY = bgMinY;
        *maxY = bgMaxY;
    }
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
             "The Forest - Full Screen Infinite Map");
  SetTargetFPS(60);

  RenderTexture2D target = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

  const char *mapList[] = {
      "assets/forestmap.tmj",
      "assets/nightcity.tmj",
      "assets/boss map 1v1.tmj"
  };
  // Fallback spawn nếu map không có spawn point object
  // Boss map: sàn chính y=419, nhân vật cao 64px → đứng tại y = 419 - 64 = 355
  const Vector2 mapSpawnDefaults[] = {
      {50.0f,  1168.0f},  // forestmap
      {50.0f,  1168.0f},  // nightcity
      {100.0f,  355.0f},  // boss map 1v1
  };
  int totalMaps = 3;
  int currentMapIndex = 0;

  GameMap gameMap = LoadMapData(mapList[currentMapIndex]);

  // Find spawn point from the map, or fallback to default
  Vector2 startPos = FindSpawnPoint(&gameMap, mapSpawnDefaults[currentMapIndex]);

  Player player = {0};
  InitPlayer(&player, startPos);

  Texture2D texIdle = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/IDLE.png");
  Texture2D texWalk = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/WALK.png");
  Texture2D texRun = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/RUN.png");
  Texture2D texJump = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/JUMP.png");
  Texture2D texAttack = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/ATTACK 1.png");
  Texture2D texRunJump = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/RUNNING JUMP.png");
  Texture2D texHurt = LoadTexture(
      "FREE_Cat 2D Pixel Art/FREE_Cat 2D Pixel Art/Sprites/HURT.png");

  MyCamera myCam = CameraNew(player.position.x, player.position.y,
                             VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  myCam.zoom = 1.42f;
  CameraSetSmoothDamped(&myCam, 10.0f);
  float bgMinX, bgMaxX, bgMinY, bgMaxY;
  GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
  CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.016f; // Cap dt to prevent physics glitches when loading a map takes time!
    UpdatePlayer(&player, &gameMap, dt);
    UpdateSkills(&player, dt);

    // Skill casting
    if (IsKeyPressed(KEY_Q)) {
      CastSkill(&player, "assets/skills/redfire.tmj");
    }
    if (IsKeyPressed(KEY_E)) {
      CastSkill(&player, "assets/skills/redclaw.tmj");
    }

    if (IsKeyPressed(KEY_R)) {
        player.loadNextMap = true;
    }

    if (player.loadNextMap) {
        currentMapIndex++;
        if (currentMapIndex >= totalMaps) {
            currentMapIndex = 0; // Loop back to the first map when you beat the last one!
        }

        UnloadMapData(&gameMap);
        gameMap = LoadMapData(mapList[currentMapIndex]);
        player.loadNextMap = false;
        player.position = FindSpawnPoint(&gameMap, mapSpawnDefaults[currentMapIndex]); // Teleport to spawn point
        
        // Reset player states completely
        player.velocity = (Vector2){0, 0};
        player.currentFrame = 0;
        player.isAttacking = false;
        player.isJumping = false;
        player.freezeTimer = 0.0f;

        // Snap camera instantly to new location
        CameraLookAt(&myCam, player.position);
        
        // Update bounds for camera just in case the new map has different dimensions
        float bgMinX, bgMaxX, bgMinY, bgMaxY;
        GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
        CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);
    }

    CameraUpdate(&myCam, player.position, dt);

    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode2D(myCam.rl);

    // Pass 1: Vẽ các layer background trước (tên chứa "background", "sky", "mountain")
    for (int i = 0; i < gameMap.layerCount; i++) {
      TMJLayer *layer = &gameMap.layers[i];
      if (!layer->visible) continue;
      char lname[64];
      strncpy(lname, layer->name, 63); lname[63] = '\0';
      for (int c = 0; lname[c]; c++) if (lname[c] >= 'A' && lname[c] <= 'Z') lname[c] += 32;
      if (strstr(lname, "background") == NULL && strstr(lname, "sky") == NULL && strstr(lname, "mountain") == NULL)
        continue;
      if (strcmp(layer->type, "objectgroup") == 0) {
        for (int j = 0; j < layer->objectCount; j++) {
          TMJObject *obj = &layer->objects[j];
          if (!obj->visible || obj->texture.id == 0) continue;
          Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
          if (obj->flipX) source.width = -source.width;
          if (obj->flipY) source.height = -source.height;
          Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
          Vector2 origin = {0, obj->height};
          DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                         Fade(WHITE, layer->opacity * obj->opacity));
        }
      }
    }

    // Pass 2: Vẽ tất cả layers còn lại theo thứ tự
    for (int i = 0; i < gameMap.layerCount; i++) {
      TMJLayer *layer = &gameMap.layers[i];
      if (!layer->visible) continue;

      // Tên layer lowercase để so sánh
      char lowerName[64];
      strncpy(lowerName, layer->name, 63); lowerName[63] = '\0';
      for (int c = 0; lowerName[c]; c++) if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') lowerName[c] += 32;

      // Skip background layers (đã vẽ ở pass 1)
      if (strstr(lowerName, "background") != NULL || strstr(lowerName, "sky") != NULL || strstr(lowerName, "mountain") != NULL)
        continue;

      // Skip layer "ground" - chỉ dùng cho collision
      if (strstr(lowerName, "ground") != NULL) {
        if (IsKeyDown(KEY_G)) {
          for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            DrawRectangleLines((int)(obj->x + layer->offsetx),
                               (int)(obj->y + layer->offsety),
                               (int)obj->width, (int)obj->height, GREEN);
          }
        }
        continue;
      }

      // --- VẼ TILE LAYER ---
      if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
          for (int y = 0; y < layer->height; y++) {
              for (int x = 0; x < layer->width; x++) {
                  int gid = layer->data[y * layer->width + x];
                  if (gid == 0) continue;
                  
                  // Tìm tileset phù hợp cho gid này
                  int tsIdx = -1;
                  for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                      if (gid >= gameMap.tilesets[k].firstgid) {
                          tsIdx = k;
                          break;
                      }
                  }

                  if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0) {
                      TMJTileset *ts = &gameMap.tilesets[tsIdx];
                      int localId = gid - ts->firstgid;
                      int tx = ts->margin + (localId % ts->columns) * (ts->tileWidth + ts->spacing);
                      int ty = ts->margin + (localId / ts->columns) * (ts->tileHeight + ts->spacing);
                      
                      Rectangle source = { (float)tx, (float)ty, (float)ts->tileWidth, (float)ts->tileHeight };
                      Vector2 pos = { (float)x * gameMap.tileWidth + layer->offsetx, (float)y * gameMap.tileHeight + layer->offsety };
                      DrawTextureRec(ts->texture, source, pos, WHITE);
                  }
              }
          }
      }


      // --- VẼ OBJECT LAYER ---
      if (strcmp(layer->type, "objectgroup") == 0) {
          for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (!obj->visible || obj->texture.id == 0)
              continue;
            
            Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
            if (obj->flipX) source.width = -source.width;
            if (obj->flipY) source.height = -source.height;
            
            // Vẽ vật thể tại vị trí tuyệt đối từ Tiled + Offset của Layer
            Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
            
            // Tiled dùng bottom-left cho GID objects
            Vector2 origin = {0, obj->height};
            
            DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                           Fade(WHITE, layer->opacity * obj->opacity));
          }
      }
    }

    DrawPlayer(&player, texIdle, texWalk, texRun, texJump, texAttack, texRunJump, texHurt, 64, 64, 0.78f);
    DrawSkills(&player);

    EndMode2D();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle sourceRec = {0.0f, 0.0f, (float)target.texture.width,
                           (float)-target.texture.height};
    Rectangle destRec = {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
    DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f,
                   WHITE);
    DrawFPS(10, 10);
    EndDrawing();
  }

  UnloadTexture(texIdle);
  UnloadTexture(texWalk);
  UnloadTexture(texRun);
  UnloadTexture(texJump);
  UnloadTexture(texAttack);
  UnloadTexture(texRunJump);
  UnloadTexture(texHurt);
  UnloadMapData(&gameMap);
  UnloadRenderTexture(target);
  CloseWindow();
  return 0;
}
