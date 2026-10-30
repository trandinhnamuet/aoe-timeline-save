/* Cửa sổ kết quả, vẽ tay bằng GDI theo phong cách XArena: nền đen đỏ, viền đỏ phát sáng, chữ vàng kim. */
#include "app.h"
#include <dwmapi.h>
#include <stdio.h>
#include <wchar.h>

#define BG RGB(12, 9, 9)
#define BG_TOP RGB(38, 12, 12)
#define PANEL RGB(26, 17, 17)
#define PANEL_HI RGB(38, 22, 22)
#define LINE RGB(70, 34, 34)
#define RED RGB(198, 40, 40)
#define RED_DEEP RGB(74, 16, 16)
#define RED_GLOW RGB(120, 24, 24)
#define GOLD RGB(240, 192, 64)
#define GOLD_DIM RGB(176, 136, 48)
#define INK RGB(242, 233, 228)
#define MUTED RGB(168, 154, 149)
#define OK_GREEN RGB(92, 190, 110)
#define BAD_RED RGB(255, 92, 80)

/* màu người chơi 1..8 theo màu mặc định của AoE */
static const COLORREF PCOL[8] = {RGB(58, 110, 224), RGB(224, 64, 58), RGB(232, 208, 64), RGB(160, 112, 58),
                                 RGB(240, 138, 42), RGB(64, 176, 80), RGB(160, 160, 160), RGB(64, 200, 208)};

enum { F_TITLE, F_SUB, F_VERDICT, F_LABEL, F_BODY, F_SMALL, F_BIG, F_BTN, F_COUNT };

enum { BTN_REFRESH, BTN_SAVE, BTN_FOLDER, BTN_COUNT };
static const wchar_t *BTN_TEXT[BTN_COUNT] = {L"ĐỌC LẠI GAME", L"LƯU BÁO CÁO", L"MỞ THƯ MỤC BÁO CÁO"};

typedef struct {
    App *a;
    double s;                   /* hệ số DPI */
    HFONT font[F_COUNT];
    RECT btn[BTN_COUNT];
    RECT chip[MAX_PLAYERS];
    int nchip;
    RECT chart;                 /* vùng vẽ biểu đồ (để tính mẫu dưới chuột) */
    int hover_btn, hover_chip, hover_sample;
    int busy;
} Ui;

static Ui U;

static int S(int v) { return (int)(v * U.s + 0.5); }

