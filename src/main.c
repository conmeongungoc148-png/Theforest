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
                    float spawnY = obj->y + map->layers[i].offsety + obj->height - 20.0f;
                    TraceLog(LOG_INFO, "[SPAWN] Found spawn point at: %.2f, %.2f", spawnX, spawnY);
                    return (Vector2){spawnX, spawnY};
                }
            }
        }
    }
    TraceLog(LOG_WARNING, "[SPAWN] Spawn point NOT found! Using default: %.2f, %.2f", defaultPos.x, defaultPos.y);
    return defaultPos;
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
             "The Forest - Full Screen Infinite Map");
  SetTargetFPS(60);

  RenderTexture2D target = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

  const char *mapList[] = {
      "assets/The forestmap 2.tmj",
      "assets/secondmap.tmj",
      "assets/Thirdcitymap.tmj",
      "assets/nowfinishedmap.tmj"
  };
  int totalMaps = 4;
  int currentMapIndex = 0;

  GameMap gameMap = LoadMapData(mapList[currentMapIndex]);

  // Find spawn point from the map, or fallback to default
  Vector2 startPos = FindSpawnPoint(&gameMap, (Vector2){50.0f, 1312.0f});

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
  Texture2D texOvertree = LoadTexture("assets/overtree.png");

  MyCamera myCam = CameraNew(player.position.x, player.position.y,
                             VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  myCam.zoom = 1.42f;
  CameraSetSmoothDamped(&myCam, 10.0f);
  float mapPixelWidth = (float)gameMap.mapWidth * gameMap.tileWidth;
  float mapPixelHeight = (float)gameMap.mapHeight * gameMap.tileHeight;
  CameraSetBounds(&myCam, mapPixelWidth, mapPixelHeight, VIRTUAL_WIDTH, VIRTUAL_HEIGHT);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.016f; // Cap dt to prevent physics glitches when loading a map takes time!
    UpdatePlayer(&player, &gameMap, dt);

    if (player.loadNextMap) {
        currentMapIndex++;
        if (currentMapIndex >= totalMaps) {
            currentMapIndex = 0; // Loop back to the first map when you beat the last one!
        }

        UnloadMapData(&gameMap);
        gameMap = LoadMapData(mapList[currentMapIndex]);
        player.loadNextMap = false;
        player.position = FindSpawnPoint(&gameMap, (Vector2){100.0f, 100.0f}); // Teleport to spawn point
        
        // Reset player states completely
        player.velocity = (Vector2){0, 0};
        player.currentFrame = 0;
        player.isAttacking = false;
        player.isJumping = false;
        player.freezeTimer = 0.0f;

        // Snap camera instantly to new location
        CameraLookAt(&myCam, player.position);
        
        // Update bounds for camera just in case the new map has different dimensions
        mapPixelWidth = (float)gameMap.mapWidth * gameMap.tileWidth;
        mapPixelHeight = (float)gameMap.mapHeight * gameMap.tileHeight;
        CameraSetBounds(&myCam, mapPixelWidth, mapPixelHeight, VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
    }

    CameraUpdate(&myCam, player.position, dt);

    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode2D(myCam.rl);

    int currentChunk = (int)(player.position.x / 900.0f);

    // Duyệt qua từng layer để đảm bảo Z-order đúng
    for (int i = 0; i < gameMap.layerCount; i++) {
      TMJLayer *layer = &gameMap.layers[i];
      if (!layer->visible)
        continue;

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

      // --- VẼ NGƯỜI CHƠI NẰM DƯỚI LỚP CỎ (HOẶC SAU TILE LAYER 1) ---
      if (TextIsEqual(layer->name, "grass") || TextIsEqual(layer->name, "Tile Layer 1")) {
        DrawPlayer(&player, texIdle, texWalk, texRun, texJump, texAttack, 64, 64, 0.5f);
      }

      // TRƯỚC KHI VẼ LỚP LEAFS, VẼ OVERTREE
      if (TextIsEqual(layer->name, "leafs")) {
        for (int chunkOffset = -1; chunkOffset <= 2; chunkOffset++) {
          float offsetX = (currentChunk + chunkOffset) * 900.0f;
          if (offsetX < -900.0f)
            continue;

          float overX = offsetX + 900.0f;
          float overBaseY = 760.0f;
          float targetTopY = 320.0f; // Hạ đỉnh cây xuống mốc 330

          float overH = overBaseY - targetTopY;
          float scaleRatio = overH / (float)texOvertree.height;
          float overW = (float)texOvertree.width * scaleRatio;

          Vector2 origin = {overW / 2.0f, overH};
          DrawTexturePro(texOvertree,
                         (Rectangle){0, 0, (float)texOvertree.width,
                                     (float)texOvertree.height},
                         (Rectangle){overX, overBaseY, overW, overH}, origin,
                         0.0f, WHITE);
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
  UnloadTexture(texOvertree);
  UnloadMapData(&gameMap);
  UnloadRenderTexture(target);
  CloseWindow();
  return 0;
}
