# Bộ nhớ game mà AoE Timeline đọc

Game: Age of Empires: Rise of Rome 1.0, file `EMPIRESX.EXE` của gói AoE-HD (XArena dùng cùng file). Exe không bật
ASLR, image base cố định `0x400000`, nên các địa chỉ dưới đây dùng được trên mọi máy. Các địa chỉ được dịch ngược
tĩnh bằng capstone, rồi kiểm chứng trên trận thật. Ký hiệu `[X]` là giá trị dword tại địa chỉ X.

## Gốc

| Địa chỉ | Ý nghĩa |
|---|---|
| `[0x585E88]` | TRIBE_Game (thiết lập ván) |
| `[0x585E88]+0x3F4` | thế giới (world); bằng 0 khi không ở trong trận |
| `settings+0x994 + (n-1)` | byte đội của người chơi n: 1 = không đội, 2.. = đội 1.. |
| `world+0x04` | giờ game (ms) |
| `world+0x3C` | int16 số người chơi, kể cả Gaia (chỉ số 0) |
| `world+0x40` | mảng con trỏ PLAYER |
| `world+0x7C` | int16 người chơi của máy này; vẫn đúng ở màn Achievements |

## PLAYER

| Offset | Ý nghĩa |
|---|---|
| `+0x28`, `+0x30` | danh sách unit: header → `[+4]` node đầu; node `{unit, next}` |
| `+0x44` | `char*` tên người chơi |
| `+0x50` | `float*` thuộc tính; chỉ số 11 = dân số hiện tại |
| `+0x80` / `+0x81` | trạng thái 0 đang chơi / 1 thắng / 2 thua; cờ đầu hàng |
| `+0x109` | civ 1..16 |
| `+0x224` | đối tượng nghiên cứu: `[+0]` → mảng entry 8 byte theo id công nghệ: `float` giây đã nghiên cứu, `int16` trạng thái (2 đang nghiên cứu, 3 xong). Bronze Age = công nghệ 102 |
| `+0x840` | lịch sử Timeline (xem dưới) |

## Lịch sử Timeline: `PLAYER+0x840`

Tìm qua chuỗi `tpnl_tml::draw_timeline`: hàm vẽ màn Timeline là `0x4FFAB0`.

- Đối tượng: `+0x00` `byte*` mảng mẫu, `+0x04` danh sách sự kiện, `+0x0C` int số mẫu, `+0x10` sức chứa (mỗi lần
  tăng thêm 60).
- Hàm ghi mẫu `0x4B2430`, được gọi từ `0x4F23C0` (cập nhật người chơi). `PLAYER+0x238` cộng dồn thời gian tick;
  mỗi đủ 1,0 giây thì `PLAYER+0x230` tăng 1; đủ 15 thì ghi một mẫu = `round(thuộc tính 11)`, tức dân số. Hàm
  `0x4F3E40` ghi mẫu đầu tiên lúc bắt đầu trận. Vậy mẫu #k ứng với giây k×15 (đo thật: lúc 3,5 s đã có 1 mẫu, lúc
  19 s có 2 mẫu).
- Hàm vẽ cộng giá trị mẫu của người chơi 1..n-1 để ra tỉ lệ; hàm đọc mẫu `0x4B24A0` trả 0 khi chỉ số vượt quá số
  mẫu đang có.
- Sự kiện: node `{byte loại, int chỉ số mẫu, int giờ game ms, node kế}`. Hàm `0x4ECD0D` (nghiên cứu xong) ghi loại
  0/1/2 khi xong công nghệ 101/102/103, tức lên xong đời 2/3/4.

## Unit

`+0x04` id (tăng dần theo thứ tự sinh, đã kiểm), `+0x08` → định nghĩa unit (`+0x04` loại đối tượng: 70 = unit,
`+0x08` tên, `+0x10` id loại, `+0x14` lớp), `+0x0C` → PLAYER chủ, `+0x30` máu.

Quân = loại đối tượng 70, trừ các lớp dân (4), thuyền buôn (2), xe buôn (19), thuyền chở (20), thuyền đánh cá (21).

**Không có giờ sinh.** Đã quét 0x600 byte đầu của 23 unit và công trình mới sinh, mỗi cái đọc hai lần cách nhau
24 giây game. Không có trường nào vừa giữ nguyên sau khi sinh vừa tăng theo thứ tự sinh, ngoài id. `+0xA4` là giờ
game của hành động gần nhất, được cập nhật liên tục.

## Khi một người chơi rớt mạng

Đo trên trận 1v1 thật (hai bản game chạy ẩn, tắt hẳn tiến trình của một bên):

| Thời điểm | Máy còn lại |
|---|---|
| +0 s | tiến trình bên kia bị tắt |
| +5 s | giờ game dừng (979 020 ms), vẫn ở màn trận, trạng thái hai người vẫn là 0 |
| +63 s | màn Status |
| +68 s | màn Achievements: **máy còn lại bị xử thua** (trạng thái 2), người rớt vẫn là 0; dữ liệu phiên mạng bị xoá; Timeline, tên, giờ game vẫn còn |

Vì vậy tool không dùng trạng thái thắng/thua để xác định người rớt. Ở Solo, người rớt mặc định là đối thủ của máy
đang chạy tool; ở 4VS4, trọng tài chọn.
