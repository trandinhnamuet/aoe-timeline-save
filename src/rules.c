#include "rules.h"

DcResult rules_disconnect(const DcCase *c)
{
    DcResult r = {V_NONE, 0, 0, 0.0};
    long total = c->pop_dc + c->pop_other;
    if (total <= 0)
        return r;
    r.share_dc = 100.0 * (double)c->pop_dc / (double)total;
    /* đạt 60-40 khi phe lớn hơn >= 60% tổng (so số nguyên để không lệch vì làm tròn) */
    long hi = c->pop_dc > c->pop_other ? c->pop_dc : c->pop_other;
    r.reached = 10 * hi >= 6 * total;
    r.lower_is_dc = c->pop_dc < c->pop_other;

    int minute = c->team_mode ? TEAM_MINUTE : SOLO_MINUTE;
    int early = c->dc_time_s < minute * 60;

    if (!c->team_mode && early) {
        r.v = V_REMATCH_RANDOM;
        return r;
    }
    if (r.reached) {
        r.v = V_LOWER_LOSES;
        return r;
    }
    if (c->team_mode && early) {
        r.v = V_NOT_DC_CHOOSES;
        return r;
    }
    if (c->pop_dc == c->pop_other)
        r.v = V_TIE;
    else if (c->pop_dc > c->pop_other)
        r.v = V_REMATCH_RANDOM_CIV;
    else
        r.v = V_HIGHER_CHOOSES;
    return r;
}

/* 1 = chắc chắn sinh trước mốc, 0 = chắc chắn không, -1 = không xác định.
 * mốc < 0: chưa xảy ra lúc đọc -> mọi quân đang có đều sinh trước mốc. */
static int born_before(int born_s, int present, int now_s, int limit_s)
{
    if (!present)
        return 0;
    if (limit_s < 0)
        return 1;
    if (born_s >= 0)
        return born_s < limit_s;
    return now_s < limit_s ? 1 : -1;
}

int rules_military(const MilCase *m)
{
    int f = 0;
    int a = born_before(m->first_mil_s, m->n_mil >= 1 || m->first_mil_s >= 0, m->now_s, m->bronze_click_s);
    int b = born_before(m->second_mil_s, m->n_mil >= 2 || m->second_mil_s >= 0, m->now_s, m->bronze_done_s);
    f |= a == 1 ? MIL_BEFORE_CLICK : a < 0 ? MIL_UNSURE_CLICK : 0;
    f |= b == 1 ? MIL_TWO_BEFORE_DONE : b < 0 ? MIL_UNSURE_DONE : 0;
    return f;
}
