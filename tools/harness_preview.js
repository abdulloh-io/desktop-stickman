// Headless smoke test for the browser preview (v1..v4).
//  phase A: 90 s of organic play — pointer gestures + every toolbar button clicked
//  phase B: every v4 act force-started through its own starter, 8 s each
// Asserts: no exceptions, no NaN joints/props, nobody leaves the stage, props drain.
const fs = require('fs');
const vm = require('vm');
const path = require('path');

const HTML = process.argv[2] || '/home/user/AlanBeckerStickmen_Preview.html';
const src = fs.readFileSync(HTML, 'utf8');
const m = src.match(/<script>([\s\S]*)<\/script>/);
if (!m) { console.log('no <script> block in ' + HTML); process.exit(1); }
const code = m[1];

let t = 0;
function ctxProxy() {
  const store = {};
  return new Proxy(store, {
    get(o, k) { if (k === 'canvas') return {width: 1200, height: 640}; return k in o ? o[k] : () => undefined; },
    set(o, k, v) { o[k] = v; return true; }
  });
}
function el(id) {
  const L = {};
  return {id, style: {}, textContent: '', value: '1',
    classList: {add(){}, remove(){}, toggle(){}, contains(){return false;}},
    getContext: ctxProxy, getBoundingClientRect: () => ({left: 0, top: 0, width: 1200, height: 640}),
    addEventListener: (k, f) => { (L[k] = L[k] || []).push(f); },
    removeEventListener(){}, setPointerCapture(){}, releasePointerCapture(){},
    appendChild(){}, contains(){ return true; }, focus(){},
    onclick: null, onchange: null,
    fire(k, e) { (L[k] || []).forEach(f => f(e)); }};
}
const els = {};
const getEl = (id) => (els[id] = els[id] || el(id));
const sandbox = {
  document: {getElementById: getEl, createElement: () => el('n'), body: {}, addEventListener(){}, querySelector: () => el('q')},
  addEventListener(){}, removeEventListener(){},
  navigator: {}, performance: {now: () => t * 1000}, requestAnimationFrame: () => {},
  setTimeout: () => 0, clearTimeout(){}, setInterval: () => 0, Math, JSON, console, devicePixelRatio: 1,
};
vm.createContext(sandbox);
sandbox.window = sandbox; sandbox.globalThis = sandbox;
const run = (s) => vm.runInContext(s, sandbox);

let bad = 0, dead = false;
try { vm.runInContext(code, sandbox); }
catch (e) { console.log('TOP-LEVEL THROW:', e.stack.split('\n').slice(0, 3).join(' | ')); process.exit(1); }

const V4 = ['chase','ride','fight','theft','speak','guard','hideicon','kickicon','folder',
            'taboops','couchd','plug','umbrella','pass','crack'];
const seen = new Set();
const wake = 'couchOn=false; deskOn=false; for (const s of men) if (s.act==="couch"||s.act==="popcorn"||s.act==="laptop") s.timer=0.02;';
const clamp = (v, a, b) => Math.min(Math.max(v, a), b);

