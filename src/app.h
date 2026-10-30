/* Trạng thái chung của ứng dụng: dữ liệu đọc từ game + kết quả phân tích theo luật. */
#ifndef APP_H
#define APP_H

#include "game.h"
#include "rules.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

typedef struct {
    int valid;                  /* có đủ dữ liệu để xử */
    int team_mode;              /* >2 người chơi -> thể thức 4VS4 */
    int nsides;
    int side_of[MAX_PLAYERS];   /* chỉ số phe của từng người chơi */
    wchar_t side_name[MAX_PLAYERS][160];
    long side_pop[MAX_PLAYERS]; /* dân số timeline của phe tại mẫu xét */
    int dc;                     /* chỉ số người rớt mạng trong snap.p, -1 nếu chưa biết */
    int dc_auto;                /* 0 = trọng tài chọn, 1 = đối thủ của máy này (Solo), 2 = gợi ý (4VS4) */
    int dc_side, other_side;
    int dc_time_s;              /* giờ game lúc rớt */
    int sample;                 /* chỉ số mẫu timeline dùng để xử (-1 nếu chưa có mẫu) */
    DcResult res;
    MilCase mil[MAX_PLAYERS];
    int mil_flags[MAX_PLAYERS];
    wchar_t title[200];         /* kết luận ngắn */
    wchar_t detail[400];        /* giải thích */
} Analysis;

typedef struct {
    Snapshot snap;
    Analysis an;
    int demo;                   /* dữ liệu mẫu (xem giao diện / chụp ảnh README) */
    DWORD pid_arg;              /* --pid */
    int dc_override;            /* số người chơi do trọng tài chọn (0 = tự động) */
    int at_override_s;          /* --at: giờ rớt mạng do trọng tài nhập (-1 = tự động) */
    wchar_t out_dir[MAX_PATH];  /* thư mục lưu báo cáo */
    wchar_t report_path[MAX_PATH];
    wchar_t status[260];        /* dòng trạng thái (đã lưu báo cáo ở đâu, lỗi...) */
    int need_admin;
} App;

void app_refresh(App *a);       /* tìm game, đọc lại, phân tích */
void app_analyze(App *a);
void app_demo(App *a);
void fmt_time(int s, wchar_t *out, int n);   /* "mm:ss" / "h:mm:ss"; "—" nếu s < 0 */
const wchar_t *civ_name(int civ);
/* level 2 vi phạm, 1 không xác định, 0 hợp lệ; brief = bản ngắn cho cột KẾT LUẬN của giao diện */
const wchar_t *mil_verdict(int flags, int n_mil, int brief, int *level);

int report_write(App *a);       /* ghi .txt + .csv vào out_dir; 1 = được */

int ui_run(App *a);
int ui_shot(App *a, const wchar_t *bmp_path, int w, int h);

#endif
