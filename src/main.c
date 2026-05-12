#include "camera.h"
#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define VIRTUAL_HEIGHT 760
#define VIRTUAL_WIDTH 1351

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
             "The Forest - Full Screen Infinite Map");
  SetTargetFPS(60);

  RenderTexture2D target = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

  GameMap gameMap = LoadMapData("assets/back.tmj");

  float groundY = 696.0f;
  for (int i = 0; i < gameMap.layerCount; i++) {
    if (TextIsEqual(gameMap.layers[i].name, "ground") &&
        gameMap.layers[i].objectCount > 0) {
      groundY = gameMap.layers[i].objects[0].y;
      break;
    }
  }

  Player player = {0};
  InitPlayer(&player, (Vector2){100, groundY});

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
  CameraSetBounds(&myCam, 1000000.0f, 760.0f, VIRTUAL_WIDTH, VIRTUAL_HEIGHT);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    UpdatePlayer(&player, dt);
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

      // --- VẼ NGƯỜI CHƠI NẰM DƯỚI LỚP CỎ (GRASS) ---
      if (TextIsEqual(layer->name, "grass")) {
        DrawPlayer(&player, texIdle, texWalk, texRun, texJump, texAttack, 64, 64, 1.0f);
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

      for (int chunkOffset = -1; chunkOffset <= 2; chunkOffset++) {
        float offsetX = (currentChunk + chunkOffset) * 900.0f;
        if (offsetX < -900.0f)
          continue;

        for (int j = 0; j < layer->objectCount; j++) {
          TMJObject *obj = &layer->objects[j];
          if (!obj->visible || obj->texture.id == 0)
            continue;
          Rectangle source = {0, 0, (float)obj->texture.width,
                              (float)obj->texture.height};
          if (obj->flipX)
            source.width = -source.width;
          if (obj->flipY)
            source.height = -source.height;
          Rectangle dest = {obj->x + offsetX, obj->y, obj->width, obj->height};
          DrawTexturePro(obj->texture, source, dest, (Vector2){0, obj->height},
                         obj->rotation,
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
