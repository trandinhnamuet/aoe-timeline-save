/* Báo cáo lưu cạnh file exe: .txt (đọc được bằng Notepad) và .csv (toàn bộ timeline). */
#include "app.h"
#include <stdio.h>
#include <wchar.h>

typedef struct {
    HANDLE f;
    int ok;
} Out;

static void put(Out *o, const wchar_t *fmt, ...)
{
    wchar_t w[2048];
    char u[6144];
    va_list ap;
    va_start(ap, fmt);
    vswprintf(w, 2048, fmt, ap);
    va_end(ap);
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof u, NULL, NULL);
    DWORD wr;
    if (n > 1 && o->ok)
        o->ok = WriteFile(o->f, u, (DWORD)(n - 1), &wr, NULL);
}

static int open_out(Out *o, const wchar_t *path, int bom)
{
    o->f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    o->ok = o->f != INVALID_HANDLE_VALUE;
    DWORD wr;
    if (o->ok && bom)
        o->ok = WriteFile(o->f, "\xEF\xBB\xBF", 3, &wr, NULL);
    return o->ok;
}

static int close_out(Out *o)
{
    if (o->f != INVALID_HANDLE_VALUE)
        CloseHandle(o->f);
    return o->ok;
}

static const wchar_t *status_name(const Player *p)
{
    if (p->resigned)
        return L"đầu hàng";
    return p->status == 1 ? L"thắng" : p->status == 2 ? L"thua" : L"đang chơi";
}

static void write_txt(Out *o, App *a)
{
    const Snapshot *s = &a->snap;
    const Analysis *an = &a->an;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t t[16], t2[16];
    put(o, L"AoE Timeline v%hs — báo cáo trọng tài\r\n", APP_VERSION);
    put(o, L"Lập lúc: %04d-%02d-%02d %02d:%02d:%02d (giờ máy)\r\n", st.wYear, st.wMonth, st.wDay, st.wHour,
        st.wMinute, st.wSecond);
    put(o, L"Tiến trình game: %ls (PID %lu)%ls\r\n", s->exe, (unsigned long)s->pid, a->demo ? L" — DỮ LIỆU MẪU" : L"");
    fmt_time((int)(s->time_ms / 1000), t, 16);
    put(o, L"Giờ game: %ls (%ls)\r\n\r\n", t, s->frozen ? L"game đang dừng" : L"game đang chạy");

    put(o, L"KẾT LUẬN: %ls\r\n%ls\r\n\r\n", an->title, an->detail);
    if (an->dc >= 0) {
        fmt_time(an->dc_time_s, t, 16);
        fmt_time(an->sample * TIMELINE_STEP_S, t2, 16);
        put(o, L"Người rớt mạng: %ls (%ls)\r\nGiờ rớt mạng: %ls  ·  mẫu timeline dùng để xử: #%d lúc %ls\r\n",
            s->p[an->dc].name, an->dc_auto == 1 ? L"mặc định: đối thủ của máy này" : an->dc_auto == 2 ? L"gợi ý theo trạng thái game" : L"trọng tài chọn", t, an->sample, t2);
    }
    long tot = 0;
    for (int k = 0; k < an->nsides; k++)
        tot += an->side_pop[k];
    for (int k = 0; k < an->nsides; k++)
        put(o, L"  Phe %d: %ls — %ld dân (%.1f%%)\r\n", k + 1, an->side_name[k], an->side_pop[k],
            tot ? 100.0 * an->side_pop[k] / tot : 0.0);

    put(o, L"\r\nNGƯỜI CHƠI\r\n");
    for (int i = 0; i < s->n; i++) {
        const Player *p = &s->p[i];
        wchar_t team[16];
        swprintf(team, 16, p->team ? L"đội %d" : L"không đội", p->team);
        put(o, L"  P%d %ls — %ls, %ls, %ls, dân số hiện tại %.0f, %d mẫu timeline%ls\r\n", p->number, p->name,
            civ_name(p->civ), team, status_name(p), p->pop_now, p->n_samples,
            p->number == s->me ? L" (máy này)" : L"");
    }

    put(o, L"\r\nKIỂM TRA QUÂN TRƯỚC ĐỜI 3\r\n");
    for (int i = 0; i < s->n; i++) {
        const Player *p = &s->p[i];
        wchar_t c[16], d[16], e2[16];
        fmt_time(p->bronze_click_s, c, 16);
        fmt_time(p->age_done_s[1], d, 16);
        fmt_time(p->age_done_s[0], e2, 16);
        int fl = an->mil_flags[i];
        int level;
        put(o, L"  P%d %ls: lên xong đời 2 %ls · kích đời 3 %ls · lên xong đời 3 %ls · %d quân còn sống → %ls\r\n",
            p->number, p->name, e2, c, d, p->n_mil, mil_verdict(fl, p->n_mil, &level));
        for (int k = 0; k < p->n_mil && k < MAX_MIL; k++) {
            const MilUnit *u = &p->mil[k];
            fmt_time(u->born_s, c, 16);
            put(o, L"      quân thứ %d: %ls (loại %d, lớp %d, id %d) sinh lúc %ls\r\n", k + 1, u->name, u->type,
                u->cls, u->id, c);
        }
        if (p->n_mil > MAX_MIL)
            put(o, L"      ... và %d quân khác\r\n", p->n_mil - MAX_MIL);
    }
    put(o, L"\r\nGhi chú: tỉ lệ timeline lấy từ chính dữ liệu game dùng để vẽ màn Timeline (Achievements), 15 giây game\r\n"
           L"một mẫu, mẫu #k ứng với giây k*15. Kích đời 3 = lên xong đời 3 - %d giây nghiên cứu Bronze Age.\r\n"
           L"Game không lưu giờ sinh của unit: tool chỉ thấy quân còn sống lúc đọc (xếp theo thứ tự sinh) và chỉ\r\n"
           L"kết luận vi phạm khi lúc đọc vẫn còn quân mà chưa kích / chưa lên xong đời 3.\r\n"
           L"Toàn bộ timeline: file .csv cùng tên.\r\n",
        BRONZE_RESEARCH_S);
}

