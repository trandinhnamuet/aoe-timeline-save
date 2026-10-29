"""Build AoE Timeline (một file exe, không cài đặt) bằng zig cc.

    pip install ziglang==0.13.0
    python build.py            -> dist/aoe-timeline.exe
    python build.py test       -> chạy kiểm thử luật xử (tests/test_rules.c)

Phiên bản lấy từ biến môi trường APP_VERSION (workflow đặt theo tag), mặc định "dev".
Build tất định (cùng mã nguồn + cùng zig = cùng SHA-256), nên ai cũng tự build lại để so với file Release được.
"""
import hashlib
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = ["src/main.c", "src/app.c", "src/game.c", "src/rules.c", "src/report.c", "src/ui.c", "assets/app.rc"]
LIBS = ["-lgdi32", "-luser32", "-lshell32", "-ladvapi32", "-lmsimg32", "-ldwmapi"]
ZIG = [sys.executable, "-m", "ziglang"]


def run(cmd):
    print(" ".join(cmd))
    subprocess.run(cmd, cwd=ROOT, check=True)


def version():
    v = os.environ.get("APP_VERSION", "dev")
    if not re.fullmatch(r"[0-9A-Za-z.\-]{1,32}", v):
        sys.exit("APP_VERSION không hợp lệ: %r" % v)
    return v


def build():
    os.makedirs(os.path.join(ROOT, "dist"), exist_ok=True)
    out = "dist/aoe-timeline.exe"
    # -s: không kèm thông tin debug (PDB mang GUID ngẫu nhiên)
    run(ZIG + ["cc", "-target", "x86-windows-gnu", "-O2", "-s", "-municode", "-Wl,--subsystem,windows", "-std=c11",
               "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-missing-field-initializers", "-Werror",
               "-DAPP_VERSION=\"%s\"" % version(),
               "-o", out] + SRC + LIBS)
    for junk in ("dist/aoe-timeline.pdb", "dist/aoe-timeline.lib"):
        if os.path.exists(os.path.join(ROOT, junk)):
            os.remove(os.path.join(ROOT, junk))
    path = os.path.join(ROOT, out)
    data = bytearray(open(path, "rb").read())
    # giờ build trong COFF header (TimeDateStamp) -> 0 để build tất định
    pe = int.from_bytes(data[0x3C:0x40], "little")
    if data[pe:pe + 4] != b"PE\0\0":
        sys.exit("không phải file PE")
    data[pe + 8:pe + 12] = b"\0\0\0\0"
    open(path, "wb").write(data)
    h = hashlib.sha256(bytes(data)).hexdigest()
    print("%s  %s" % (h, os.path.basename(out)))


def test():
    os.makedirs(os.path.join(ROOT, "dist"), exist_ok=True)
    out = "dist/test_rules.exe"
    run(ZIG + ["cc", "-target", "x86-windows-gnu", "-O1", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-o", out, "tests/test_rules.c", "src/rules.c"])
    run([os.path.join(ROOT, out)])


if __name__ == "__main__":
    test() if sys.argv[1:] == ["test"] else build()
