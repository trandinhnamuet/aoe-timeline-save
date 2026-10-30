/* Đọc bộ nhớ EMPIRESX.EXE. Mọi địa chỉ dưới đây được dịch ngược từ file exe (không ASLR, image base 0x400000)
 * và kiểm chứng trên trận thật; ghi chú chi tiết trong docs/bo-nho-game.md. */
#include "game.h"
#include "rules.h"
#include <tlhelp32.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define A_SETTINGS 0x585E88     /* -> TRIBE_Game (thiết lập ván) */
#define A_COMM 0x585DF8         /* -> TCommunications_Handler (phiên mạng) */

#define S_WORLD 0x3F4           /* settings -> thế giới (0 khi không trong trận) */
#define S_TEAMS 0x994           /* byte[người chơi-1]: 1 = không đội, 2.. = đội 1.. */

#define W_TIME_MS 0x04          /* uint32 giờ game (ms) */
#define W_GAME_OVER 0x1D
#define W_PLAYER_COUNT 0x3C     /* int16, kể cả Gaia */
#define W_PLAYERS 0x40
#define W_LOCAL_PLAYER 0x7C     /* int16 người chơi của máy này (còn đúng ở màn Achievements) */

#define C_PLAYER_STATE 0x16D4   /* dword[1..8] */

#define P_UNITS_A 0x28          /* danh sách unit: header -> [+4] node đầu; node {unit, next} */
#define P_UNITS_B 0x30
#define P_NAME 0x44             /* char* */
#define P_ATTRS 0x50            /* float*; thuộc tính 11 = dân số */
#define P_STATUS 0x80
#define P_RESIGNED 0x81
#define P_CIV 0x109
#define P_RESEARCH 0x224        /* -> {entry 8 byte * n: float giây đã nghiên cứu, int16 trạng thái} */
#define P_TIMELINE 0x840        /* -> {byte* mẫu, sự kiện*, ?, int số mẫu}; RE: tpnl_tml::draw_timeline 0x4FFAB0 */

#define TECH_BRONZE 102

#define O_ID 0x04
#define O_MASTER 0x08
#define O_OWNER 0x0C
#define O_HP 0x30
#define M_TYPE 0x04             /* 70 = unit chiến đấu / dân */
#define M_NAME 0x08
#define M_ID 0x10
#define M_CLASS 0x14

/* Lớp unit KHÔNG tính là quân: artifact/ruins (1), thuyền buôn (2), dân (4), cá (5), bụi quả (7), mỏ đá (8),
 * thú (9, 10), khác (11), cây (15, 16), xe buôn (19), thuyền chở (20), thuyền đánh cá (21), gia súc (29), cờ (30),
 * cá (31, 33), mỏ vàng (32). Trên trận thật: Ruins người chơi chiếm được là lớp 1, Scout / Camel lớp 12. */
static const int NON_MILITARY_CLASSES[] = {1, 2, 4, 5, 7, 8, 9, 10, 11, 15, 16, 19, 20, 21, 29, 30, 31, 32, 33};

static HANDLE g_h;

static int rd(DWORD addr, void *buf, DWORD n)
{
    SIZE_T got = 0;
    if (!g_h || addr < 0x10000)
        return 0;
    return ReadProcessMemory(g_h, (LPCVOID)(ULONG_PTR)addr, buf, n, &got) && got == n;
}

static DWORD u32(DWORD a) { DWORD v = 0; rd(a, &v, 4); return v; }
static int i32(DWORD a) { int v = 0; rd(a, &v, 4); return v; }
static short i16(DWORD a) { short v = 0; rd(a, &v, 2); return v; }
static int u8(DWORD a) { unsigned char v = 0; rd(a, &v, 1); return v; }
static float f32(DWORD a) { float v = 0; rd(a, &v, 4); return v; }

static void cstr_w(DWORD a, wchar_t *out, int outlen)
{
    char tmp[64] = {0};
    out[0] = 0;
    if (!a || !rd(a, tmp, sizeof tmp - 1)) {
        /* chuỗi sát cuối vùng nhớ: đọc từng byte */
        for (int i = 0; a && i < (int)sizeof tmp - 1; i++)
            if (!rd(a + i, tmp + i, 1) || !tmp[i])
                break;
    }
    tmp[sizeof tmp - 1] = 0;
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, out, outlen);
    out[outlen - 1] = 0;
}

