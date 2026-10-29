/* AoE Timeline — công cụ trọng tài.
 *
 *   aoe-timeline.exe                 tìm game AoE đang chạy, đọc, hiện kết quả, tự lưu báo cáo cạnh exe
 *   aoe-timeline.exe --pid 1234      chọn tiến trình game
 *   aoe-timeline.exe --dc 2          trọng tài chỉ định người chơi số 2 là người rớt mạng
 *   aoe-timeline.exe --at 24:30      trọng tài nhập giờ rớt mạng (giờ game)
 *   aoe-timeline.exe --out D:\BaoCao thư mục lưu báo cáo
 *   aoe-timeline.exe --report        chỉ ghi báo cáo rồi thoát (không mở cửa sổ)
 *   aoe-timeline.exe --demo --shot a.bmp   dữ liệu mẫu, lưu ảnh giao diện
 */
#include "app.h"
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int is_elevated(void)
{
    HANDLE tok;
    TOKEN_ELEVATION e = {0};
    DWORD len;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
        return 0;
    GetTokenInformation(tok, TokenElevation, &e, sizeof e, &len);
    CloseHandle(tok);
    return e.TokenIsElevated;
}

static int relaunch_elevated(void)
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    const wchar_t *args = GetCommandLineW();
    /* bỏ phần tên exe ở đầu dòng lệnh */
    if (*args == L'"') {
        args = wcschr(args + 1, L'"');
        args = args ? args + 1 : L"";
    } else {
        while (*args && *args != L' ')
            args++;
    }
    SHELLEXECUTEINFOW sei = {sizeof sei};
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.lpParameters = args;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei);
}

static int parse_time(const wchar_t *s)
{
    int a = 0, b = 0, c = 0;
    int n = swscanf(s, L"%d:%d:%d", &a, &b, &c);
    return n == 3 ? a * 3600 + b * 60 + c : n == 2 ? a * 60 + b : n == 1 ? a : -1;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, PWSTR cmd, int show)
{
    static App app;
    App *a = &app;
    a->at_override_s = -1;
    int report_only = 0;
    const wchar_t *shot = NULL;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    GetModuleFileNameW(NULL, a->out_dir, MAX_PATH);
    wchar_t *slash = wcsrchr(a->out_dir, L'\\');
    if (slash)
        *slash = 0;
    wcsncat(a->out_dir, L"\\BaoCao-AoE-Timeline", MAX_PATH - 1 - wcslen(a->out_dir));
    for (int i = 1; i < argc; i++) {
        const wchar_t *v = i + 1 < argc ? argv[i + 1] : L"";
        if (!wcscmp(argv[i], L"--pid"))
            a->pid_arg = (DWORD)wcstoul(v, NULL, 10), i++;
        else if (!wcscmp(argv[i], L"--dc"))
            a->dc_override = _wtoi(v), i++;
        else if (!wcscmp(argv[i], L"--at"))
            a->at_override_s = parse_time(v), i++;
        else if (!wcscmp(argv[i], L"--out"))
            wcsncpy(a->out_dir, v, MAX_PATH - 1), i++;
        else if (!wcscmp(argv[i], L"--shot"))
            shot = v, i++;
        else if (!wcscmp(argv[i], L"--demo"))
            a->demo = 1;
        else if (!wcscmp(argv[i], L"--report"))
            report_only = 1;
    }
    SetProcessDPIAware();

    if (a->demo)
        app_demo(a);
    else
        app_refresh(a);
    if (a->need_admin && !a->snap.in_match && !is_elevated() && !report_only && relaunch_elevated())
        return 0;

    if (shot)
        return ui_shot(a, shot, 0, 0) ? 0 : 1;
    if (!a->demo && a->snap.in_match) {
        if (report_write(a))
            swprintf(a->status, 260, L"Đã tự lưu báo cáo: %ls.txt", a->report_path);
        else
            swprintf(a->status, 260, L"!Không lưu được báo cáo vào %ls (thư mục chỉ đọc?)", a->out_dir);
    } else if (a->demo) {
        swprintf(a->status, 260, L"Đang xem DỮ LIỆU MẪU (--demo).");
    }
    if (report_only)
        return a->snap.in_match ? 0 : 2;
    return ui_run(a);
}
