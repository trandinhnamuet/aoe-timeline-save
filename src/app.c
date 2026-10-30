/* Tìm game, đọc dữ liệu, áp luật xử. */
#include "app.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const wchar_t *CIVS[] = {L"Gaia", L"Egyptian", L"Greek", L"Babylonian", L"Assyrian", L"Minoan", L"Hittite",
                                L"Phoenician", L"Sumerian", L"Persian", L"Shang", L"Yamato", L"Choson", L"Roman",
                                L"Carthaginian", L"Palmyran", L"Macedonian"};

const wchar_t *civ_name(int civ)
{
    return civ >= 0 && civ <= 16 ? CIVS[civ] : L"?";
}

void fmt_time(int s, wchar_t *out, int n)
{
    if (s < 0)
        swprintf(out, n, L"—");
    else if (s >= 3600)
        swprintf(out, n, L"%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    else
        swprintf(out, n, L"%02d:%02d", s / 60, s % 60);
}

const wchar_t *mil_verdict(int fl, int n_mil, int brief, int *level)
{
    *level = (fl & (MIL_BEFORE_CLICK | MIL_TWO_BEFORE_DONE)) ? 2 : fl ? 1 : 0;
    if ((fl & MIL_BEFORE_CLICK) && (fl & MIL_TWO_BEFORE_DONE))
        return brief ? L"VI PHẠM: cả 2 lỗi" : L"VI PHẠM: quân trước khi kích đời 3 + 2 quân trước khi lên xong";
    if (fl & MIL_BEFORE_CLICK)
        return brief ? L"VI PHẠM: quân trước kích" : L"VI PHẠM: quân trước khi kích đời 3";
    if (fl & MIL_TWO_BEFORE_DONE)
        return brief ? L"VI PHẠM: 2 quân trước xong" : L"VI PHẠM: 2 quân trước khi lên xong đời 3";
    if (fl)
        return brief ? L"Không xác định" : L"Không xác định (game không lưu giờ sinh của quân)";
    return n_mil ? L"Hợp lệ" : L"Hợp lệ (không có quân)";
}

static int sample_of(const Player *p, int k)
{
    return k >= 0 && k < p->n_samples ? p->samples[k] : 0;
}

static void side_pops(const Snapshot *s, Analysis *an, int sample)
{
    for (int k = 0; k < MAX_PLAYERS; k++)
        an->side_pop[k] = 0;
    for (int i = 0; i < s->n; i++)
        an->side_pop[an->side_of[i]] += sample_of(&s->p[i], sample);
}

static double pct(const Analysis *an, int side)
{
    long t = 0;
    for (int k = 0; k < an->nsides; k++)
        t += an->side_pop[k];
    return t > 0 ? 100.0 * an->side_pop[side] / t : 0.0;
}

static void short_side(const App *a, int side, wchar_t *out, int n)
{
    const Snapshot *s = &a->snap;
    if (!a->an.team_mode) {
        for (int i = 0; i < s->n; i++)
            if (a->an.side_of[i] == side) {
                swprintf(out, n, L"%ls", s->p[i].name);
                return;
            }
    }
    for (int i = 0; i < s->n; i++)
        if (a->an.side_of[i] == side) {
            if (s->p[i].team)
                swprintf(out, n, L"ĐỘI %d", s->p[i].team);
            else
                swprintf(out, n, L"%ls", s->p[i].name);
            return;
        }
    swprintf(out, n, L"?");
}

static void build_sides(const Snapshot *s, Analysis *an)
{
    int key[MAX_PLAYERS];
    an->nsides = 0;
    for (int i = 0; i < s->n; i++) {
        int k = s->p[i].team ? 100 + s->p[i].team : i;
        int found = -1;
        for (int j = 0; j < an->nsides; j++)
            if (key[j] == k)
                found = j;
        if (found < 0) {
            found = an->nsides++;
            key[found] = k;
            an->side_name[found][0] = 0;
        }
        an->side_of[i] = found;
        wchar_t *nm = an->side_name[found];
        if (nm[0])
            wcsncat(nm, L", ", 159 - wcslen(nm));
        else if (s->p[i].team)
            swprintf(nm, 160, L"Đội %d: ", s->p[i].team);
        wcsncat(nm, s->p[i].name, 159 - wcslen(nm));
    }
}

/* Người rớt mạng mặc định. Trạng thái thắng/thua của game KHÔNG dùng được: thử trên trận thật, vài phút sau khi
 * đối thủ rớt, game chuyển sang màn Achievements và xử thua chính máy còn lại. Tool chạy trên máy chưa rớt, nên:
 * Solo -> đối thủ của máy này; 4VS4 -> gợi ý khi đúng một người khác máy này bị xử thua mà không đầu hàng. */
static int detect_dc(const Snapshot *s, int *sure)
{
    *sure = 0;
    if (s->n == 2) {
        for (int i = 0; i < 2; i++)
            if (s->p[i].number != s->me && (s->p[1 - i].number == s->me)) {
                *sure = 1;
                return i;
            }
        return -1;
    }
    int found = -1, count = 0;
    for (int i = 0; i < s->n; i++)
        if (s->p[i].status == 2 && !s->p[i].resigned && s->p[i].number != s->me)
            found = i, count++;
    return count == 1 ? found : -1;
}

void app_analyze(App *a)
{
    Snapshot *s = &a->snap;
    Analysis *an = &a->an;
    memset(an, 0, sizeof *an);
    an->dc = an->dc_side = an->other_side = an->sample = -1;
    an->dc_time_s = -1;

    if (!s->in_match || s->n < 1) {
        swprintf(an->title, 200, L"CHƯA ĐỌC ĐƯỢC TRẬN ĐẤU");
        swprintf(an->detail, 400, L"%ls", s->error[0] ? s->error : L"Không tìm thấy game AoE đang trong trận.");
        return;
    }
    an->team_mode = s->n > 2;
    build_sides(s, an);

    for (int i = 0; i < s->n; i++) {
        const Player *p = &s->p[i];
        MilCase m = {p->bronze_click_s, p->age_done_s[1], p->n_mil > 0 ? p->mil[0].born_s : -1,
                     p->n_mil > 1 ? p->mil[1].born_s : -1, p->n_mil, (int)(s->time_ms / 1000)};
        an->mil[i] = m;
        an->mil_flags[i] = rules_military(&m);
    }

    int maxn = 0, minn = MAX_SAMPLES;
    for (int i = 0; i < s->n; i++) {
        if (s->p[i].n_samples > maxn)
            maxn = s->p[i].n_samples;
        if (s->p[i].n_samples < minn)
            minn = s->p[i].n_samples;
    }

    if (a->dc_override) {
        for (int i = 0; i < s->n; i++)
            if (s->p[i].number == a->dc_override)
                an->dc = i;
    } else {
        int sure;
        an->dc = detect_dc(s, &sure);
        an->dc_auto = an->dc >= 0 ? (sure ? 1 : 2) : 0;
    }

    /* mẫu timeline và giờ rớt mạng */
    if (a->at_override_s >= 0) {
        an->dc_time_s = a->at_override_s;
        an->sample = a->at_override_s / TIMELINE_STEP_S - 1;
    } else if (an->dc >= 0 && s->p[an->dc].n_samples < maxn - 1) {
        /* game ngừng ghi timeline của người rớt: mẫu cuối của họ là mẫu cuối trước khi rớt */
        an->sample = s->p[an->dc].n_samples - 1;
        an->dc_time_s = s->p[an->dc].n_samples * TIMELINE_STEP_S;
    } else {
        an->sample = minn - 1;       /* mẫu cuối mà mọi người chơi đều có */
        an->dc_time_s = (int)(s->time_ms / 1000);
    }
    if (an->sample > maxn - 1)
        an->sample = maxn - 1;
    side_pops(s, an, an->sample);

    if (an->nsides != 2) {
        swprintf(an->title, 200, L"KHÔNG PHẢI TRẬN 2 PHE");
        swprintf(an->detail, 400, L"Trận có %d phe; luật xử timeline chỉ áp dụng cho Solo và 4VS4.", an->nsides);
        return;
    }
    if (an->dc < 0) {
        swprintf(an->title, 200, L"CHỌN NGƯỜI BỊ RỚT MẠNG");
        swprintf(an->detail, 400, L"Game chưa ghi nhận ai rớt mạng. Bấm vào tên người bị rớt trong khung "
                                  L"TỈ LỆ DÂN SỐ để tool áp luật.");
        return;
    }
    if (an->sample < 0) {
        swprintf(an->title, 200, L"CHƯA CÓ MẪU TIMELINE");
        swprintf(an->detail, 400, L"Game ghi mẫu timeline đầu tiên ở giây thứ 15.");
        return;
    }
    an->dc_side = an->side_of[an->dc];
    an->other_side = 1 - an->dc_side;
    DcCase c = {an->team_mode, an->dc_time_s, an->side_pop[an->dc_side], an->side_pop[an->other_side]};
    an->res = rules_disconnect(&c);
    an->valid = an->res.v != V_NONE;

    wchar_t t[16], dcn[64], oth[64];
    fmt_time(an->dc_time_s, t, 16);
    short_side(a, an->dc_side, dcn, 64);
    short_side(a, an->other_side, oth, 64);
    double pd = pct(an, an->dc_side), po = pct(an, an->other_side);
    switch (an->res.v) {
    case V_REMATCH_RANDOM:
        swprintf(an->title, 200, L"ĐÁNH LẠI RANDOM");
        swprintf(an->detail, 400, L"Solo: %ls rớt mạng lúc %ls, trước phút %d → đánh lại random.", dcn, t,
                 SOLO_MINUTE);
        break;
    case V_LOWER_LOSES:
        swprintf(an->title, 200, L"XỬ THUA: %ls", an->res.lower_is_dc ? dcn : oth);
        swprintf(an->detail, 400, L"Timeline lúc rớt mạng (%ls): %ls %.1f%% – %ls %.1f%%. Đạt 60-40 → xử thua "
                                  L"bên timeline bé hơn.", t, dcn, pd, oth, po);
        break;
    case V_REMATCH_RANDOM_CIV:
        swprintf(an->title, 200, L"ĐẤU LẠI RANDOM QUÂN");
        swprintf(an->detail, 400, L"Timeline lúc %ls: %ls %.1f%% – %ls %.1f%%, chưa đạt 60-40. Bên timeline lớn hơn "
                                  L"(%ls) bị rớt mạng.", t, dcn, pd, oth, po, dcn);
        break;
    case V_HIGHER_CHOOSES:
        swprintf(an->title, 200, L"%ls CHỌN: QUÂN CŨ HOẶC RANDOM", oth);
        swprintf(an->detail, 400, L"Timeline lúc %ls: %ls %.1f%% – %ls %.1f%%, chưa đạt 60-40. Bên timeline nhỏ hơn "
                                  L"(%ls) bị rớt → %ls chọn đấu lại quân cũ hoặc random quân.", t, dcn, pd, oth, po,
                 dcn, oth);
        break;
    case V_NOT_DC_CHOOSES:
        swprintf(an->title, 200, L"%ls CHỌN: GIỮ QUÂN HOẶC RANDOM LẠI", oth);
        swprintf(an->detail, 400, L"4VS4: rớt mạng lúc %ls, trước phút %d. Timeline %ls %.1f%% – %ls %.1f%% chưa "
                                  L"đạt 60-40 → bên không bị rớt chọn.", t, TEAM_MINUTE, dcn, pd, oth, po);
        break;
    case V_TIE:
        swprintf(an->title, 200, L"BTC QUYẾT ĐỊNH");
        swprintf(an->detail, 400, L"Timeline lúc %ls bằng nhau (%.1f%% – %.1f%%); luật không quy định trường hợp này.",
                 t, pd, po);
        break;
    default:
        swprintf(an->title, 200, L"CHƯA CÓ DỮ LIỆU DÂN SỐ");
        swprintf(an->detail, 400, L"Mẫu timeline lúc %ls chưa có dân số của cả hai bên.", t);
        break;
    }
}

void app_refresh(App *a)
{
    Snapshot *s = &a->snap;
    if (a->demo) {
        app_demo(a);
        return;
    }
    memset(s, 0, sizeof *s);
    a->need_admin = 0;
    GameProc procs[16];
    int n = game_find(procs, 16);
    int chosen = -1;
    for (int i = 0; i < n && chosen < 0; i++)
        if (!a->pid_arg || procs[i].pid == a->pid_arg)
            chosen = i;
    if (chosen < 0) {
        s->in_match = 0;
        swprintf(s->error, 200, a->pid_arg ? L"Không có tiến trình AoE với PID %lu." :
                                             L"Không tìm thấy game AoE (EMPIRESX.EXE) đang chạy trên máy này.",
                 (unsigned long)a->pid_arg);
        app_analyze(a);
        return;
    }
    /* nhiều tiến trình game (hiếm): lấy tiến trình đầu tiên đang trong trận */
    for (int i = chosen; i < n; i++) {
        if (a->pid_arg && procs[i].pid != a->pid_arg)
            continue;
        int r = game_open(procs[i].pid);
        if (r < 0) {
            a->need_admin = 1;
            swprintf(s->error, 200, L"Không có quyền đọc game (game chạy bằng quyền Administrator).");
            continue;
        }
        if (r == 0)
            continue;
        unsigned t0 = game_clock();
        Sleep(1200);
        if (game_read(s)) {
            s->pid = procs[i].pid;
            wcscpy(s->exe, procs[i].exe);
            s->frozen = s->time_ms == t0;
            break;
        }
    }
    app_analyze(a);
}

void app_demo(App *a)
{
    Snapshot *s = &a->snap;
    memset(s, 0, sizeof *s);
    s->in_match = 1;
    s->pid = 0;
    wcscpy(s->exe, L"DỮ LIỆU MẪU");
    s->time_ms = (24 * 60 + 37) * 1000;
    s->frozen = 1;
    s->me = 1;
    s->n = 2;
    const wchar_t *names[2] = {L"Demo_Xanh", L"Demo_Do"};
    int civs[2] = {7, 11};
    for (int i = 0; i < 2; i++) {
        Player *p = &s->p[i];
        p->number = i + 1;
        wcscpy(p->name, names[i]);
        p->civ = civs[i];
        p->n_samples = 99;
        for (int k = 0; k < p->n_samples; k++) {
            double m = (k + 1) * 15 / 60.0;
            double v = m < 9 ? 4 + m * 2.4 : 25 + (m - 9) * (i == 0 ? 3.2 : 1.4);
            if (i == 1 && m > 19)
                v -= (m - 19) * 4.5;
            p->samples[k] = (unsigned char)(v < 3 ? 3 : v);
        }
        p->pop_now = p->samples[p->n_samples - 1];
        p->vil_now = p->pop_now - (i == 0 ? 2 : 3);
        p->age_done_s[0] = i == 0 ? 8 * 60 + 41 : 9 * 60 + 12;
        p->age_done_s[1] = i == 0 ? 13 * 60 + 25 : -1;
        p->bronze_state = i == 0 ? 3 : 2;
        p->bronze_click_s = i == 0 ? p->age_done_s[1] - BRONZE_RESEARCH_S : 23 * 60 + 10;
    }
    /* game không lưu giờ sinh của quân (born_s = -1), như dữ liệu thật */
    MilUnit m1[] = {{1201, 299, 12, -1, L"Scout"}, {1260, 37, 12, -1, L"Cavalry"}};
    MilUnit m2[] = {{1150, 73, 6, -1, L"Clubman"}, {1190, 73, 6, -1, L"Clubman"}, {1300, 4, 0, -1, L"Bowman"}};
    s->p[0].n_mil = 2;
    memcpy(s->p[0].mil, m1, sizeof m1);
    s->p[1].n_mil = 3;
    memcpy(s->p[1].mil, m2, sizeof m2);
    app_analyze(a);
}
