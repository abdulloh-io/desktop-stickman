#!/usr/bin/env python3
"""Generate the JS pose cases for the browser preview straight from the C++
ComputeAngles switch, so the HTML demo uses exactly the same angles as the exe."""
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "main.cpp")

ACTS = [("CARD_GAME", "cards"), ("WATCH_GAME", "watch"), ("COOK", "cook"), ("QUEUE", "queue"),
        ("LAB", "lab"), ("ARMWRESTLE", "arm"), ("TUGWAR", "tug"), ("HANDSHAKE", "hand"),
        ("SPY_CHAT", "spy"), ("LAUGH", "laugh"), ("FACEPALM", "facepalm"), ("YAWN", "yawn"),
        ("DANCE", "dance"), ("CURSOR_PLAY", "cplay"), ("HANG_CURSOR", "hang"),
        ("PETTED", "pat"), ("ASK_ATTENTION", "ask"), ("TEA_RUN", "tea"),
        ("CRACK_PRANK", "crack"), ("MUSTACHE", "moust"), ("BOX_KICK", "kickbox"),
        ("SQUISH", "squish"), ("EXTINGUISH", "snow1"), ("IN_VIDEO", "invideo"),
        ("PUSH_WINDOW", "push"), ("AIR_TYPE", "airtype"),
        # --- v4: pointer games, notes, drawn mischief ---
        ("CHASE_CURSOR", "chase"), ("RIDE_CURSOR", "ride"), ("FIGHT_CURSOR", "fight"),
        ("CURSOR_THEFT", "theft"), ("SPEAK", "speak"), ("SEND_GUARD", "guard"),
        ("HIDE_ICON", "hideicon"), ("ICON_KICK", "kickicon"), ("BRING_FOLDER", "folder"),
        ("TAB_OOPS", "taboops"), ("DRAG_COUCH", "couch"), ("PLUG_IN", "plugin"),
        ("UMBRELLA", "umbrella"), ("PASS_BALL", "pass")]

# acts that share another act's pose through a fall-through label
ALIAS = {"ICON_KICK": "BOX_KICK", "PASS_BALL": "BOX_KICK", "BRING_FOLDER": "TEA_RUN",
         "DRAG_COUCH": "TEA_RUN", "FIGHT_CURSOR": "CRACK_PRANK"}


def to_js(expr):
    e = expr
    e = re.sub(r"//.*", "", e)
    e = e.replace("std::sin(", "Math.sin(").replace("std::cos(", "Math.cos(")
    e = e.replace("std::fabs(", "Math.abs(").replace("std::abs(", "Math.abs(")
    e = e.replace("std::max(", "Math.max(").replace("std::min(", "Math.min(")
    e = e.replace("LerpF(", "lerp(").replace("ClampF(", "clamp(")
    e = e.replace("Osc(", "osc(").replace("Bounce(", "bounce(")
    e = e.replace("kSitHipRaise", "SITH")
    e = e.replace("PI_F", "Math.PI")
    e = e.replace("(float)s.facing", "s.face").replace("(float)", "")
    e = e.replace("s.actPhase", "s.prop").replace("s.propT", "s.prop")
    e = e.replace("s.animTime", "s.anim").replace("s.keyAnim", "s.key")
    e = e.replace("s.grabAngle", "s.grabA").replace("s.pet", "s.pet")
    e = e.replace("s.actTimer", "s.timer").replace("s.speechT", "s.prop")
    e = e.replace("s.floatT", "s.prop").replace("g_app.typingPulse", "s.key")
    e = e.replace("s.speechSeen", "s.prop")
    e = re.sub(r"(\d\.\d+)f\b", r"\1", e)
    e = re.sub(r"\bf\b(?=[;,)\s])", "", e)
    return e.strip()


def main():
    src = open(SRC, encoding="utf-8").read()
    out = []
    for cname, jname in ACTS:
        look = ALIAS.get(cname, cname)
        pat = (r"case Act::" + look + r":(?:\s*case Act::[A-Z_]+:)*\s*\{(.*?)\n        \}")
        m = re.search(pat, src, re.S)
        if not m:
            print("!! not found:", cname)
            continue
        body = m.group(1)
        lines = []
        for lm in re.finditer(r"float (.*?);", body, re.S):
            depth = 0
            pieces, cur = [], ""
            for ch in lm.group(1):
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                if ch == "," and depth == 0:
                    pieces.append(cur); cur = ""
                else:
                    cur += ch
            pieces.append(cur)
            for piece in pieces:
                dm = re.match(r"\s*(\w+)\s*=\s*(.+)$", piece, re.S)
                if not dm:
                    continue
                lines.append("     const %s = %s;" % (dm.group(1), to_js(dm.group(2))))
        fields = []
        for fm in re.finditer(r"a\.(\w+)\s*=\s*([^;]+);", body):
            fields.append("%s:%s" % (fm.group(1), to_js(fm.group(2))))
        if not fields:
            print("!! no fields:", cname)
            continue
        stmts = []
        for f in fields:
            k, v = f.split(":", 1)
            stmts.append("a.%s=%s" % (k, v))
        txt = "; ".join(stmts)
        if len(txt) > 200:
            txt = txt.replace(";", ";\n     ")
        out.append("   case '%s': { %s\n     %s; break; }" % (jname, " ".join(lines), txt))
    js = "\n".join(out)
    open("/tmp/prev_poses.js", "w").write(js + "\n")
    print(js[:1500])
    print("... total cases:", len(out))


if __name__ == "__main__":
    main()
