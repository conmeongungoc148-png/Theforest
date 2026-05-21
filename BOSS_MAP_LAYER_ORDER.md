# Boss Map - Thứ tự Layer CHÍNH XÁC

## ✅ Code đã xử lý ĐÚNG thứ tự layer!

### Cách cute_tiled xử lý:
1. **Đọc JSON**: Layers được đọc theo thứ tự trong file (sky → mountain → Group Layer 1)
2. **Tự động REVERSE**: `cute_tiled` tự động reverse linked list → thứ tự ngược lại
3. **Flatten Group**: Hàm `ProcessLayers()` trong `game.c` flatten tất cả group layers
4. **Kết quả**: Layers được vẽ theo đúng thứ tự Z-order từ dưới lên trên

## Thứ tự vẽ cuối cùng (từ dưới lên trên):

### Background Layers (parallax)
1. **sky** (objectgroup) - Background trời, parallax 0.5
2. **mountain** (objectgroup) - Background núi, parallax 0.8

### Main Layers (từ Group Layer 1)
3. **fence** (tilelayer) - Hàng rào
4. **building** (objectgroup) - Tòa nhà
5. **tiles2** (tilelayer) - Tiles phụ, offsety: 50
6. **statue** (objectgroup) - Tượng, offsety: 50
7. **black** (objectgroup) - Overlay đen (opacity 0.15)
8. **abc** (objectgroup) - Object đặc biệt (boss/NPC?)
9. **tiles** (tilelayer) - Tiles chính, offsety: 50
10. **props** (tilelayer) - Props/decoration, offsety: 50
11. **ground** (objectgroup) ⭐ - **KHÔNG VẼ** - chỉ dùng collision!

### Player Layer
12. **Player** - Vẽ sau tất cả layers

## Layer "ground" - Collision Only

Layer "ground" **KHÔNG ĐƯỢC VẼ** ra màn hình:
- Code skip layer này khi vẽ
- Chỉ dùng cho collision detection trong `UpdatePlayer()`
- Chứa 3 collision rectangles:
  - **Sàn chính**: 1217x64 tại (0.67, 370.67 + 50 offset) = (0.67, 420.67)
  - **Platform trái**: 352x20 tại (0, 240 + 50 offset) = (0, 290)
  - **Platform phải**: 350x20 tại (864, 240 + 50 offset) = (864, 290)

## Debug Mode

Nhấn giữ phím **G** trong game để xem collision boxes của layer "ground" (màu xanh lá).

## Code Implementation

```c
// Trong main.c - Vẽ layers
for (int i = 0; i < gameMap.layerCount; i++) {
    TMJLayer *layer = &gameMap.layers[i];
    
    // Skip layer "ground" - chỉ dùng collision
    if (strstr(layer->name, "ground") != NULL) {
        // Debug: Vẽ collision boxes nếu nhấn G
        if (IsKeyDown(KEY_G)) {
            // Vẽ green rectangles...
        }
        continue; // Không vẽ layer ground
    }
    
    // Vẽ các layers khác...
}
```

```c
// Trong game.c - ProcessLayers flatten group layers
static void ProcessLayers(cute_tiled_layer_t *layer, GameMap *map,
                          int *currentLayerIdx, float parentOffsetX,
                          float parentOffsetY) {
    while (layer) {
        if (strcmp(layer->type.ptr, "group") == 0) {
            // Đệ quy vào group, cộng offset của parent
            ProcessLayers(layer->layers, map, currentLayerIdx,
                        parentOffsetX + layer->offsetx,
                        parentOffsetY + layer->offsety);
        } else {
            // Xử lý layer thường, lưu offset tổng
            tmjLayer->offsetx = parentOffsetX + layer->offsetx;
            tmjLayer->offsety = parentOffsetY + layer->offsety;
            // ...
        }
        layer = layer->next;
    }
}
```

## Kết luận

✅ Thứ tự layer **ĐÃ ĐÚNG** - code tự động xử lý:
- Flatten group layers
- Cộng offset của parent vào child
- Vẽ theo đúng thứ tự Z-order
- Skip layer "ground" khi vẽ (chỉ dùng collision)

Nhân vật sẽ đứng trên các collision rectangles trong layer "ground" với offset đã được tính sẵn!
