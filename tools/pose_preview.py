#!/usr/bin/env python3
"""Prototype/tuner for the stickman forward-kinematics pose model.
The angle constants validated here are transcribed verbatim into src/main.cpp."""
import math
from PIL import Image, ImageDraw

HIPH = 25.5; SPINE = 21.5; HEADR = 8.6
UARM = 12.8; LARM = 12.2
ULEG = 13.0; LLEG = 12.5

def dd(a): return (math.sin(a), math.cos(a))   # 0 = straight down, + = forward
def du(a): return (math.sin(a), -math.cos(a))   # 0 = straight up

def A(**kw):
    a = dict(spine=0.05, headA=0.0, upArmL=-0.09, foreArmL=0.30,
             upArmR=0.11, foreArmR=0.26, thighL=-0.11, shinL=-0.03,
             thighR=0.12, shinR=0.04, hipRaise=0.0, lat=0.0, hop=0.0,
             sq=0.0, rot=None, plant=True)
    a.update(kw); return a

def build(a):
    hip = (a['lat'], -(HIPH + a['hipRaise']))
    d = du(a['spine']); neck = (hip[0] + d[0]*SPINE, hip[1] + d[1]*SPINE)
    d = du(a['spine'] + a['headA']); head = (neck[0] + d[0]*(HEADR+1.6), neck[1] + d[1]*(HEADR+1.6))
    d = dd(a['upArmL']); eL = (neck[0]+d[0]*UARM, neck[1]+d[1]*UARM)
    d = dd(a['foreArmL']); hL = (eL[0]+d[0]*LARM, eL[1]+d[1]*LARM)
    d = dd(a['upArmR']); eR = (neck[0]+d[0]*UARM, neck[1]+d[1]*UARM)
    d = dd(a['foreArmR']); hR = (eR[0]+d[0]*LARM, eR[1]+d[1]*LARM)
    d = dd(a['thighL']); kL = (hip[0]+d[0]*ULEG, hip[1]+d[1]*ULEG)
    d = dd(a['shinL']); fL = (kL[0]+d[0]*LLEG, kL[1]+d[1]*LLEG)
    d = dd(a['thighR']); kR = (hip[0]+d[0]*ULEG, hip[1]+d[1]*ULEG)
    d = dd(a['shinR']); fR = (kR[0]+d[0]*LLEG, kR[1]+d[1]*LLEG)
    J = dict(hip=hip, neck=neck, head=head, eL=eL, hL=hL, eR=eR, hR=hR,
             kL=kL, fL=fL, kR=kR, fR=fR)
    # plant lowest foot on y = 0
    if a['plant']:
        dy = -max(fL[1], fR[1])
        J = {k: (v[0], v[1]+dy) for k, v in J.items()}
    # squash & stretch around the ground line
    if a['sq']:
        ky = 1.0 - a['sq']; kx = 1.0 + a['sq']*0.45
        J = {k: (v[0]*kx, v[1]*ky) for k, v in J.items()}
    if a['hop']:
        J = {k: (v[0], v[1]-a['hop']) for k, v in J.items()}
    if a['rot'] is not None:
        c = (0.0, -32.0); ca, sa = math.cos(a['rot']), math.sin(a['rot'])
        def rot(p):
            x, y = p[0]-c[0], p[1]-c[1]
            return (c[0]+x*ca-y*sa, c[1]+x*sa+y*ca)
        J = {k: rot(v) for k, v in J.items()}
    return J

# ---------------- pose library ----------------
def walk(p, run=False):
    amp = 1.05 if run else 0.62
    la = 0.95 if run else 0.58
    s1, s2 = math.sin(p), math.sin(p+math.pi)
    c1, c2 = math.cos(p), math.cos(p+math.pi)
    bob = (2.6 if run else 1.3)*abs(math.sin(p)) - (1.2 if run else 0.5)
    return A(spine=0.34 if run else 0.10, hipRaise=bob,
             upArmL=s1*amp*0.85-0.02, foreArmL=s1*amp*0.85-0.02+(1.35 if run else 0.75)-0.28*s1,
             upArmR=s2*amp*0.85-0.02, foreArmR=s2*amp*0.85-0.02+(1.35 if run else 0.75)-0.28*s2,
             thighL=s1*la, shinL=s1*la-max(0.0, min(1.9, 0.18-c1*0.75))*(1.25 if run else 1.0),
             thighR=s2*la, shinR=s2*la-max(0.0, min(1.9, 0.18-c2*0.75))*(1.25 if run else 1.0))

