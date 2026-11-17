// Numerical FK check for the pose block *as embedded in the HTML preview*.
// Extracts the switch(s.act){...} body from AlanBeckerStickmen_Preview.html, runs every
// case over a grid of phases through the same limb lengths the preview uses, and asserts
// the skeleton stays sane (finite, head above neck, knees not inverted, feet under the hips,
// hip raise within travel).  Usage: node tools/fk_check_pose.js [path/to/Preview.html]
const fs = require('fs');

const HTML = process.argv[2] || '/home/user/AlanBeckerStickmen_Preview.html';
const src = fs.readFileSync(HTML, 'utf8');

// ---- pull the switch body out of the file by brace matching ----
const head = src.indexOf('switch(s.act){');
if (head < 0) { console.log('no switch(s.act){ found in ' + HTML); process.exit(1); }
let depth = 0, end = -1;
for (let i = src.indexOf('{', head); i < src.length; i++) {
  if (src[i] === '{') depth++;
  else if (src[i] === '}') { depth--; if (depth === 0) { end = i; break; } }
}
if (end < 0) { console.log('unbalanced switch body'); process.exit(1); }
const body = src.slice(head + 'switch(s.act){'.length, end);

// ---- geometry constants, must match the preview's P ----
const P = {hipH: 25.5, spine: 21.5, headR: 8.6, uArm: 12.8, lArm: 12.2, uLeg: 13.0, lLeg: 12.5};
const SITH = -(P.hipH - P.lLeg);
const osc = (t, f) => Math.sin(t * f), bounce = (t, f) => Math.abs(Math.sin(t * f));
const clamp = (v, a, b) => Math.min(Math.max(v, a), b), lerp = (a, b, k) => a + (b - a) * k;
const dd = a => [Math.sin(a), Math.cos(a)], du = a => [Math.sin(a), -Math.cos(a)];
function build(a) {
  let hip = [a.lat, -(P.hipH + a.hipRaise)];
  let d = du(a.spine), neck = [hip[0] + d[0] * P.spine, hip[1] + d[1] * P.spine];
  d = du(a.spine + a.headA); const head = [neck[0] + d[0] * (P.headR + 1.6), neck[1] + d[1] * (P.headR + 1.6)];
  d = dd(a.upArmL); const eL = [neck[0] + d[0] * P.uArm, neck[1] + d[1] * P.uArm];
  d = dd(a.foreArmL); const hL = [eL[0] + d[0] * P.lArm, eL[1] + d[1] * P.lArm];
  d = dd(a.upArmR); const eR = [neck[0] + d[0] * P.uArm, neck[1] + d[1] * P.uArm];
  d = dd(a.foreArmR); const hR = [eR[0] + d[0] * P.lArm, eR[1] + d[1] * P.lArm];
  d = dd(a.thighL); const kL = [hip[0] + d[0] * P.uLeg, hip[1] + d[1] * P.uLeg];
  d = dd(a.shinL); const fL = [kL[0] + d[0] * P.lLeg, kL[1] + d[1] * P.lLeg];
  d = dd(a.thighR); const kR = [hip[0] + d[0] * P.uLeg, hip[1] + d[1] * P.uLeg];
  d = dd(a.shinR); const fR = [kR[0] + d[0] * P.lLeg, kR[1] + d[1] * P.lLeg];
  const J = {hip, neck, head, eL, hL, eR, hR, kL, fL, kR, fR};
  if (a.plant !== false) { const dy = -Math.max(fL[1], fR[1]); for (const k in J) J[k] = [J[k][0], J[k][1] + dy]; }
  if (a.hop) for (const k in J) J[k] = [J[k][0], J[k][1] - a.hop];
  return J;
}
const fn = new Function('s', 'a', 't', 'ph', 'cur', 'osc', 'clamp', 'lerp', 'bounce', 'SITH', 'P',
  'switch(s.act){' + body + '}');
const names = [...body.matchAll(/case '([a-z_0-9]+)'/g)].map(m => m[1]);
// poses that legitimately leave the ground, kick above the hip line, or lie sideways
const AIRY = new Set(['hang', 'invideo', 'squish', 'ride', 'chase', 'theft', 'plugin', 'taboops', 'dance', 'yawn',
                      'umbrella', 'couchd', 'pass', 'guard']);
const KICKY = new Set(['kickbox', 'kickicon', 'pass', 'tea', 'folder', 'couch', 'chase', 'airtype', 'ride',
                       'squish', 'fight', 'pat', 'cplay', 'hideicon', 'taboops', 'plug', 'umbrella', 'guard']);
