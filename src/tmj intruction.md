BƯỚC 1: Chuẩn bị vũ khí (Thư viện cJSON)
Vì C không có sẵn hàm đọc JSON, bạn cần thư viện cJSON. Thư viện này cực kỳ phù hợp cho project của bạn vì nó siêu nhẹ.

Bạn chỉ cần tải 2 file: cJSON.h và cJSON.c.

Bỏ cả 2 file này chung vào thư mục chứa file main.c của bạn.

Trong main.c, thêm dòng: #include "cJSON.h".

BƯỚC 2: Định nghĩa "Cái rổ" đựng dữ liệu (C Structs)
Bạn phải tạo sẵn các struct để hứng dữ liệu từ file .tmj đổ vào.

C
#include "raylib.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

// Struct chứa một layer gạch (Background, Grass, BeforeGrass)
typedef struct {
    char name[32];
    int *data;      // Mảng 1 chiều chứa ID của các viên gạch
    int width;
    int height;
} TileLayer;

// Struct chứa toàn bộ Map
typedef struct {
    int mapWidth;
    int mapHeight;
    int tileWidth;
    int tileHeight;
    
    TileLayer *layers;     // Mảng các layer đồ họa
    int layerCount;
    
    Rectangle *collisions; // Mảng các khối va chạm vô hình
    int collisionCount;
} GameMap;
BƯỚC 3: Viết hàm Parse (Giải mã) file .tmj
Đây là lúc bạn dùng Raylib để đọc file text và cJSON để bóc tách nó. Bạn sẽ gọi hàm này trước khi vào vòng lặp while.

C
GameMap LoadMapData(const char *fileName) {
    GameMap map = { 0 };

    // 1. Raylib hỗ trợ đọc toàn bộ nội dung file thành chuỗi Text
    char *fileData = LoadFileText(fileName); 
    if (fileData == NULL) return map; // Lỗi không tìm thấy file

    // 2. Yêu cầu cJSON phân tích chuỗi Text
    cJSON *mapJSON = cJSON_Parse(fileData);

    // 3. Lấy thông số cơ bản của bản đồ
    map.mapWidth = cJSON_GetObjectItem(mapJSON, "width")->valueint;
    map.mapHeight = cJSON_GetObjectItem(mapJSON, "height")->valueint;
    map.tileWidth = cJSON_GetObjectItem(mapJSON, "tilewidth")->valueint;
    map.tileHeight = cJSON_GetObjectItem(mapJSON, "tileheight")->valueint;

    // 4. Lặp qua danh sách các Layers
    cJSON *layers = cJSON_GetObjectItem(mapJSON, "layers");
    int totalLayers = cJSON_GetArraySize(layers);
    
    // Cấp phát bộ nhớ cho mảng layers của C
    map.layers = (TileLayer *)malloc(totalLayers * sizeof(TileLayer));
    map.layerCount = 0;

    cJSON *layer = NULL;
    cJSON_ArrayForEach(layer, layers) {
        cJSON *type = cJSON_GetObjectItem(layer, "type");
        
        // --- NẾU LÀ LAYER GẠCH (ĐỒ HỌA) ---
        if (strcmp(type->valuestring, "tilelayer") == 0) {
            TileLayer newLayer;
            strcpy(newLayer.name, cJSON_GetObjectItem(layer, "name")->valuestring);
            newLayer.width = cJSON_GetObjectItem(layer, "width")->valueint;
            newLayer.height = cJSON_GetObjectItem(layer, "height")->valueint;

            // Đọc mảng data ID
            cJSON *dataArray = cJSON_GetObjectItem(layer, "data");
            int dataSize = cJSON_GetArraySize(dataArray);
            newLayer.data = (int *)malloc(dataSize * sizeof(int));
            
            for (int i = 0; i < dataSize; i++) {
                newLayer.data[i] = cJSON_GetArrayItem(dataArray, i)->valueint;
            }
            
            map.layers[map.layerCount] = newLayer;
            map.layerCount++;
        }
        // --- NẾU LÀ LAYER OBJECT (VA CHẠM / ĐẤT) ---
        else if (strcmp(type->valuestring, "objectgroup") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            map.collisionCount = cJSON_GetArraySize(objects);
            map.collisions = (Rectangle *)malloc(map.collisionCount * sizeof(Rectangle));
            
            cJSON *obj = NULL;
            int idx = 0;
            cJSON_ArrayForEach(obj, objects) {
                map.collisions[idx].x = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                map.collisions[idx].y = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                map.collisions[idx].width = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                map.collisions[idx].height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;
                idx++;
            }
        }
    }

    // 5. CỰC KỲ QUAN TRỌNG: DỌN DẸP BỘ NHỚ JSON
    cJSON_Delete(mapJSON);    // Xóa object JSON khỏi RAM
    UnloadFileText(fileData); // Trả lại bộ nhớ text cho Raylib

    return map;
}
BƯỚC 4: Cách sử dụng dữ liệu để vẽ (Render)
Bây giờ bạn đã có struct GameMap hoàn toàn thuần C, không còn dính líu gì đến JSON nữa. Tốc độ đọc lúc này là nhanh nhất có thể.

C
void DrawTileLayer(TileLayer layer, Texture2D tileset, int mapCols) {
    for (int i = 0; i < layer.width * layer.height; i++) {
        int tileID = layer.data[i];
        if (tileID == 0) continue; // ID = 0 là ô trống (trong suốt)

        tileID -= 1; // Tiled xuất ID bắt đầu từ 1, nhưng tính toán cắt ảnh bắt đầu từ 0

        // Tính tọa độ cắt ảnh từ file PNG
        int tileX = (tileID % mapCols) * 16;
        int tileY = (tileID / mapCols) * 16;
        Rectangle source = { tileX, tileY, 16, 16 };

        // Tính tọa độ vẽ lên màn hình
        int destX = (i % layer.width) * 16;
        int destY = (i / layer.width) * 16;
        Vector2 position = { destX, destY };

        DrawTextureRec(tileset, source, position, WHITE);
    }
}
3 Chú ý "Xương Máu" khi làm việc này trong C
tileID - 1: Trong file JSON, ID 0 nghĩa là ô trống không có gạch. Các viên gạch thực tế sẽ bắt đầu từ ID 1. Nhưng mảng và tính toán trong C bắt đầu từ 0. Vì vậy, khi bạn lấy tileID từ mảng data, luôn phải trừ đi 1 trước khi tính tọa độ tileX, tileY.

Giải phóng mảng: Khi tắt game, bạn đã malloc mảng layer.data và mảng map.collisions, bạn phải viết một hàm FreeMap(GameMap map) để free() tất cả chúng, nếu không sẽ bị Memory Leak (tràn RAM).

Double vs Int: Tọa độ của Object Layer (Rectangle) trong Tiled thường xuất ra số thực (Double/Float) chứ không phải số nguyên (Int). Đó là lý do trong đoạn code trên, mình dùng valuedouble thay vì valueint cho phần objectgroup.