/* Đọc trạng thái ván đấu từ bộ nhớ AoE: Rise of Rome 1.0 (EMPIRESX.EXE, bản AoE-HD / XArena).
 * Chỉ mở tiến trình game với quyền ĐỌC (PROCESS_VM_READ), không ghi, không chèn DLL. */
#ifndef GAME_H
#define GAME_H

#include <windows.h>

#define MAX_PLAYERS 8
#define MAX_SAMPLES 4000        /* 4000 mẫu x 15 giây = 16 giờ game */
#define MAX_MIL 64              /* số đơn vị quân sớm nhất giữ lại mỗi người */
#define TIMELINE_STEP_S 15      /* game ghi một mẫu timeline mỗi 15 giây game */

typedef struct {
    int id;                     /* id đối tượng trong game (tăng dần theo thứ tự sinh) */
    int type;                   /* id loại unit trong empires.dat */
    int cls;                    /* lớp unit */
    int born_s;                 /* giờ game lúc sinh (giây), -1 nếu không xác định */
    wchar_t name[40];           /* tên loại unit (tiếng Anh, từ empires.dat) */
} MilUnit;

typedef struct {
    int number;                 /* số người chơi 1..8 */
    wchar_t name[48];
    int civ;                    /* 1..16 */
    int team;                   /* 0 = không đội, 1.. = đội */
    int status;                 /* 0 đang chơi, 1 thắng, 2 thua */
    int resigned;
    int comm_state;             /* trạng thái người chơi trong phiên mạng */
    float pop_now;              /* dân số hiện tại (thuộc tính 11) */
    int n_samples;
    unsigned char samples[MAX_SAMPLES];
    int age_done_s[3];          /* lên xong đời 2 / 3 / 4 (giây game), -1 nếu chưa */
    int bronze_state;           /* nghiên cứu Bronze Age: 0 khoá, 1 có thể, 2 đang nghiên cứu, 3 xong */
    float bronze_progress;      /* giây đã nghiên cứu Bronze */
    int bronze_click_s;         /* lúc kích đời 3 (giây game), -1 nếu chưa kích */
    int n_mil;                  /* số đơn vị quân đang sống */
    MilUnit mil[MAX_MIL];       /* sắp theo thứ tự sinh */
} Player;

typedef struct {
    DWORD pid;
    wchar_t exe[MAX_PATH];
    int in_match;               /* 0 nếu game không ở trong trận (dữ liệu đã mất) */
    unsigned time_ms;           /* giờ game */
    int frozen;                 /* giờ game đứng yên trong lúc đọc (game đang chờ người chơi) */
    int game_over;
    int me;                     /* số người chơi của máy này */
    int n;                      /* số người chơi (không tính Gaia) */
    Player p[MAX_PLAYERS];
    wchar_t error[200];
} Snapshot;

typedef struct {
    DWORD pid;
    wchar_t exe[MAX_PATH];
} GameProc;

int game_find(GameProc *out, int max);                 /* liệt kê tiến trình AoE đang chạy */
int game_open(DWORD pid);                               /* 1 = mở được; 0 = lỗi; -1 = cần quyền admin */
void game_close(void);
int game_read(Snapshot *s);                             /* 1 = đọc được trận */
unsigned game_clock(void);                              /* giờ game (ms), 0 nếu không đọc được */

#endif
