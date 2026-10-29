/* Luật xử của giải (mục 4 và 5 trong "Luật thi đấu AoE Quần Hùng Tranh Bá 2026") dưới dạng hàm thuần,
 * không đọc game, để kiểm thử độc lập (tests/test_rules.c). */
#ifndef RULES_H
#define RULES_H

#define SOLO_MINUTE 18          /* Solo Random: mốc phút 18 */
#define TEAM_MINUTE 25          /* 4VS4 Random: mốc phút 25 */
#define BRONZE_RESEARCH_S 140   /* nghiên cứu Bronze Age (đời 3) = 140 giây game, đo trên trận thật */

typedef enum {
    V_NONE = 0,                 /* không đủ dữ liệu / không phải trận 2 phe */
    V_REMATCH_RANDOM,           /* Solo rớt trước phút 18: đánh lại random */
    V_LOWER_LOSES,              /* đạt 60-40: xử thua bên timeline bé hơn */
    V_REMATCH_RANDOM_CIV,       /* chưa đạt, bên timeline lớn hơn rớt: đấu lại random quân */
    V_HIGHER_CHOOSES,           /* chưa đạt, bên timeline nhỏ hơn rớt: bên lớn hơn chọn quân cũ hoặc random */
    V_NOT_DC_CHOOSES,           /* 4VS4 trước phút 25, chưa đạt: bên không rớt chọn giữ quân hoặc random lại */
    V_TIE                       /* chưa đạt và hai bên bằng nhau: luật không nói, BTC quyết */
} Verdict;

typedef struct {
    int team_mode;              /* 0 = Solo Random, 1 = 4VS4 Random (thể thức đồng đội) */
    int dc_time_s;              /* giờ game lúc rớt mạng (giây) */
    long pop_dc;                /* dân số timeline của phe có người rớt */
    long pop_other;             /* dân số timeline của phe còn lại */
} DcCase;

typedef struct {
    Verdict v;
    int reached;                /* 1 nếu phe lớn hơn có >= 60% */
    int lower_is_dc;            /* phe timeline bé hơn là phe rớt mạng (khi reached) */
    double share_dc;            /* % timeline của phe rớt */
} DcResult;

DcResult rules_disconnect(const DcCase *c);

/* Kiểm tra quân trước đời 3 cho một người chơi (mục 3 và 4 của luật).
 * Thời gian tính bằng giây game; -1 = chưa xảy ra (kích / lên xong) hoặc không biết (giờ sinh).
 * Game KHÔNG lưu giờ sinh của unit: với quân còn sống mà không rõ giờ sinh, chỉ biết nó sinh trước lúc đọc (now_s). */
typedef struct {
    int bronze_click_s;         /* lúc kích đời 3 */
    int bronze_done_s;          /* lúc lên xong đời 3 */
    int first_mil_s;            /* lúc sinh đơn vị quân thứ 1 */
    int second_mil_s;           /* lúc sinh đơn vị quân thứ 2 */
    int n_mil;                  /* số đơn vị quân còn sống lúc đọc */
    int now_s;                  /* giờ game lúc đọc */
} MilCase;

enum {
    MIL_OK = 0,
    MIL_BEFORE_CLICK = 1,       /* "Xuất hiện đơn vị quân trước khi kích đời 3" -> xử thua */
    MIL_TWO_BEFORE_DONE = 2,    /* "Có 02 đơn vị quân khi chưa lên xong đời 3" -> xử thua */
    MIL_UNSURE_CLICK = 4,       /* có quân nhưng không biết sinh trước hay sau lúc kích */
    MIL_UNSURE_DONE = 8         /* có >= 2 quân nhưng không biết quân thứ 2 sinh trước hay sau lúc lên xong */
};

int rules_military(const MilCase *m);   /* tổ hợp bit MIL_* */

#endif