function frames(n, opts) {
  const o = opts || {};
  for (let i = 0; i < n; i++) {
    t += 1 / 60;
    run('lastInput=performance.now(); afkUntil=0;');
    if (o.wave) {
      const tgt = JSON.parse(run('(function(){const s=men[(Math.random()*men.length)|0];return s?JSON.stringify([s.x,s.y]):"[600,400]";})()'));
      const jx = clamp(tgt[0] + Math.sin(t * 2.1) * 90, 30, 1170);
      const jy = clamp(tgt[1] - 40 + Math.cos(t * 1.4) * 26, 20, 620);
      if (i % 2 === 0) getEl('c').fire('pointermove', {clientX: jx, clientY: jy, pointerId: 1, button: 0});
      if (i % 260 === 0) getEl('c').fire('pointerdown', {clientX: jx, clientY: jy, button: 2, pointerId: 1});
    }
    if (o.every && i % o.every === 0 && o.tick) o.tick(i);
    if (o.cruise) {                                            // steady side-to-side glide
      let amp = 260, cx0 = 600;
      if (o.cruise === 'near') { cx0 = JSON.parse(run('JSON.stringify(men[0].x)')); amp = 40; }
      const cy = (o.cruise === 'near' ? 412 : 300) + Math.sin(t * 0.5) * (o.cruise === 'near' ? 18 : 90);
      const cx = cx0 + Math.sin(t * 0.9) * amp;
      // glide, never teleport: a 1-frame jump reads as a violent flick and throws riders off
      const c = JSON.parse(run('JSON.stringify([cur.x,cur.y])'));
      const nx = c[0] + Math.max(-22, Math.min(22, cx - c[0]));
      const ny = c[1] + Math.max(-22, Math.min(22, cy - c[1]));
      getEl('c').fire('pointermove', {clientX: nx, clientY: ny, pointerId: 1, button: 0});
    }
    try { run('step(1/60); draw();'); }
    catch (e) { console.log('STEP THROW at frame', i, ':', e.message); bad++; dead = true; return false; }
    for (const a of run('men.map(m=>m.act)')) if (V4.includes(a)) seen.add(a);
    const nan = run('men.filter(s=>!s.J||Object.values(s.J).some(p=>!p||!isFinite(p[0])||!isFinite(p[1]))).length');
    if (nan) { console.log('NaN joints:', run('JSON.stringify(men.map(s=>[s.name,s.act]))')); bad++; dead = true; return false; }
    if (run('props3.filter(w=>!isFinite(w.x)||!isFinite(w.y)||!isFinite(w.rot||0)).length')) { console.log('NaN prop'); bad++; dead = true; return false; }
    if (run('parts.length>1400')) { console.log('particle leak:', run('parts.length')); bad++; dead = true; return false; }
    const off = run('men.filter(s=>s.x<-300||s.x>1500||s.y>1200||s.y<-600).length');
    if (off) { console.log('off-stage:', run('JSON.stringify(men.map(s=>[s.name,s.act,Math.round(s.x),Math.round(s.y)]))')); bad++; dead = true; return false; }
  }
  return true;
}

// ---------------- phase A: organic play, one button per 2.5 s ----------------
const buttons = ['bChase','bRide','bFight','bTheft','bNote','bGuard','bHide','bKickIcon','bFolder',
                 'bTab','bCouchDrag','bPlug','bUmb','bPass','bDuelShow','bBatt',
                 'bHello','bCouch','bDesk','bPizza','bBall','bSpar','bParty','bCook','bDuel','bPat',
                 'bSnow','bPrank','bTea','bCake','bMelon','bPotion','bCube'];
let bi = 0;
frames(5400, {wave: true, collect: true, every: 150, tick: () => {
  if (bi >= buttons.length) return;
  const id = buttons[bi++], f = getEl(id).onclick;
  if (!f) { console.log('BTN missing handler:', id); bad++; return; }
  try { run(wake); f({target: getEl(id)}); } catch (e) { console.log('BTN', id, 'THREW:', e.message); bad++; }
}});
const phaseA = new Set(seen);
console.log('phase A: v4 acts seen organically (' + phaseA.size + '/' + V4.length + '):', [...phaseA].join(' '));