// bodies that lie flat: the head->neck->hip vertical ordering does not apply
const SIDEWAYS = new Set(['squish', 'nap']);
const SEATED = new Set(['couch', 'popcorn', 'laptop', 'watch', 'cards', 'nap', 'campfire', 'cook', 'lab',
                        'queue', 'cplay', 'tea', 'guard', 'speak', 'plug', 'couchd']);
const PHASES = [0.0, 0.45, 0.9, 1.4, 2.0, 2.7, 3.5, 4.4];
const STAND = {spine: 0.05, headA: 0, upArmL: -0.09, foreArmL: 0.30, upArmR: 0.11, foreArmR: 0.26,
               thighL: -0.11, shinL: -0.03, thighR: 0.12, shinR: 0.04, hipRaise: 0, lat: 0, hop: 0, plant: true};

let bad = [], n = 0;
for (const act of names) {
  for (const t of PHASES) for (const face of [1, -1]) {
    const s = {act, x: 0, y: 0, vx: 40, vy: 0, face, timer: 3.5, anim: t, phase: t, prop: t, key: 0.3,
               grabA: 0.2, pet: 0.4, soot: 0.2, squash: 0, blocks: 0, carry: 0, dlen: 3, social: 0,
               mood: 0.7, speech: 'привет', speechT: 1, seen: 4, floatT: t, rides: 1, hidden: false,
               pidx: -1, seat: 1, partner: -1, hello: false, inVideo: false, lx: 4, ly: 9, rotA: 0.2,
               fish: 0.7 + t, hunger: 0.4, sitT: 1.2, hold: 0.5, blink: 0.3, flip: false, flipAng: 0.1,
               grounded: true, i: 0, weapon: 0, spin: 0.2, lean: 0.3, kick: 0.4, hug: 0.5, step: 0,
               squash2: 0, sootT: 0, lastTouch: 0, grabT: 1, mug: 1, bites: 2, drink: 0.5, n: 2};
    const a = Object.assign({}, STAND, {plant: !AIRY.has(act)});
    s.phase = t; const cur = {x: 140, y: -36, vx: 60, vy: 0};
    try { fn(s, a, t, t, cur, osc, clamp, lerp, bounce, SITH, P); }
    catch (e) { bad.push(act + ' t=' + t + ' THREW ' + e.message); continue; }
    n++;
    for (const k in a) if (typeof a[k] === 'number' && !isFinite(a[k])) bad.push(act + ' t=' + t + ': ' + k + ' NaN');
    if (a.hipRaise <= -22 || a.hipRaise >= 8) bad.push(act + ' t=' + t + ': hipRaise ' + a.hipRaise.toFixed(1));
    if (!AIRY.has(act) && (Math.abs(a.thighL) > 2.35 || Math.abs(a.thighR) > 2.35))
      bad.push(act + ' t=' + t + ': thigh over-rotates');
    const J = build(a);
    for (const k in J) if (!isFinite(J[k][0]) || !isFinite(J[k][1])) bad.push(act + ' t=' + t + ': joint ' + k + ' NaN');
    if (!SIDEWAYS.has(act) && !(J.head[1] <= J.neck[1] + 0.6 && J.neck[1] < J.hip[1] - 4))
      bad.push(act + ' t=' + t + ': head/neck/hip ' + J.head[1].toFixed(1) + ' ' + J.neck[1].toFixed(1) + ' ' + J.hip[1].toFixed(1));
    if (!KICKY.has(act)) for (const L of ['L', 'R']) { const k = J['k' + L], f = J['f' + L];
      if (f[1] < k[1] - 1.0) bad.push(act + ' t=' + t + ': foot' + L + ' above knee'); }
    if (!AIRY.has(act) && a.plant !== false) for (const L of ['L', 'R']) if (J['f' + L][1] > 0.6)
      bad.push(act + ' t=' + t + ': foot' + L + ' under floor ' + J['f' + L][1].toFixed(2));
  }
}
console.log('extracted ' + names.length + ' pose cases from ' + HTML);
console.log('checked ' + n + ' pose samples across ' + names.length + ' acts (' + PHASES.length + ' phases x 2 faces)');
if (bad.length) { console.log(bad.slice(0, 40).join('\n')); console.log('FAIL: ' + bad.length + ' problems'); process.exit(1); }
console.log('ALL PREVIEW POSES NUMERICALLY OK');
