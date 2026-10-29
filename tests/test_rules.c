/* Kiểm thử luật xử: python build.py test */
#include <stdio.h>
#include "../src/rules.h"

static int fails = 0;

static void mil_case(const char *name, MilCase m, int want);

static void dc(const char *name, int team, int t, long pdc, long pother, Verdict want, int want_reached)
{
    DcCase c = {team, t, pdc, pother};
    DcResult r = rules_disconnect(&c);
    int ok = r.v == want && r.reached == want_reached;
    printf("%s %-48s v=%d reached=%d share_dc=%.1f\n", ok ? "OK  " : "FAIL", name, r.v, r.reached, r.share_dc);
    fails += !ok;
}

static void mil(const char *name, int click, int done, int m1, int m2, int want)
{
    MilCase m = {click, done, m1, m2, (m1 >= 0) + (m2 >= 0), 20 * 60};
    mil_case(name, m, want);
}

static void mil_case(const char *name, MilCase m, int want)
{
    int f = rules_military(&m);
    printf("%s %-48s flags=%d\n", f == want ? "OK  " : "FAIL", name, f);
    fails += f != want;
}

int main(void)
{
    /* Solo */
    dc("solo 17:59 -> danh lai random", 0, 17 * 60 + 59, 10, 90, V_REMATCH_RANDOM, 1);
    dc("solo 18:00 60-40 dung moc -> ben be thua", 0, 18 * 60, 40, 60, V_LOWER_LOSES, 1);
    dc("solo 25:00 59-41 -> chua dat, ben lon roi", 0, 25 * 60, 59, 41, V_REMATCH_RANDOM_CIV, 0);
    dc("solo 25:00 41-59 -> chua dat, ben nho roi", 0, 25 * 60, 41, 59, V_HIGHER_CHOOSES, 0);
    dc("solo 30:00 ben roi lon 61% -> ben kia thua", 0, 30 * 60, 61, 39, V_LOWER_LOSES, 1);
    dc("solo 30:00 50-50 -> BTC", 0, 30 * 60, 50, 50, V_TIE, 0);
    dc("solo 30:00 chua co du lieu", 0, 30 * 60, 0, 0, V_NONE, 0);
    /* 4VS4 */
    dc("4v4 20:00 dat 60-40", 1, 20 * 60, 300, 200, V_LOWER_LOSES, 1);
    dc("4v4 20:00 chua dat -> ben khong roi chon", 1, 20 * 60, 250, 260, V_NOT_DC_CHOOSES, 0);
    dc("4v4 26:00 chua dat, ben lon roi", 1, 26 * 60, 260, 250, V_REMATCH_RANDOM_CIV, 0);
    dc("4v4 26:00 chua dat, ben nho roi", 1, 26 * 60, 250, 260, V_HIGHER_CHOOSES, 0);
    /* quân trước đời 3 */
    mil("khong co quan", 600, 740, -1, -1, 0);
    mil("1 quan sau khi kich, truoc khi xong", 600, 740, 650, -1, 0);
    mil("1 quan truoc khi kich", 600, 740, 590, -1, MIL_BEFORE_CLICK);
    mil("quan khi chua kich doi 3", -1, -1, 300, -1, MIL_BEFORE_CLICK);
    mil("2 quan truoc khi xong", 600, 740, 650, 700, MIL_TWO_BEFORE_DONE);
    mil("quan thu 2 dung luc xong", 600, 740, 650, 740, 0);
    mil("ca hai loi", 600, 740, 500, 700, MIL_BEFORE_CLICK | MIL_TWO_BEFORE_DONE);
    /* không rõ giờ sinh (game không lưu): chỉ biết sinh trước lúc đọc */
    mil_case("1 quan, chua kich doi 3 luc doc", (MilCase){-1, -1, -1, -1, 1, 600}, MIL_BEFORE_CLICK);
    mil_case("2 quan, dang len doi 3 luc doc", (MilCase){500, -1, -1, -1, 2, 600},
             MIL_UNSURE_CLICK | MIL_TWO_BEFORE_DONE);
    mil_case("2 quan, da len doi 3, khong ro gio", (MilCase){600, 740, -1, -1, 2, 1200},
             MIL_UNSURE_CLICK | MIL_UNSURE_DONE);
    printf("%s\n", fails ? "CO LOI" : "TAT CA DAT");
    return fails ? 1 : 0;
}
