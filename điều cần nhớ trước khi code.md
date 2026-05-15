# Điều cần nhớ trước khi code - The Forest Project

Tài liệu này tóm tắt cấu trúc hệ thống và các lưu ý quan trọng để đảm bảo tính ổn định khi mở rộng game.

## 1. Cấu trúc Thư mục & File (Modular Architecture)
Dự án được tách thành các module riêng biệt để dễ quản lý:
- **`main.c`**: Trung tâm điều khiển, xử lý vòng lặp game, chuyển đổi Map (phím `R`) và khởi tạo tài nguyên.
- **`map.c / map.h`**: Xử lý toàn bộ logic liên quan đến Tiled Map (`cute_tiled`), bao gồm nạp file `.tmj`, vẽ layer và bộ nhớ đệm hình ảnh (**Texture Cache**).
- **`game.c / game.h`**: Quản lý logic nhân vật mèo (Player), trạng thái di chuyển, tấn công và animation.
- **`camera.c / camera.h`**: Hệ thống camera nâng cao (Smooth damping, Screen shake, Camera Bounds).

## 2. Hệ thống Bản đồ (Tiled Map Integration)
- **Cơ chế nạp:** Sử dụng `cute_tiled.h`. Luôn nạp qua `MapLoad()` để kích hoạt hệ thống Cache.
- **Mặt đất (GroundY):** Hệ thống tự động tìm Layer có tên chính xác là `"ground"`. Tọa độ `y` của vật thể đầu tiên trong layer này sẽ được dùng làm `groundY` cho nhân vật.
- **Đa Tileset:** Hỗ trợ nạp nhiều bộ gạch khác nhau trong cùng một bản đồ. Nếu một viên gạch có GID thuộc Tileset khác, hệ thống sẽ tự động tìm ảnh tương ứng.
- **Texture Cache:** Mọi hình ảnh bản đồ được nạp vào `MapTextureCache`. Khi đổi Map (phím `R`), phải gọi `MapUnload()` để giải phóng cache tránh rò rỉ bộ nhớ.

## 3. Điều khiển Nhân vật (Controls)
- **Di chuyển:** WASD hoặc phím Mũi tên.
- **Nhảy:** Phím `W` hoặc `Space`.
- **Tấn công:** Chuột trái.
- **Chuyển Map:** Phím `R`.
- **Lưu ý Sprite:** Sprite mèo mặc định quay mặt sang **Trái**. Do đó, logic vẽ trong `DrawPlayer` là: `if (player->facingRight) source.width = -source.width;`.

## 4. Camera & Hiển thị
- **Vị trí nhân vật:** Được thiết lập ở mức 72% chiều cao màn hình (khoảng 1/3 dưới) để tối ưu tầm nhìn phía trên.
- **Biên giới hạn (Bounds):** Camera luôn bị giới hạn trong kích thước bản đồ. Khi đổi Map, biên này phải được cập nhật lại theo `map->width` và `map->height`.
- **Smooth Mode:** Sử dụng `CameraSetSmoothDamped` để tạo hiệu ứng trượt mượt mà.

## 5. Quy tắc mở rộng
- Khi tạo bản đồ mới trong Tiled:
    1. Đặt tên Layer mặt đất là `"ground"`.
    2. Vẽ một Object tại vị trí muốn nhân vật đứng để xác định độ cao.
    3. Export sang định dạng `.tmj` (JSON).
    4. Thêm đường dẫn vào mảng `mapFiles[]` trong `main.c`.

## 6. Quản lý Phiên bản (Git & GitHub)
Để bảo vệ code và dễ dàng quay lại các phiên bản ổn định:
- **Git Branch:** 
    - Luôn giữ nhánh `main` là bản chạy ổn định nhất (Production-ready).
    - Khi phát triển tính năng mới (ví dụ: thêm quái vật, hệ thống túi đồ), hãy tạo nhánh riêng: `git checkout -b feature/ten-tinh-nang`.
    - Chỉ gộp (merge) vào `main` sau khi đã kiểm tra kỹ không gây lỗi (crash).
- **Git Commit:**
    - Commit thường xuyên theo từng bước nhỏ. Đừng đợi code cả ngày mới commit một lần.
    - Message commit nên rõ ràng (Tiếng Việt hoặc Tiếng Anh), ví dụ: 
        - `feat: thêm hệ thống chuyển map bằng phím R`
        - `fix: sửa lỗi mèo quay sai hướng khi nhảy`
- **GitHub Sync:**
    - Sau mỗi buổi làm việc, hãy `git push origin <ten-nhanh>` để lưu trữ lên GitHub.

### Quy trình lệnh Git mẫu (Cheat Sheet)
```bash
# 1. Tạo nhánh mới để làm tính năng mới
git checkout -b feature/ten-tinh-nang

# 2. Kiểm tra các file đã thay đổi
git status

# 3. Thêm tất cả thay đổi vào hàng đợi commit
git add .

# 4. Lưu lại phiên bản (Commit)
git commit -m "feat: mô tả tính năng vừa làm"
# Hoặc: git commit -m "fix: mô tả lỗi vừa sửa"

# 5. Đẩy code lên GitHub
git push origin feature/ten-tinh-nang

# 6. Hợp nhất vào nhánh chính (sau khi tính năng đã xong)
git checkout main
git merge feature/ten-tinh-nang
git push origin main
```

---
*Ghi chú: Luôn sử dụng `mingw32-make run` để biên dịch và chạy bản chính thức.*