POSES = [
    ("idle",            A()),
    ("walk 0",          walk(0.0)),
    ("walk 1",          walk(0.9)),
    ("walk 2",          walk(1.9)),
    ("walk 3",          walk(3.0)),
    ("run 0",           walk(0.6, True)),
    ("run 1",           walk(1.7, True)),
    ("run 2",           walk(2.8, True)),
    ("wave hello",      A(spine=0.05, hipRaise=0.3, upArmL=-0.05, foreArmL=0.25,
                          upArmR=2.42, foreArmR=2.72, headA=0.10)),
    ("cheer",           A(spine=-0.05, hipRaise=2.0, hop=5.0, upArmL=-2.62, foreArmL=-2.95,
                          upArmR=2.62, foreArmR=2.95, thighL=-0.22, shinL=-0.10,
                          thighR=0.22, shinR=0.10)),
    ("high five",       A(spine=0.12, hop=3.0, upArmL=-0.10, foreArmL=0.30,
                          upArmR=2.30, foreArmR=2.45, thighL=-0.18, thighR=0.24)),
    ("point",           A(spine=0.10, upArmR=1.35, foreArmR=1.42, upArmL=-0.15, foreArmL=0.50,
                          headA=0.25)),
    ("air rise",        A(spine=-0.02, plant=False, upArmL=2.70, foreArmL=2.30,
                          upArmR=-2.55, foreArmR=-2.15, thighL=0.55, shinL=0.05,
                          thighR=-0.35, shinR=-0.90)),
    ("air fall",        A(spine=0.20, plant=False, upArmL=2.20, foreArmL=1.60,
                          upArmR=-1.90, foreArmR=-1.30, thighL=0.75, shinL=0.30,
                          thighR=-0.55, shinR=-1.10)),
    ("flip tucked",     A(spine=0.90, plant=False, rot=1.0, upArmL=1.50, foreArmL=0.50,
                          upArmR=1.60, foreArmR=0.60, thighL=1.50, shinL=2.90,
                          thighR=1.40, shinR=2.80)),
    ("sit couch",       A(hipRaise=-13.0, spine=-0.10, headA=0.42, plant=False,
                          upArmL=1.95, foreArmL=1.70, upArmR=1.00, foreArmR=0.85,
                          thighL=1.55, shinL=0.02, thighR=1.50, shinR=-0.04)),
    ("couch: eat",      A(hipRaise=-13.0, spine=-0.10, headA=0.42, plant=False,
                          upArmL=1.95, foreArmL=1.70, upArmR=2.05, foreArmR=-1.51,
                          thighL=1.55, shinL=0.02, thighR=1.50, shinR=-0.04)),
    ("laptop",          A(hipRaise=-13.0, spine=0.42, headA=0.30, plant=False,
                          upArmL=1.18, foreArmL=1.02, upArmR=1.34, foreArmR=1.18,
                          thighL=1.52, shinL=0.05, thighR=1.46, shinR=0.00)),
    ("sit edge",        A(hipRaise=-13.0, spine=0.12, headA=0.20, plant=False,
                          upArmL=1.25, foreArmL=1.05, upArmR=1.40, foreArmR=1.22,
                          thighL=1.55, shinL=0.35, thighR=1.52, shinR=0.05)),
    ("sketch crouch",   A(hipRaise=-15.5, spine=0.55, headA=0.35, plant=True,
                          upArmL=0.95, foreArmL=0.62, upArmR=1.15, foreArmR=0.80,
                          thighL=1.25, shinL=-0.55, thighR=1.05, shinR=-0.35)),
    ("read",            A(hipRaise=-20.5, spine=0.35, headA=0.45, plant=False,
                          upArmL=1.00, foreArmL=1.62, upArmR=1.12, foreArmR=1.72,
                          thighL=1.42, shinL=1.52, thighR=1.36, shinR=1.46)),
    ("music",           A(hipRaise=-1.2, spine=0.16, headA=-0.15,
                          upArmL=1.70, foreArmL=1.35, upArmR=0.80, foreArmR=0.55,
                          thighL=-0.20, shinL=-0.05, thighR=0.22, shinR=0.05)),
    ("fish",            A(hipRaise=-2.0, spine=0.22, headA=0.30,
                          upArmL=1.85, foreArmL=1.95, upArmR=1.70, foreArmR=1.60,
                          thighL=-0.30, shinL=-0.20, thighR=0.10, shinR=0.05)),
    ("nap",             A(hipRaise=-21.5, spine=1.45, headA=0.10, plant=False, lat=-4.0,
                          upArmL=2.20, foreArmL=-2.60, upArmR=1.60, foreArmR=1.50,
                          thighL=-1.50, shinL=-1.45, thighR=-1.44, shinR=-1.40)),
    ("guard spar",      A(hipRaise=-2.0, spine=0.20, lat=-2.0, headA=0.05,
                          upArmL=1.15, foreArmL=2.55, upArmR=1.30, foreArmR=2.35,
                          thighL=-0.42, shinL=-0.75, thighR=0.30, shinR=0.05)),
    ("jab",             A(hipRaise=-1.6, spine=0.34, lat=-1.0, headA=0.10,
                          upArmL=1.15, foreArmL=2.55, upArmR=1.45, foreArmR=1.42,
                          thighL=-0.45, shinL=-0.80, thighR=0.50, shinR=0.10)),
    ("kick",            A(hipRaise=-1.0, spine=-0.18, lat=1.0, headA=0.15, hop=2.0,
                          upArmL=2.30, foreArmL=2.60, upArmR=-1.60, foreArmR=-1.20,
                          thighL=-0.14, shinL=-0.10, thighR=1.45, shinR=1.35)),
    ("flinch",          A(hipRaise=-3.0, spine=-0.28, hop=3.0, plant=True,
                          upArmL=-1.90, foreArmL=-1.20, upArmR=2.05, foreArmR=1.35,
                          thighL=-0.30, shinL=-0.70, thighR=0.35, shinR=-0.20)),
]

