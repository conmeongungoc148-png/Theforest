# Boss Map (Map 3) - Thông tin chi tiết

## Tổng quan
Map 3 là **Boss Arena** - khu vực chiến đấu với boss, được load từ file `assets/boss map 1v1.tmj`

## Cấu trúc Map
- **Kích thước**: 76 tiles (width) x 34 tiles (height)
- **Tile size**: 16x16 pixels
- **Kích thước thực tế**: 1216 x 544 pixels

## Layer "ground" - Collision Objects

Map có 3 vùng collision chính:

### 1. Sàn chính (Main Floor)
- **Vị trí**: x=0.67, y=370.67
- **Kích thước**: width=1217, height=64
- **Mô tả**: Sàn chính cho trận chiến boss, nhân vật sẽ đứng trên đây

### 2. Platform trái (Left Platform)
- **Vị trí**: x=0, y=240
- **Kích thước**: width=352, height=20
- **Mô tả**: Platform phụ bên trái, có thể nhảy lên để tránh đòn boss

### 3. Platform phải (Right Platform)
- **Vị trí**: x=864, y=240
- **Kích thước**: width=350, height=20
- **Mô tả**: Platform phụ bên phải, có thể nhảy lên để tránh đòn boss

## Cách chuyển map
Nhấn phím **R** để chuyển sang map tiếp theo:
- Map 1 (Forest) → Map 2 (Night City) → **Map 3 (Boss Arena)** → Map 1 (lặp lại)

## Lưu ý kỹ thuật
- Layer "ground" được xử lý tự động bởi hàm `UpdatePlayer()` trong `src/game.c`
- Code tự động phát hiện layer có tên chứa "ground" và xử lý collision
- Nhân vật sẽ đứng trên các collision objects với offset -64 pixels (chiều cao nhân vật)
- Spawn point mặc định: (50, 1168) nếu không tìm thấy spawn point trong map

## Các layer khác trong Boss Map
- **sky**: Background layer với parallax
- **mountain**: Layer núi với parallax
- **fence**: Hàng rào trang trí
- **building**: Các tòa nhà
- **statue**: Tượng trang trí
- **tiles**: Tile layer chính
- **props**: Các vật thể trang trí

## Code liên quan
- `src/main.c`: Quản lý danh sách map và chuyển map
- `src/game.c`: Xử lý collision với layer "ground"
- `src/map.c`: Load và parse file TMJ