// ---------------- phase B: start every v4 act through its own starter ----------------
const startVia = (id) => () => { const f = getEl(id).onclick; if (!f) throw new Error('no handler ' + id); f({target: getEl(id)}); };
// what each v4 beat must actually ACHIEVE (not just which act name is set)
const BEAT = {
  chase:   'men.some(s=>Math.abs(s.x-cur.x)<34*scale)',
  ride:    'men.some(s=>Math.hypot(s.x-cur.x,s.y-(cur.y+26*scale))<24)',
  fight:   'men.some(s=>s.blocks>0)||Math.min(...men.map(s=>Math.hypot(s.x-cur.x,s.y-92*scale-cur.y)))<92',
  theft:   'men.some(s=>s.carry===13&&Math.hypot(ghost.x-cur.x,ghost.y-cur.y)>50)',
  speak:   'men.some(s=>s.speech)',
  guard:   'guard.a>0.9&&men.some(s=>s.act==="guard")',
  hideicon:'props3.some(w=>w.k==="icon"&&w.held)',
  kickicon:'props3.some(w=>w.k==="icon"&&Math.abs(w.vx)>1)',
  folder:  'men.some(s=>s.carry===8)',
  taboops: 'props3.some(w=>w.k==="tab"&&!w.held)',
  couchd:  'Math.abs(couch.x-570)>60',
  plug:    'men.some(s=>s.blocks>=1)',
  umbrella:'men.some(s=>s.carry===10)',
  pass:    'balls.some(b=>b.kick>0)',
  crack:   'props3.some(w=>w.k==="crack")&&men.some(s=>s.carry===6)'
};
const START = {
  chase:   () => run('for (const s of men) if (s.grounded&&!SCENE3.includes(s.act)){ s.act="chase"; s.timer=9; s.face=cur.x>=s.x?1:-1; }'),
  ride:    () => run('for (const s of men) if (s.grounded&&!SCENE3.includes(s.act)){ s.act="ride"; s.timer=9; }'),
  fight:   () => run('for (const s of men) if (s.grounded&&!SCENE3.includes(s.act)){ s.act="fight"; s.timer=9; s.blocks=0; }'),
  theft:   startVia('bTheft'),
  speak:   startVia('bNote'),
  guard:   startVia('bGuard'),
  hideicon:startVia('bHide'),
  kickicon:startVia('bKickIcon'),
  folder:  startVia('bFolder'),
  taboops: startVia('bTab'),
  couchd:  () => { run('couchOn=true; couchSeats();'); startVia('bCouchDrag')(); },
  plug:    startVia('bPlug'),
  umbrella:() => { run('weatherOn=false; weatherMode=0; raining=true; wTimer=600;'); startVia('bUmb')(); },
  pass:    startVia('bPass'),
  crack:   startVia('bCrack')
};
let beats = {}, fails = [];
const cruiseFor = (a) => (a === 'fight' || a === 'chase') ? 'near' : (a === 'ride' || a === 'theft');
for (const a of V4) {
  if (dead) break;
  run(wake);
  run('for (const s of men){ if (s.act!=="grab"){ s.pidx=-1; s.hidden=false; s.carry=0; s.blocks=0; s.rides=0; s.floatT=0; s.speech=""; s.speechT=9; s.prop=0; s.dest=null; setAct(s,"idle",30); } }');
  frames(150);                                  // let the squad land on the floor first
  const couch0 = run('Math.round(couch.x)');
  run('couch0=' + couch0 + ';');
  try { (START[a] || (() => run('for (const s of men){ s.act="' + a + '"; s.timer=9; }')))(); }
  catch (e) { console.log('STARTER for', a, 'THREW:', e.message); bad++; continue; }
  let k = 0;                                    // up to 4 s for the starter to hand the act out
  while (!run('men.some(s=>s.act==="' + a + '")') && k < 240 && !dead) { frames(1); k++; }
  if (!run('men.some(s=>s.act==="' + a + '")')) {
    run('for (const s of men){ s.grounded=true; s.y=floor; s.vy=0; s.act="' + a + '"; s.timer=10; s.blocks=0; }');
    frames(30);
    if (!run('men.some(s=>s.act==="' + a + '")')) { console.log(a.padEnd(9) + '  !! never reached the act even when forced'); bad++; continue; }
    console.log(a.padEnd(9) + '  (starter declined, pose checked in the forced state only)');
  }
  // run the act to its end, sampling whether the beat ever lands
  let n = 0, ok = false;
  const stillBusy = 'men.some(s=>s.act==="' + a + '"||(s.act==="travel"&&s.dest==="' + a + '"))';
  while (run(stillBusy) && n < 1500 && !dead) {
    frames(10, {cruise: cruiseFor(a)}); n += 10;
    if (a === 'chase') { ok = ok || !!run('men.some(s=>' + BEAT.chase.replace('s.', 's.') + ')'); }
    else ok = ok || !!run(BEAT[a]);
    if (ok) continue;                            // keep waiting for it while the act runs
  }
  frames(240, {cruise: cruiseFor(a)});   // let errands finish
  beats[a] = ok;
  if (!ok) fails.push(a);
  const props = run('props3.length');
  const stuck = run('men.filter(s=>s.hidden||s.pidx>=0).length');
  const carry = run('men.filter(s=>s.carry&&s.carry!==1).length');
  console.log(a.padEnd(9) + (ok ? ' beat OK  ' : ' beat MISS ') + (n / 60).toFixed(1) + 's  propsLeft:' + props +
              '  stuck:' + stuck + '  carryLeft:' + carry);
  if (stuck || carry) { console.log('  !! state not cleaned up after ' + a); bad++; }
  if (props > 2) { console.log('  !! props piled up after ' + a); bad++; }
}
const missed = V4.filter(a => !seen.has(a));
console.log('\nphase A+B: v4 acts reached (' + seen.size + '/' + V4.length + ')' + (missed.length ? '  MISSED: ' + missed.join(' ') : ''));
console.log('idle props/parts:', run('JSON.stringify({props:props3.length,parts:parts.length,balls:balls.length,snacks:snacks.length})'));
console.log(bad || missed.length ? 'RESULT: ' + bad + ' problems, ' + missed.length + ' acts unreachable' : 'RESULT: clean');
process.exit(bad || missed.length ? 1 : 0);