static int name_is_game(const wchar_t *n)
{
    wchar_t low[MAX_PATH];
    int i = 0;
    for (; n[i] && i < MAX_PATH - 1; i++)
        low[i] = towlower(n[i]);
    low[i] = 0;
    if (wcsncmp(low, L"empiresx", 8) == 0 && wcsstr(low, L".exe"))
        return 1;
    return wcscmp(low, L"empires.exe") == 0 || wcscmp(low, L"aoebot.exe") == 0;
}

int game_find(GameProc *out, int max)
{
    int k = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && k < max; ok = Process32NextW(snap, &pe)) {
        if (!name_is_game(pe.szExeFile))
            continue;
        out[k].pid = pe.th32ProcessID;
        wcsncpy(out[k].exe, pe.szExeFile, MAX_PATH - 1);
        out[k].exe[MAX_PATH - 1] = 0;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (h) {
            DWORD len = MAX_PATH;
            QueryFullProcessImageNameW(h, 0, out[k].exe, &len);
            CloseHandle(h);
        }
        k++;
    }
    CloseHandle(snap);
    return k;
}

int game_open(DWORD pid)
{
    game_close();
    g_h = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (g_h)
        return 1;
    return GetLastError() == ERROR_ACCESS_DENIED ? -1 : 0;
}

void game_close(void)
{
    if (g_h)
        CloseHandle(g_h);
    g_h = NULL;
}

static DWORD world(void)
{
    DWORD s = u32(A_SETTINGS);
    return s ? u32(s + S_WORLD) : 0;
}

unsigned game_clock(void)
{
    DWORD w = world();
    return w ? u32(w + W_TIME_MS) : 0;
}

static int is_military(int master_type, int cls)
{
    if (master_type != 70)
        return 0;
    for (unsigned i = 0; i < sizeof NON_MILITARY_CLASSES / sizeof NON_MILITARY_CLASSES[0]; i++)
        if (cls == NON_MILITARY_CLASSES[i])
            return 0;
    return 1;
}

static int mil_cmp(const void *a, const void *b)
{
    const MilUnit *x = a, *y = b;
    return x->id < y->id ? -1 : x->id > y->id;
}

static void read_military(DWORD pptr, Player *pl)
{
    static MilUnit all[4096];
    int n = 0;
    DWORD lists[2] = {u32(pptr + P_UNITS_A), u32(pptr + P_UNITS_B)};
    for (int li = 0; li < 2; li++) {
        DWORD node = lists[li] ? u32(lists[li] + 4) : 0;
        for (int guard = 0; node && guard < 20000 && n < 4096; guard++) {
            DWORD nd[2];
            if (!rd(node, nd, 8))
                break;
            DWORD unit = nd[0];
            node = nd[1];
            unsigned char o[0x34];
            if (!unit || !rd(unit, o, sizeof o))
                continue;
            DWORD master, owner;
            int id;
            float hp;
            memcpy(&id, o + O_ID, 4);
            memcpy(&master, o + O_MASTER, 4);
            memcpy(&owner, o + O_OWNER, 4);
            memcpy(&hp, o + O_HP, 4);
            if (owner != pptr || !master || hp <= 0)
                continue;
            unsigned char m[0x18];
            if (!rd(master, m, sizeof m))
                continue;
            short type, cls;
            memcpy(&type, m + M_ID, 2);
            memcpy(&cls, m + M_CLASS, 2);
            if (!is_military(m[M_TYPE], cls))
                continue;
            int dup = 0;
            for (int j = 0; j < n && !dup; j++)
                dup = all[j].id == id;
            if (dup)
                continue;
            MilUnit *u = &all[n++];
            u->id = id;
            u->type = type;
            u->cls = cls;
            u->born_s = -1;
            DWORD np;
            memcpy(&np, m + M_NAME, 4);
            cstr_w(np, u->name, 40);
            if (!wcsncmp(u->name, L"Soldier-", 8))
                memmove(u->name, u->name + 8, (wcslen(u->name + 8) + 1) * sizeof(wchar_t));
        }
    }
    qsort(all, n, sizeof all[0], mil_cmp);
    pl->n_mil = n;
    memcpy(pl->mil, all, sizeof(MilUnit) * (n < MAX_MIL ? n : MAX_MIL));
}

