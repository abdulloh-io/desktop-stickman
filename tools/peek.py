import sys, math
src = open("pose_preview.py").read().split("# ---------------- pose library ----------------")[0]
ns = {}
exec(src, ns)
from PIL import Image, ImageDraw
build, A, walk, HEADR = ns['build'], ns['A'], ns['walk'], ns['HEADR']
POSES = ns['POSES'] if 'POSES' in ns else None