static void make_fonts(void)
{
    static const int px[F_COUNT] = {26, 12, 30, 12, 14, 12, 40, 13};
    static const int wt[F_COUNT] = {FW_BOLD, FW_SEMIBOLD, FW_BOLD, FW_BOLD, FW_NORMAL, FW_NORMAL, FW_BOLD, FW_BOLD};
    for (int i = 0; i < F_COUNT; i++) {
        if (U.font[i])
            DeleteObject(U.font[i]);
        U.font[i] = CreateFontW(-S(px[i]), 0, 0, 0, wt[i], 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
}

static void fill(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r = {x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

static void vgrad(HDC dc, int x, int y, int w, int h, COLORREF a, COLORREF b)
{
    TRIVERTEX v[2] = {{x, y, (COLOR16)(GetRValue(a) << 8), (COLOR16)(GetGValue(a) << 8), (COLOR16)(GetBValue(a) << 8), 0},
                      {x + w, y + h, (COLOR16)(GetRValue(b) << 8), (COLOR16)(GetGValue(b) << 8),
                       (COLOR16)(GetBValue(b) << 8), 0}};
    GRADIENT_RECT gr = {0, 1};
    GradientFill(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

static void hgrad(HDC dc, int x, int y, int w, int h, COLORREF a, COLORREF b)
{
    TRIVERTEX v[2] = {{x, y, (COLOR16)(GetRValue(a) << 8), (COLOR16)(GetGValue(a) << 8), (COLOR16)(GetBValue(a) << 8), 0},
                      {x + w, y + h, (COLOR16)(GetRValue(b) << 8), (COLOR16)(GetGValue(b) << 8),
                       (COLOR16)(GetBValue(b) << 8), 0}};
    GRADIENT_RECT gr = {0, 1};
    GradientFill(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
}

static void frame(HDC dc, int x, int y, int w, int h, COLORREF c, int width)
{
    HPEN p = CreatePen(PS_SOLID, width, c);
    HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, x, y, x + w, y + h);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(p);
}

static void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int width, int style)
{
    HPEN p = CreatePen(style, width, c);
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, op);
    DeleteObject(p);
}

static void poly(HDC dc, POINT *pt, int n, COLORREF fillc, COLORREF pen)
{
    HPEN p = CreatePen(PS_SOLID, 1, pen);
    HBRUSH b = CreateSolidBrush(fillc);
    HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, b);
    Polygon(dc, pt, n);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(p);
    DeleteObject(b);
}

static int text(HDC dc, int f, COLORREF c, int x, int y, int w, int h, const wchar_t *s, UINT fmt)
{
    RECT r = {x, y, x + w, y + h};
    SelectObject(dc, U.font[f]);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    return DrawTextW(dc, s, -1, &r, fmt | DT_NOPREFIX);
}

static int text_w(HDC dc, int f, const wchar_t *s)
{
    SIZE sz;
    SelectObject(dc, U.font[f]);
    GetTextExtentPoint32W(dc, s, (int)wcslen(s), &sz);
    return sz.cx;
}

static COLORREF mix(COLORREF a, COLORREF b, double t)
{
    return RGB((int)(GetRValue(a) * (1 - t) + GetRValue(b) * t), (int)(GetGValue(a) * (1 - t) + GetGValue(b) * t),
               (int)(GetBValue(a) * (1 - t) + GetBValue(b) * t));
}

/* khung thẻ: nền tối, viền đỏ mảnh, nhãn vàng in hoa có gạch đỏ bên trái */
static void card(HDC dc, int x, int y, int w, int h, const wchar_t *label)
{
    fill(dc, x, y, w, h, PANEL);
    frame(dc, x, y, w, h, LINE, 1);
    fill(dc, x, y, S(3), h, RED_DEEP);
    fill(dc, x + S(16), y + S(16), S(4), S(14), RED);
    text(dc, F_LABEL, GOLD, x + S(26), y + S(14), w - S(40), S(20), label, DT_LEFT | DT_SINGLELINE);
}

/* nút kiểu XArena: hình lục giác dẹt, nền đỏ đậm, viền sáng */
static void button(HDC dc, RECT r, const wchar_t *label, int hover)
{
    int c = S(10), my = (r.top + r.bottom) / 2;
    POINT pt[6] = {{r.left + c, r.top}, {r.right - c, r.top}, {r.right, my},
                   {r.right - c, r.bottom}, {r.left + c, r.bottom}, {r.left, my}};
    poly(dc, pt, 6, hover ? RGB(110, 22, 22) : RED_DEEP, hover ? GOLD : RGB(200, 150, 150));
    text(dc, F_BTN, hover ? GOLD : INK, r.left, r.top, r.right - r.left, r.bottom - r.top, label,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void header(HDC dc, int W)
{
    App *a = U.a;
    vgrad(dc, 0, 0, W, S(76), BG_TOP, BG);
    text(dc, F_TITLE, GOLD, S(24), S(10), S(600), S(36), L"AOE TIMELINE", DT_LEFT | DT_SINGLELINE);
    text(dc, F_SUB, RGB(214, 120, 110), S(26), S(46), S(600), S(20),
         L"CÔNG CỤ TRỌNG TÀI  ·  XỬ DISCONNECT & KIỂM TRA QUÂN TRƯỚC ĐỜI 3", DT_LEFT | DT_SINGLELINE);
    /* đường vàng + hoạ tiết giữa, như thanh trang trí của XArena */
    int y = S(75);
    hgrad(dc, 0, y, W / 2, S(1), BG, GOLD_DIM);
    hgrad(dc, W / 2, y, W / 2, S(1), GOLD_DIM, BG);
    POINT d[4] = {{W / 2, y - S(5)}, {W / 2 + S(6), y}, {W / 2, y + S(5)}, {W / 2 - S(6), y}};
    poly(dc, d, 4, GOLD, GOLD);

    wchar_t l1[200], l2[300], t[16];
    const Snapshot *s = &a->snap;
    if (s->in_match) {
        fmt_time((int)(s->time_ms / 1000), t, 16);
        swprintf(l1, 200, L"Giờ game %ls  ·  %ls", t,
                 a->demo ? L"DỮ LIỆU MẪU" : s->frozen ? L"game đang dừng" : L"game đang chạy");
    } else {
        swprintf(l1, 200, L"Chưa đọc được trận");
    }
    const wchar_t *exe = wcsrchr(s->exe, L'\\');
    swprintf(l2, 300, L"%ls  ·  PID %lu  ·  v%hs", exe ? exe + 1 : (s->exe[0] ? s->exe : L"—"),
             (unsigned long)s->pid, APP_VERSION);
    text(dc, F_BODY, INK, W - S(624), S(16), S(600), S(22), l1, DT_RIGHT | DT_SINGLELINE);
    text(dc, F_SMALL, MUTED, W - S(624), S(42), S(600), S(20), l2, DT_RIGHT | DT_SINGLELINE);
}

static void verdict(HDC dc, int x, int y, int w, int h)
{
    const Analysis *an = &U.a->an;
    COLORREF edge = an->valid ? RED : (U.a->snap.in_match ? GOLD_DIM : LINE);
    /* viền phát sáng: vài lớp viền nhạt dần */
    frame(dc, x - S(3), y - S(3), w + S(6), h + S(6), mix(BG, edge, 0.18), S(2));
    frame(dc, x - S(1), y - S(1), w + S(2), h + S(2), mix(BG, edge, 0.45), S(2));
    hgrad(dc, x, y, w, h, an->valid ? RGB(70, 14, 14) : RGB(40, 26, 14), RGB(20, 10, 10));
    frame(dc, x, y, w, h, edge, S(2));
    text(dc, F_LABEL, GOLD, x + S(22), y + S(12), S(300), S(18), L"KẾT LUẬN THEO LUẬT", DT_LEFT | DT_SINGLELINE);
    text(dc, F_VERDICT, INK, x + S(20), y + S(30), w - S(40), S(44), an->title,
         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    text(dc, F_BODY, RGB(232, 220, 214), x + S(22), y + S(74), w - S(44), h - S(78), an->detail,
         DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
}

static int chart_max_samples(const Snapshot *s)
{
    int m = 0;
    for (int i = 0; i < s->n; i++)
        if (s->p[i].n_samples > m)
            m = s->p[i].n_samples;
    return m;
}

static void chart(HDC dc, int x, int y, int w, int h)
{
    card(dc, x, y, w, h, L"TIMELINE DÂN SỐ = DÂN + QUÂN (GAME GHI 15 GIÂY MỘT MẪU)");
    const Snapshot *s = &U.a->snap;
    const Analysis *an = &U.a->an;
    int cx = x + S(54), cy = y + S(46), cw = w - S(76), ch = h - S(84);
    SetRect(&U.chart, cx, cy, cx + cw, cy + ch);
    fill(dc, cx, cy, cw, ch, RGB(16, 11, 11));
    int N = chart_max_samples(s);
    if (N < 2) {
        text(dc, F_BODY, MUTED, cx, cy, cw, ch, L"Chưa có dữ liệu timeline", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    /* thứ tự xếp chồng: theo phe để mỗi phe là một dải liền */
    int order[MAX_PLAYERS], no = 0;
    for (int sd = 0; sd < an->nsides; sd++)
        for (int i = 0; i < s->n; i++)
            if (an->side_of[i] == sd)
                order[no++] = i;
    static double cum[MAX_PLAYERS + 1][MAX_SAMPLES];
    for (int k = 0; k < N; k++) {
        double tot = 0;
        for (int i = 0; i < s->n; i++)
            tot += k < s->p[i].n_samples ? s->p[i].samples[k] : 0;
        cum[0][k] = 0;
        for (int j = 0; j < no; j++) {
            const Player *p = &s->p[order[j]];
            double v = (tot > 0 && k < p->n_samples) ? p->samples[k] / tot : 0;
            cum[j + 1][k] = cum[j][k] + v;
        }
    }
    /* nhiều mẫu hơn số điểm ảnh: gộp, mỗi điểm ảnh tối đa một điểm */
    int step = N > cw ? (N + cw - 1) / cw : 1;
    int npt = (N - 1) / step + 1;
    POINT *pt = (POINT *)HeapAlloc(GetProcessHeap(), 0, sizeof(POINT) * (2 * npt + 4));
    for (int j = 0; j < no; j++) {
        int m = 0;
        for (int k = 0; k < N; k += step)
            pt[m++] = (POINT){cx + (int)((double)k / (N - 1) * cw), cy + ch - (int)(cum[j + 1][k] * ch)};
        for (int k = (N - 1) / step * step; k >= 0; k -= step)
            pt[m++] = (POINT){cx + (int)((double)k / (N - 1) * cw), cy + ch - (int)(cum[j][k] * ch)};
        COLORREF c = PCOL[(s->p[order[j]].number - 1) & 7];
        poly(dc, pt, m, mix(BG, c, 0.62), mix(BG, c, 0.9));
    }
    HeapFree(GetProcessHeap(), 0, pt);
    /* mốc 60 / 40 */
    for (int q = 0; q < 2; q++) {
        int yy = cy + ch - (int)((q ? 0.4 : 0.6) * ch);
        line(dc, cx, yy, cx + cw, yy, GOLD, 1, PS_DOT);
        text(dc, F_SMALL, GOLD, x + S(8), yy - S(8), S(40), S(16), q ? L"40%" : L"60%", DT_RIGHT | DT_SINGLELINE);
    }
    text(dc, F_SMALL, MUTED, x + S(8), cy - S(8), S(40), S(16), L"100%", DT_RIGHT | DT_SINGLELINE);
    text(dc, F_SMALL, MUTED, x + S(8), cy + ch - S(8), S(40), S(16), L"0%", DT_RIGHT | DT_SINGLELINE);
    /* trục thời gian: nhãn mỗi 5 phút (hoặc 10 phút với trận dài) */
    int total_s = (N - 1) * TIMELINE_STEP_S, every = total_s > 3600 ? 600 : 300;
    for (int t = every; t <= total_s; t += every) {
        int xx = cx + (int)((double)t / total_s * cw);
        line(dc, xx, cy + ch, xx, cy + ch + S(4), MUTED, 1, PS_SOLID);
        wchar_t lb[16];
        swprintf(lb, 16, L"%d'", t / 60);
        text(dc, F_SMALL, MUTED, xx - S(30), cy + ch + S(5), S(60), S(16), lb, DT_CENTER | DT_SINGLELINE);
    }
    /* mẫu dùng để xử */
    if (an->sample >= 0 && an->dc >= 0) {
        int xx = cx + (int)((double)an->sample / (N - 1) * cw);
        line(dc, xx, cy, xx, cy + ch, BAD_RED, S(2), PS_SOLID);
        wchar_t lb[48], t[16];
        fmt_time(an->sample * TIMELINE_STEP_S, t, 16);
        swprintf(lb, 48, L"MẪU XỬ %ls", t);
        int tw = text_w(dc, F_LABEL, lb) + S(12);
        int lx = xx + S(4) + tw > cx + cw ? xx - S(4) - tw : xx + S(4);
        fill(dc, lx, cy + S(4), tw, S(18), RGB(90, 16, 16));
        text(dc, F_LABEL, INK, lx, cy + S(4), tw, S(18), lb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    /* chú thích */
    int lx = cx;
    for (int j = 0; j < no; j++) {
        const Player *p = &s->p[order[j]];
        fill(dc, lx, y + h - S(22), S(10), S(10), PCOL[(p->number - 1) & 7]);
        int tw = text_w(dc, F_SMALL, p->name);
        text(dc, F_SMALL, INK, lx + S(14), y + h - S(26), tw + S(4), S(18), p->name, DT_LEFT | DT_SINGLELINE);
        lx += tw + S(30);
    }
    /* chuột: đường dọc + bảng tỉ lệ tại mẫu đó */
    int k = U.hover_sample;
    if (k >= 0 && k < N && an->nsides >= 1) {
        int xx = cx + (int)((double)k / (N - 1) * cw);
        line(dc, xx, cy, xx, cy + ch, INK, 1, PS_DOT);
        long sp[MAX_PLAYERS] = {0}, tot = 0;
        for (int i = 0; i < s->n; i++) {
            int v = k < s->p[i].n_samples ? s->p[i].samples[k] : 0;
            sp[an->side_of[i]] += v;
            tot += v;
        }
        wchar_t buf[600], t[16];
        fmt_time(k * TIMELINE_STEP_S, t, 16);
        int len = swprintf(buf, 600, L"%ls", t);
        for (int sd = 0; sd < an->nsides && len < 500; sd++)
            len += swprintf(buf + len, 600 - len, L"\n%.40ls: %ld (%.1f%%)", an->side_name[sd], sp[sd],
                            tot ? 100.0 * sp[sd] / tot : 0.0);
        int bw = S(260), bh = S(22) + an->nsides * S(18);
        int bx = xx + S(10) + bw > cx + cw ? xx - S(10) - bw : xx + S(10);
        fill(dc, bx, cy + S(28), bw, bh, RGB(30, 18, 18));
        frame(dc, bx, cy + S(28), bw, bh, GOLD_DIM, 1);
        text(dc, F_SMALL, INK, bx + S(8), cy + S(32), bw - S(16), bh - S(6), buf, DT_LEFT);
    }
}

static void shares(HDC dc, int x, int y, int w, int h)
{
    card(dc, x, y, w, h, L"TỈ LỆ DÂN SỐ LÚC RỚT MẠNG");
    const Snapshot *s = &U.a->snap;
    const Analysis *an = &U.a->an;
    long tot = 0;
    for (int sd = 0; sd < an->nsides; sd++)
        tot += an->side_pop[sd];
    int yy = y + S(44);
    int show = an->nsides < 4 ? an->nsides : 4;
    int rowh = an->nsides <= 2 ? S(86) : S(44);
    for (int sd = 0; sd < show; sd++) {
        double pc = tot ? 100.0 * an->side_pop[sd] / tot : 0;
        int first = -1, vil = 0, mil = 0;
        for (int i = 0; i < s->n; i++)
            if (an->side_of[i] == sd) {
                if (first < 0)
                    first = i;
                vil += (int)(s->p[i].vil_now + 0.5f);
                mil += s->p[i].n_mil;
            }
        COLORREF c = first >= 0 ? PCOL[(s->p[first].number - 1) & 7] : MUTED;
        wchar_t big[24], name[200], small[200];
        swprintf(big, 24, L"%.1f%%", pc);
        swprintf(name, 200, L"%ls%ls", an->side_name[sd], sd == an->dc_side ? L"  (RỚT MẠNG)" : L"");
        /* timeline = tổng dân số (dân + quân); kèm số dân / quân lúc đọc để không hiểu nhầm */
        if (an->nsides <= 2)
            swprintf(small, 200, L"Dân số %ld (dân + quân)\nLúc đọc: %d dân · %d quân", an->side_pop[sd], vil, mil);
        else
            swprintf(small, 200, L"Dân số %ld (dân + quân)", an->side_pop[sd]);
        int bigf = an->nsides <= 2 ? F_BIG : F_VERDICT;
        text(dc, bigf, sd == an->dc_side ? BAD_RED : INK, x + S(20), yy, S(170), rowh - S(16), big,
             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        text(dc, F_BTN, INK, x + S(176), yy + S(4), w - S(196), S(18), name,
             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        text(dc, F_SMALL, MUTED, x + S(176), yy + S(24), w - S(196), rowh - S(50), small, DT_LEFT | DT_END_ELLIPSIS);
        int bx = x + S(176), bw = w - S(196), by = yy + rowh - S(26);
        fill(dc, bx, by, bw, S(8), RGB(46, 30, 30));
        fill(dc, bx, by, (int)(bw * pc / 100), S(8), c);
        yy += rowh;
    }
    /* chọn người rớt mạng */
    yy += S(4);
    line(dc, x + S(16), yy, x + w - S(16), yy, LINE, 1, PS_SOLID);
    text(dc, F_LABEL, GOLD, x + S(20), yy + S(8), w - S(40), S(18),
         U.a->an.dc_auto == 1 ? L"NGƯỜI RỚT MẠNG: ĐỐI THỦ CỦA MÁY NÀY — BẤM ĐỂ ĐỔI" :
         U.a->an.dc_auto == 2 ? L"NGƯỜI RỚT MẠNG (GỢI Ý) — BẤM ĐỂ ĐỔI" : L"NGƯỜI RỚT MẠNG — BẤM ĐỂ CHỌN",
         DT_LEFT | DT_SINGLELINE);
    int cxp = x + S(20), cyp = yy + S(32);
    U.nchip = 0;
    for (int i = 0; i < s->n; i++) {
        const Player *p = &s->p[i];
        int tw = text_w(dc, F_BTN, p->name) + S(30);
        if (cxp + tw > x + w - S(16)) {
            cxp = x + S(20);
            cyp += S(32);
        }
        RECT r = {cxp, cyp, cxp + tw, cyp + S(26)};
        U.chip[U.nchip++] = r;
        int sel = an->dc == i, hov = U.hover_chip == i;
        fill(dc, r.left, r.top, tw, S(26), sel ? RGB(150, 26, 26) : hov ? PANEL_HI : RGB(20, 13, 13));
        frame(dc, r.left, r.top, tw, S(26), sel ? GOLD : hov ? RGB(200, 150, 150) : LINE, 1);
        fill(dc, r.left + S(8), r.top + S(9), S(8), S(8), PCOL[(p->number - 1) & 7]);
        text(dc, F_BTN, sel ? GOLD : INK, r.left + S(20), r.top, tw - S(24), S(26), p->name,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        cxp += tw + S(8);
    }
    wchar_t info[200], t1[16], t2[16];
    fmt_time(an->sample >= 0 ? an->sample * TIMELINE_STEP_S : -1, t1, 16);
    fmt_time(an->dc_time_s, t2, 16);
    swprintf(info, 200, L"Giờ rớt mạng %ls  ·  mẫu timeline xử: %ls", t2, t1);
    text(dc, F_SMALL, MUTED, x + S(20), y + h - S(28), w - S(40), S(18), info, DT_LEFT | DT_SINGLELINE);
}

static void mil_cell(const MilUnit *u, wchar_t *out, int n)
{
    wchar_t t[16];
    if (!u) {
        swprintf(out, n, L"—");
        return;
    }
    if (u->born_s >= 0) {
        fmt_time(u->born_s, t, 16);
        swprintf(out, n, L"%ls  %ls", t, u->name);
    } else {
        swprintf(out, n, L"%ls · không rõ giờ", u->name);
    }
}

static void military(HDC dc, int x, int y, int w, int h)
{
    card(dc, x, y, w, h, L"KIỂM TRA QUÂN TRƯỚC ĐỜI 3 (MỤC 4 CỦA LUẬT)");
    const Snapshot *s = &U.a->snap;
    const Analysis *an = &U.a->an;
    static const wchar_t *COLS[] = {L"NGƯỜI CHƠI", L"KÍCH ĐỜI 3", L"LÊN XONG ĐỜI 3", L"QUÂN THỨ 1", L"QUÂN THỨ 2",
                                    L"KẾT LUẬN"};
    static const int CW[] = {17, 11, 12, 18, 18, 24};   /* % độ rộng */
    int cols[7], tx = x + S(20), tw = w - S(40);
    cols[0] = tx;
    for (int c = 0; c < 6; c++)
        cols[c + 1] = cols[c] + tw * CW[c] / 100;
    int yy = y + S(42);
    fill(dc, tx, yy, tw, S(24), RGB(34, 20, 20));
    for (int c = 0; c < 6; c++)
        text(dc, F_LABEL, GOLD_DIM, cols[c] + S(8), yy, cols[c + 1] - cols[c] - S(8), S(24), COLS[c],
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    yy += S(26);
    for (int i = 0; i < s->n; i++) {
        const Player *p = &s->p[i];
        int fl = an->mil_flags[i];
        if (i & 1)
            fill(dc, tx, yy, tw, S(28), RGB(22, 14, 14));
        wchar_t c0[80], c1[40], c2[40], c3[80], c4[80], c5[120];
        swprintf(c0, 80, L"%ls", p->name);
        fmt_time(p->bronze_click_s, c1, 40);
        if (p->bronze_click_s >= 0 && p->age_done_s[1] < 0 && p->bronze_state == 2)
            wcsncat(c1, L" (đang lên)", 39 - wcslen(c1));
        fmt_time(p->age_done_s[1], c2, 40);
        mil_cell(p->n_mil > 0 ? &p->mil[0] : NULL, c3, 80);
        mil_cell(p->n_mil > 1 ? &p->mil[1] : NULL, c4, 80);
        int level;
        int len = swprintf(c5, 120, L"%ls", mil_verdict(fl, p->n_mil, 1, &level));
        if (level < 2 && p->n_mil > 0 && len > 0)
            swprintf(c5 + len, 120 - len, L" · %d quân", p->n_mil);
        const wchar_t *cells[6] = {c0, c1, c2, c3, c4, c5};
        for (int c = 0; c < 6; c++) {
            COLORREF col = INK;
            if (c == 3 && (fl & MIL_BEFORE_CLICK))
                col = BAD_RED;
            if (c == 4 && (fl & MIL_TWO_BEFORE_DONE))
                col = BAD_RED;
            if (c == 5)
                col = level == 2 ? BAD_RED : level == 1 ? GOLD : OK_GREEN;
            if (c == 0)
                fill(dc, cols[0] + S(8), yy + S(10), S(8), S(8), PCOL[(p->number - 1) & 7]);
            text(dc, c == 5 && fl ? F_BTN : F_BODY, col, cols[c] + S(c == 0 ? 22 : 8), yy, cols[c + 1] - cols[c] - S(12),
                 S(28), cells[c], DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        yy += S(28);
    }
    text(dc, F_SMALL, MUTED, tx, y + h - S(26), tw, S(18),
         L"Kích đời 3 = lên xong − 140 giây nghiên cứu Bronze. Game không lưu giờ sinh của quân: chỉ kết luận chắc "
         L"chắn khi lúc đọc vẫn còn quân mà chưa kích / chưa lên xong đời 3.",
         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static int layout_height(void)
{
    int n = U.a->snap.n > 2 ? U.a->snap.n : 2;
    return 76 + 16 + 116 + 18 + 330 + 16 + (104 + 28 * n) + 70;
}

static void paint(HDC dc, int W, int H)
{
    fill(dc, 0, 0, W, H, BG);
    header(dc, W);
    int m = S(24), y = S(76 + 18);
    verdict(dc, m, y, W - 2 * m, S(112));
    y += S(112 + 18);
    int left = (W - 2 * m - S(16)) * 60 / 100;
    chart(dc, m, y, left, S(330));
    shares(dc, m + left + S(16), y, W - 2 * m - left - S(16), S(330));
    y += S(330 + 16);
    int n = U.a->snap.n > 2 ? U.a->snap.n : 2;
    military(dc, m, y, W - 2 * m, S(104 + 28 * n));
    /* chân trang */
    int fy = H - S(56);
    fill(dc, 0, fy - S(1), W, S(1), LINE);
    int bx = W - m;
    for (int b = BTN_COUNT - 1; b >= 0; b--) {
        int bw = text_w(dc, F_BTN, BTN_TEXT[b]) + S(48);
        SetRect(&U.btn[b], bx - bw, fy + S(12), bx, fy + S(44));
        button(dc, U.btn[b], BTN_TEXT[b], U.hover_btn == b);
        bx -= bw + S(12);
    }
    text(dc, F_SMALL, U.a->status[0] == L'!' ? BAD_RED : MUTED, m, fy + S(12), bx - m - S(12), S(32),
         U.busy ? L"Đang đọc game…" : U.a->status[0] == L'!' ? U.a->status + 1 : U.a->status,
         DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
}

static void save_report(HWND hwnd);

static int hit(RECT *r, int n, int x, int y)
{
    POINT p = {x, y};
    for (int i = 0; i < n; i++)
        if (PtInRect(&r[i], p))
            return i;
    return -1;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        paint(mem, rc.right, rc.bottom);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int hb = hit(U.btn, BTN_COUNT, x, y), hc = hit(U.chip, U.nchip, x, y), hs = -1;
        POINT p = {x, y};
        int N = chart_max_samples(&U.a->snap);
        if (PtInRect(&U.chart, p) && N > 1)
            hs = (int)((double)(x - U.chart.left) / (U.chart.right - U.chart.left) * (N - 1) + 0.5);
        if (hb != U.hover_btn || hc != U.hover_chip || hs != U.hover_sample) {
            U.hover_btn = hb;
            U.hover_chip = hc;
            U.hover_sample = hs;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT t = {sizeof t, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&t);
        SetCursor(LoadCursor(NULL, (hb >= 0 || hc >= 0) ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_MOUSELEAVE:
        U.hover_btn = U.hover_chip = U.hover_sample = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT)
            return TRUE;
        break;
    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int c = hit(U.chip, U.nchip, x, y);
        if (c >= 0) {
            App *a = U.a;
            int num = a->snap.p[c].number;
            a->dc_override = (a->an.dc == c && !a->an.dc_auto) ? 0 : num;
            app_analyze(a);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        switch (hit(U.btn, BTN_COUNT, x, y)) {
        case BTN_REFRESH:
            U.busy = 1;
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            SetCursor(LoadCursor(NULL, IDC_WAIT));
            app_refresh(U.a);
            U.busy = 0;
            swprintf(U.a->status, 260, L"Đã đọc lại game.");
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case BTN_SAVE:
            save_report(hwnd);
            break;
        case BTN_FOLDER:
            CreateDirectoryW(U.a->out_dir, NULL);
            ShellExecuteW(hwnd, L"open", U.a->out_dir, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        return 0;
    }
    case WM_DPICHANGED: {
        U.s = HIWORD(wp) / 96.0;
        make_fonts();
        RECT *r = (RECT *)lp;
        SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* lưu ảnh màn hình kết quả (BMP 24 bit) */
static int save_bmp(const wchar_t *path, int W, int H)
{
    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    int ok = 0;
    if (bmp) {
        HGDIOBJ old = SelectObject(mem, bmp);
        int hs = U.hover_sample, hb = U.hover_btn, hc = U.hover_chip;
        U.hover_sample = U.hover_btn = U.hover_chip = -1;
        paint(mem, W, H);
        U.hover_sample = hs;
        U.hover_btn = hb;
        U.hover_chip = hc;
        GdiFlush();
        DWORD stride = ((DWORD)W * 3 + 3) & ~3u, size = stride * H;
        BITMAPFILEHEADER fh = {0x4D42, (DWORD)(sizeof fh + sizeof bi.bmiHeader + size), 0, 0,
                               (DWORD)(sizeof fh + sizeof bi.bmiHeader)};
        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD wr;
            ok = WriteFile(f, &fh, sizeof fh, &wr, NULL) && WriteFile(f, &bi.bmiHeader, sizeof bi.bmiHeader, &wr, NULL) &&
                 WriteFile(f, bits, size, &wr, NULL);
            CloseHandle(f);
        }
        SelectObject(mem, old);
        DeleteObject(bmp);
    }
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    return ok;
}

static void save_report(HWND hwnd)
{
    App *a = U.a;
    if (!report_write(a)) {
        swprintf(a->status, 260, L"!Không lưu được báo cáo vào %ls", a->out_dir);
    } else {
        wchar_t bmp[MAX_PATH];
        swprintf(bmp, MAX_PATH, L"%ls.bmp", a->report_path);
        RECT rc;
        GetClientRect(hwnd, &rc);
        save_bmp(bmp, rc.right, rc.bottom);
        swprintf(a->status, 260, L"Đã lưu báo cáo: %ls.txt (+ .csv, .bmp)", a->report_path);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static HWND create_window(int show)
{
    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG);
    wc.lpszClassName = L"AoeTimelineWnd";
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));   /* assets/app.rc */
    wc.hIconSm = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassExW(&wc);
    HDC sdc = GetDC(NULL);
    U.s = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0;
    ReleaseDC(NULL, sdc);
    make_fonts();
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, S(1200), S(layout_height())};
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AoE Timeline — công cụ trọng tài", style, CW_USEDEFAULT,
                                CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    COLORREF cap = RGB(38, 12, 12);
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR, Windows 11 */, &cap, sizeof cap);
    if (show)
        ShowWindow(hwnd, SW_SHOWNORMAL);
    return hwnd;
}

int ui_run(App *a)
{
    U.a = a;
    U.hover_btn = U.hover_chip = U.hover_sample = -1;
    HWND hwnd = create_window(1);
    SetForegroundWindow(hwnd);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}

int ui_shot(App *a, const wchar_t *bmp_path, int w, int h)
{
    U.a = a;
    U.hover_btn = U.hover_chip = U.hover_sample = -1;
    U.s = 1.0;
    make_fonts();
    if (w <= 0)
        w = 1200;
    if (h <= 0)
        h = layout_height();
    return save_bmp(bmp_path, w, h);
}
