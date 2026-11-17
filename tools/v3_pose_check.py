#!/usr/bin/env python3
"""Pull the Act pose cases (v3 + v4) straight out of src/main.cpp, evaluate the
angle expressions at several phases, and render them with the validated FK model
so the anatomy can be checked (visually and numerically) before shipping.
Usage: python3 tools/v3_pose_check.py [--out /path.png] [--only NAME,NAME]
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw
from pose_preview import A, build  # FK + joint drawing model

K_SIT = -(25.5 - 12.5)   # kSitHipRaise
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "main.cpp")

FIELDS = ["spine", "headA", "upArmL", "foreArmL", "upArmR", "foreArmR",
          "thighL", "shinL", "thighR", "shinR", "hipRaise", "lat", "hop"]


# state the pose code may read; POSE_* env lets us sweep the *driving* variables,
# because several acts are animated by s.actTimer / s.propT rather than by animTime
SVAR = {"actTimer": 0.0, "propT": 0.0, "phase": 0.0, "vy": 0.0, "squash": 0.0,
        "mood": 0.7, "petT": 0.0, "blocks": 0.0, "carry": 0.0}


class Zero:
    def __getattr__(self, k):
        return SVAR.get(k, 0.0)


def set_timer(v):
    SVAR["actTimer"] = SVAR["propT"] = float(v)


def eval_expr(txt, extra, t=0.0):
    env = dict(math=math, sin=math.sin, cos=math.cos, fabs=abs,
               Osc=lambda t, f: math.sin(t * f),
               Bounce=lambda t, f: abs(math.sin(t * f)),
               ClampF=lambda v, a, b: min(max(v, a), b),
               LerpF=lambda a, b, k: a + (b - a) * k,
               kSitHipRaise=K_SIT, PI_F=math.pi, t=t, s=Zero(), g_app=Zero())
    env.update(extra)
    txt = re.sub(r"//.*", "", txt)
    txt = txt.replace("std::", "").replace("(float)", "")
    txt = re.sub(r"(\d(?:\.\d+)?)f\b", r"\1", txt)
    return float(eval(txt.strip(), env))


POW = float(os.environ.get("POSE_ROW", "0"))     # which animation phase to draw
POSE_TIMER = float(os.environ.get("POSE_TIMER", "0"))   # act-timer state to draw


def grab_cases(src, names, t=0.0):
    """Return {name: {field: value}} for `case Act::NAME: { ... break; }` at phase t."""
    out = {}
    for n in names:
        m = re.search(r"case Act::" + n + r":(?:\s*case Act::\w+:)*\s*\{(.*?)\n        \}",
                      src, re.S)
        if not m:
            out[n] = None
            continue
        body = m.group(1)
        # local float declarations first so the field expressions can use them
        locs = {}
        for lm in re.finditer(r"float (.*?);", body, re.S):
            # split on top-level commas only: "float a = f(x, y), b = 2;"
            pieces, depth, cur = [], 0, ""
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
                try:
                    locs[dm.group(1)] = eval_expr(dm.group(2), locs, t)
                except Exception:
                    pass
        vals = {}
        class Acc:
            def __getattr__(self, k):
                return vals.get(k, 0.0)
        locs["a"] = Acc()
        for fm in re.finditer(r"a\.(\w+)\s*=\s*([^;]+);", body):
            fld, expr = fm.group(1), fm.group(2)
            if fld not in FIELDS:
                continue
            try:
                vals[fld] = eval_expr(expr, locs, t)
            except Exception as exc:
                print("  ! %s.%s: %s / %r" % (n, fld, exc, expr))
        # double assignments like "a.upArmL = 1.40f; a.foreArmL = 1.10f;" on one line
        for fm in re.finditer(r"a\.(\w+) = ([^;]+);\s*a\.", body):
            pass
        out[n] = vals
    return out


def draw_stick(d, J, cx, cy, sc, col):
    def P(p):
        return (cx + p[0] * sc, cy + p[1] * sc)
    segs = [("neck", "hip"), ("neck", "eL"), ("eL", "hL"), ("neck", "eR"), ("eR", "hR"),
            ("hip", "kL"), ("kL", "fL"), ("hip", "kR"), ("kR", "fR")]
    far = [("neck", "eL"), ("eL", "hL"), ("hip", "kL"), ("kL", "fL")]
    for a_, b_ in segs:
        w = 3.0
        c = col if (a_, b_) not in far else tuple(int(v * 0.86) for v in col)
        d.line([P(J[a_]), P(J[b_])], fill=c, width=4)
    d.ellipse([P(J["head"])[0] - 8.6 * sc, P(J["head"])[1] - 8.6 * sc,
               P(J["head"])[0] + 8.6 * sc, P(J["head"])[1] + 8.6 * sc], outline=col, width=3)
    for k in ("eL", "hL", "eR", "hR", "kL", "fL", "kR", "fR", "hip", "neck"):
        px, py = P(J[k])
        d.ellipse([px - 2.0, py - 2.0, px + 2.0, py + 2.0], fill=(230, 60, 60))


PHASES = (0.0, 0.45, 0.9, 1.4, 2.0, 2.7, 3.5, 4.4)


def main():
    names = ["CARD_GAME", "WATCH_GAME", "COOK", "QUEUE", "LAB", "ARMWRESTLE", "TUGWAR",
             "HANDSHAKE", "SPY_CHAT", "LAUGH", "FACEPALM", "YAWN", "DANCE", "CURSOR_PLAY",
             "HANG_CURSOR", "PETTED", "ASK_ATTENTION", "TEA_RUN", "CRACK_PRANK", "MUSTACHE",
             "BOX_KICK", "SQUISH", "EXTINGUISH", "IN_VIDEO", "PUSH_WINDOW", "AIR_TYPE",
             # --- v4 ---
             "CHASE_CURSOR", "RIDE_CURSOR", "CURSOR_THEFT", "SPEAK", "SEND_GUARD",
             "HIDE_ICON", "TAB_OOPS", "UMBRELLA", "PLUG_IN"]
    # acts that reuse another act's pose case (fall-through labels) must match it
    aliases = {"ICON_KICK": "BOX_KICK", "PASS_BALL": "PLAY_BALL",
               "BRING_FOLDER": "TEA_RUN", "DRAG_COUCH": "TEA_RUN",
               "FIGHT_CURSOR": "SPARRING"}
    argv = sys.argv[1:]
    out_png = "/home/user/pose_check_v3.png"
    if "--out" in argv:
        out_png = argv[argv.index("--out") + 1]
    if "--only" in argv:
        keep = set(argv[argv.index("--only") + 1].split(","))
        names = [n for n in names if n in keep]
    src = open(SRC, encoding="utf-8").read()
    set_timer(POSE_TIMER)
    cases = grab_cases(src, names, POW)
    for a, b in aliases.items():
        if not re.search(r"case Act::" + a + r":", src):
            sys.exit("MISSING fall-through label: case Act::%s" % a)
        if not re.search(r"case Act::" + b + r":", src):
            sys.exit("alias target has no pose case: Act::%s" % b)
        print("  alias ok: %-13s -> %s" % (a, b))

    # ---- numeric anatomy assertions over a sweep of phases ----
    AIRY = {"HANG_CURSOR", "IN_VIDEO", "SQUISH", "RIDE_CURSOR", "CHASE_CURSOR",
            "CURSOR_THEFT", "PLUG_IN", "TAB_OOPS", "DANCE", "YAWN"}
    SEATED = {"CARD_GAME", "WATCH_GAME", "ARMWRESTLE", "TUGWAR", "SPY_CHAT", "NAP",
              "SEND_GUARD", "YAWN", "LAB", "COOK", "PETTED", "MUSTACHE", "LAUGH",
              "ASK_ATTENTION", "FACEPALM", "CRACK_PRANK", "CURSOR_THEFT", "HIDE_ICON"}
    KICKY = {"BOX_KICK", "ICON_KICK", "PLAY_BALL", "PASS_BALL", "CHASE_CURSOR",
             "RUN", "AIRBORNE", "RIDE_CURSOR", "SQUISH", "FIGHT_CURSOR"}
    SIDEWAYS = {"SQUISH"}          # flattened against a window frame by design
    # the act timer sweep: a pose that only bends at the end of its act must be
    # checked at the end, not just at t=0
    TIMERS = (0.0, 1.4, 3.0, 4.6, 5.6, 7.0, 8.6, 10.4)
    bad = []
    ncheck = 0
    for n in names:
        for tv in ([POW] if os.environ.get("POSE_ONE") else TIMERS):
            set_timer(tv)
            SVAR["phase"] = SVAR["actTimer"]
            for t in PHASES:
                ncheck += 1
                vals = grab_cases(src, [n], t).get(n)
                if not vals:
                    bad.append("%s t=%.2f: no pose case" % (n, t))
                    continue
                kw = {f: float(vals[f]) for f in FIELDS if f in vals}
                for f, v in kw.items():
                    if not math.isfinite(v):
                        bad.append("%s t=%.2f: %s is not finite" % (n, t, f))
                hr = kw.get("hipRaise", 0.0)
                if not (-22.0 < hr < 8.0):
                    bad.append("%s t=%.2f: hipRaise %.2f out of range" % (n, t, hr))
                if n not in SEATED and hr < -15.0:
                    bad.append("%s t=%.2f: standing act sinks the hip to %.2f" % (n, t, hr))
                for leg in ("thighL", "thighR"):
                    if n not in AIRY and abs(kw.get(leg, 0.0)) > 2.35:
                        bad.append("%s t=%.2f: %s %.2f over-rotates" % (n, t, leg, kw[leg]))
                J = build(A(plant=n not in AIRY, **{k: v for k, v in kw.items() if k in A().keys()}))
                if n not in SIDEWAYS and not (J["head"][1] <= J["neck"][1] + 0.6
                                              and J["neck"][1] < J["hip"][1] - 4.0):
                    bad.append("%s t=%.2f: head/neck/hip not stacked (%.1f %.1f %.1f)"
                               % (n, t, J["head"][1], J["neck"][1], J["hip"][1]))
                if n not in KICKY:
                    for side in ("L", "R"):
                        k_, f_ = J["k" + side], J["f" + side]
                        if f_[1] < k_[1] - 1.0:
                            bad.append("%s t=%.2f: foot%s above knee%s (hyperextension)"
                                       % (n, t, side, side))
                if n not in AIRY:
                    for side in ("L", "R"):
                        if J["f" + side][1] > 0.6:
                            bad.append("%s t=%.2f: foot%s below the floor" % (n, t, side))
    if bad:
        print("POSE PROBLEMS:")
        for b in bad[:40]:
            print("  -", b)
        if len(bad) > 40:
            print("  ... %d more" % (len(bad) - 40))
    else:
        print("all %d poses numerically OK over %d phases x %d act-timer states (%d samples)"
              % (len(names), len(PHASES), len([POW] if os.environ.get("POSE_ONE") else TIMERS), ncheck))

    sc = 3.0
    cw, ch = 190, 236
    cols = 7
    rows = (len(names) + cols - 1) // cols
    img = Image.new("RGB", (cw * cols, ch * rows), (246, 247, 250))
    d = ImageDraw.Draw(img)
    for i, n in enumerate(names):
        vals = cases.get(n)
        cx = (i % cols) * cw + cw * 0.5
        cy = (i // cols) * ch + ch - 30
        d.text((cx - 40, (i // cols) * ch + 6), n.replace("_", " ").lower(), fill=(70, 74, 84))
        d.line([((i % cols) * cw + 12, cy), ((i % cols + 1) * cw - 12, cy)],
               fill=(150, 200, 150), width=1)
        if not vals:
            d.text((cx - 30, cy - 40), "MISSING", fill=(220, 60, 60))
            continue
        pi = int(os.environ.get("POSE_ROW", "0")) % len(PHASES)
        v2 = grab_cases(src, [n], PHASES[pi]).get(n) or vals
        kw = {f: v2[f] for f in FIELDS if f in v2}
        plant = n not in ("HANG_CURSOR", "IN_VIDEO", "RIDE_CURSOR", "CURSOR_THEFT")
        kw["plant"] = plant
        if not plant:
            kw["hop"] = kw.get("hop", 0.0)
        J = build(A(**kw))
        draw_stick(d, J, cx, cy, sc, (26, 28, 34))
    out = out_png
    img.save(out)
    print("saved", out, img.size)


if __name__ == "__main__":
    main()
