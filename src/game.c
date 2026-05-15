#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    
    // Nhảy bằng SPACE hoặc phím W
    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W)) && !player->isJumping) {
        player->velocity.y = jumpForce;
        player->isJumping = true;
        if (!player->isAttacking) {
            player->currentFrame = 0;
            player->frameTimer = 0.0f;
        }
    }

    // Cho phép điều khiển ngang cả khi đang trên không bằng phím mũi tên hoặc A/D
    if (player->freezeTimer <= 0) {
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            player->position.x += currentSpeed * deltaTime;
            player->facingRight = true;
            player->isRunning = true;
        }
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            player->position.x -= currentSpeed * deltaTime;
            player->facingRight = false;
            player->isRunning = true;
        }
    }

    if (player->isJumping) {
        player->velocity.y += gravity * deltaTime;
        player->position.y += player->velocity.y * deltaTime;
        if (player->position.y >= player->groundY) {
            player->position.y = player->groundY;
            player->velocity.y = 0;
            player->isJumping = false;
        }
        if (!player->isAttacking) animSpeed = 0.1f; 
    }

    player->frameTimer += deltaTime;
    if (player->frameTimer >= animSpeed) {
        player->frameTimer = 0;
        player->currentFrame++;
    }

    if (player->isAttacking && player->currentFrame >= 8) {
        player->isAttacking = false;
    }
}

void DrawPlayer(Player *player, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, int frameW, int frameH, float scale) {
    Texture2D currentTex = idle;
    int maxFrames = 10;
    int spacing = 16; // Khôi phục spacing 16px
    int frameIdx = player->currentFrame % maxFrames;

    if (player->isAttacking) { currentTex = attack; maxFrames = 8; }
    else if (player->isJumping) { currentTex = jump; maxFrames = 3; }
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
    
    // Khôi phục logic hướng nhìn chuẩn của Sprite mèo
    if (player->facingRight) source.width = -source.width;
    
    Rectangle dest = { player->position.x, player->position.y + yOffset, (float)frameW * scale, (float)frameH * scale };
    DrawTexturePro(currentTex, source, dest, (Vector2){(float)frameW * scale / 2.0f, (float)frameH * scale}, 0.0f, WHITE);
}