static void write_csv(Out *o, App *a)
{
    const Snapshot *s = &a->snap;
    int N = 0;
    for (int i = 0; i < s->n; i++)
        if (s->p[i].n_samples > N)
            N = s->p[i].n_samples;
    put(o, L"mau,giay,thoi_gian");
    for (int i = 0; i < s->n; i++)
        put(o, L",\"P%d %ls\"", s->p[i].number, s->p[i].name);
    for (int k = 0; k < a->an.nsides; k++)
        put(o, L",\"%% phe %d\"", k + 1);
    put(o, L"\r\n");
    for (int k = 0; k < N; k++) {
        wchar_t t[16];
        fmt_time(k * TIMELINE_STEP_S, t, 16);
        put(o, L"%d,%d,%ls", k, k * TIMELINE_STEP_S, t);
        long sp[MAX_PLAYERS] = {0}, tot = 0;
        for (int i = 0; i < s->n; i++) {
            int v = k < s->p[i].n_samples ? s->p[i].samples[k] : 0;
            put(o, L",%d", v);
            sp[a->an.side_of[i]] += v;
            tot += v;
        }
        for (int j = 0; j < a->an.nsides; j++)
            put(o, L",%.1f", tot ? 100.0 * sp[j] / tot : 0.0);
        put(o, L"\r\n");
    }
}

int report_write(App *a)
{
    CreateDirectoryW(a->out_dir, NULL);
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(a->report_path, MAX_PATH, L"%ls\\AoE-Timeline_%04d-%02d-%02d_%02d-%02d-%02d", a->out_dir, st.wYear,
             st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    wchar_t path[MAX_PATH + 8];
    Out o;
    swprintf(path, MAX_PATH + 8, L"%ls.txt", a->report_path);
    if (!open_out(&o, path, 1))
        return 0;
    write_txt(&o, a);
    int ok = close_out(&o);
    swprintf(path, MAX_PATH + 8, L"%ls.csv", a->report_path);
    if (open_out(&o, path, 1)) {
        write_csv(&o, a);
        ok = close_out(&o) && ok;
    }
    return ok;
}