SC = 2.4
CW, CH = 210, 200
COLS = 4
ROWS = (len(POSES)+COLS-1)//COLS
img = Image.new("RGB", (COLS*CW, ROWS*CH), (246, 246, 250))
d = ImageDraw.Draw(img)
for i, (name, a) in enumerate(POSES):
    cx = (i % COLS)*CW + CW//2
    base = (i // COLS)*CH + CH - 34
    d.rectangle([( (i%COLS)*CW, (i//COLS)*CH), ((i%COLS+1)*CW-1, (i//COLS+1)*CH-1)], outline=(210,212,220))
    J = build(a)
    def P(k):
        x, y = J[k]
        return (cx + x*SC, base + y*SC)
    d.line([(cx-62, base), (cx+62, base)], fill=(150, 200, 150), width=1)
    for seg in [("neck","hip"),("neck","eL"),("eL","hL"),("neck","eR"),("eR","hR"),
                ("hip","kL"),("kL","fL"),("hip","kR"),("kR","fR")]:
        d.line([P(seg[0]), P(seg[1])], fill=(35, 38, 46), width=4)
    h = P("head")
    d.ellipse([h[0]-HEADR*SC, h[1]-HEADR*SC, h[0]+HEADR*SC, h[1]+HEADR*SC], fill=(238, 96, 36))
    for k in ("eL","hL","eR","hR","kL","fL","kR","fR","hip","neck"):
        p = P(k); d.ellipse([p[0]-1.6, p[1]-1.6, p[0]+1.6, p[1]+1.6], fill=(230,60,60))
    d.text(((i%COLS)*CW+6, (i//COLS)*CH+4), name, fill=(60,60,70))
img.save("/home/user/pose_check.png")
print("saved", img.size)