static void read_timeline(DWORD pptr, Player *pl)
{
    DWORD tl = u32(pptr + P_TIMELINE);
    pl->age_done_s[0] = pl->age_done_s[1] = pl->age_done_s[2] = -1;
    pl->n_samples = 0;
    if (!tl)
        return;
    int cnt = i32(tl + 0xC);
    if (cnt < 0)
        cnt = 0;
    if (cnt > MAX_SAMPLES)
        cnt = MAX_SAMPLES;
    if (cnt && rd(u32(tl), pl->samples, cnt))
        pl->n_samples = cnt;
    /* sự kiện lên đời: node {byte loại 0/1/2, int chỉ số mẫu, int giờ game ms, node kế} */
    DWORD node = u32(tl + 4);
    for (int guard = 0; node && guard < 64; guard++) {
        unsigned char ev[16];
        if (!rd(node, ev, 16))
            break;
        int type = ev[0], t_ms;
        memcpy(&t_ms, ev + 8, 4);
        memcpy(&node, ev + 12, 4);
        if (type < 3 && t_ms >= 0)
            pl->age_done_s[type] = t_ms / 1000;
    }
}

int game_read(Snapshot *s)
{
    memset(s, 0, sizeof *s);
    DWORD settings = u32(A_SETTINGS);
    DWORD w = settings ? u32(settings + S_WORLD) : 0;
    if (!w) {
        wcscpy(s->error, L"Game không ở trong trận: dữ liệu trận đã mất (người chơi đã thoát trận?).");
        return 0;
    }
    s->in_match = 1;
    s->time_ms = u32(w + W_TIME_MS);
    s->game_over = u8(w + W_GAME_OVER);
    int n = i16(w + W_PLAYER_COUNT) - 1;
    if (n < 0)
        n = 0;
    if (n > MAX_PLAYERS)
        n = MAX_PLAYERS;
    DWORD tab = u32(w + W_PLAYERS);
    DWORD comm = u32(A_COMM);
    s->me = i16(w + W_LOCAL_PLAYER);
    s->n = n;
    for (int i = 1; i <= n; i++) {
        Player *pl = &s->p[i - 1];
        DWORD p = u32(tab + 4 * i);
        pl->number = i;
        if (!p)
            continue;
        cstr_w(u32(p + P_NAME), pl->name, 48);
        pl->civ = u8(p + P_CIV);
        pl->status = u8(p + P_STATUS);
        pl->resigned = u8(p + P_RESIGNED);
        int t = u8(settings + S_TEAMS + i - 1);
        pl->team = t >= 2 ? t - 1 : 0;
        pl->comm_state = comm ? i32(comm + C_PLAYER_STATE + 4 * i) : 0;
        DWORD attrs = u32(p + P_ATTRS);
        pl->pop_now = attrs ? f32(attrs + 11 * 4) : 0;
        pl->vil_now = attrs ? f32(attrs + 37 * 4) : 0;
        read_timeline(p, pl);
        pl->bronze_click_s = -1;
        DWORD res = u32(p + P_RESEARCH);
        DWORD arr = res ? u32(res) : 0;
        if (arr) {
            pl->bronze_progress = f32(arr + TECH_BRONZE * 8);
            pl->bronze_state = i16(arr + TECH_BRONZE * 8 + 4);
            if (pl->bronze_state == 2)
                pl->bronze_click_s = (int)(s->time_ms / 1000) - (int)pl->bronze_progress;
        }
        if (pl->age_done_s[1] >= 0)
            pl->bronze_click_s = pl->age_done_s[1] - BRONZE_RESEARCH_S;
        read_military(p, pl);
    }
    return 1;
}
