#!/usr/bin/env python3
"""
# NOTE: this patch has ALREADY been applied to /home/user/AlanBeckerStickmen_Preview.html
# (v4, 2026-11-16).  It is kept only as the record of what the v4 preview contains; several
# follow-up fixes (prop index re-mapping via heldProp/dropProp, the ride act leaving the
# grounded branch, the couch goal clamp, the drawn chat window, mood/battery meters) were made
# directly in the HTML afterwards.  Re-running it now trips its own anchors on purpose.
"""One-shot patch that brings /home/user/AlanBeckerStickmen_Preview.html up to v4
(pointer games, notepad signs, drawn icon/folder/tab props, weather and battery).
Run after tools/gen_preview_poses.py has refreshed the pose block."""
import io
import re
import sys

PATH = "/home/user/AlanBeckerStickmen_Preview.html"
s = io.open(PATH, encoding="utf-8").read()
orig = s


def sub(old, new, count=1):
    global s
    assert s.count(old) >= 1, "anchor not found: " + old[:70].replace("\n", "\\n")
    s = s.replace(old, new, count)


# ---------------------------------------------------------------- header
sub("<h1>Alan Becker Stickmen <span>v3</span>", "<h1>Alan Becker Stickmen <span>v4</span>")
sub("<title>", "<title>")

# ---------------------------------------------------------------- state
sub("let couchOn=false, deskOn=false, typeMeter=0, audMeter=0;",
    """let couchOn=false, deskOn=false, typeMeter=0, audMeter=0;
// --- v4: shared mood, weather, battery, pointer games ---
let weatherOn=false, weatherMode=0, raining=false, snowing=false, wTimer=0;
let battPct=-1, battLow=false, battExt=true, battProbed=false, battForced=false;
let tease=0, pokes=0, squadMood=.75, pokeT=0;
let ghost={x:0,y:0}, guard={x:0,y:0,t:0}, couchGoal=0;
const NOTE=['привет!','погладь меня','вкусненького?','не мешай!','ой, извини','починил!',
  'держи','пора размяться!','скучно...','ну ну на','не отправляй!!','победа!','хорошая драка',
  'заряда!','подожди!','тяжёлый...','я нарочно','спасибо!','сыграем?','гол! мой!','тс-с…',
  'всё, я вернул'];
function sayNote(t,reason){ if(!t) return; t.speech=NOTE[reason]||'привет!'; t.speechT=0; t.seen=0; }
function anyoneSpeaking(){ return men.some(m=>m.speech && m.speechT<5); }""")

# ---------------------------------------------------------------- per-man fields
sub("x, y, vx:0, vy:0, face:1, act:'air', timer:1.2, anim:Math.random()*6, phase:Math.random()*6,",
    "x, y, vx:0, vy:0, face:1, act:'air', timer:1.2, anim:Math.random()*6, phase:Math.random()*6,\n"
    "    speech:'', speechT:9, seen:0, mood:.75, weapon:0, hidden:false, floatT:0, rides:0, blocks:0, pidx:-1,")

# ---------------------------------------------------------------- act durations
sub("    kickbox:rnd(7,11),squish:2.2,invideo:rnd(6,10),push:5,airtype:rnd(8,14)}[act]||2.0);",
    """    kickbox:rnd(7,11),squish:2.2,invideo:rnd(6,10),push:5,airtype:rnd(8,14),
    chase:rnd(2.6,4.6),ride:rnd(4,9),fight:rnd(4,7.5),theft:7,speak:rnd(5.5,8.5),guard:rnd(12,22),
    hideicon:rnd(6,12),kickicon:rnd(7,12),folder:rnd(9,14),taboops:6,couchd:5,plug:10,
    umbrella:rnd(14,26),pass:rnd(9,15)}[act]||2.0);""")

sub("""const SCENE3 = ['cards','watch','cook','queue','lab','arm','tug','hand','spy','laugh','facepalm','yawn',
  'dance','cplay','hang','pat','ask','tea','crack','moust','kickbox','squish','invideo','push','airtype'];""",
    """const SCENE3 = ['cards','watch','cook','queue','lab','arm','tug','hand','spy','laugh','facepalm','yawn',
  'dance','cplay','hang','pat','ask','tea','crack','moust','kickbox','squish','invideo','push','airtype',
  // v4
  'chase','ride','fight','theft','speak','guard','hideicon','kickicon','folder','taboops','couchd',
  'plug','umbrella','pass'];""")

# ---------------------------------------------------------------- v4 starters
sub("function treat(kind){",
    """// ---- v4: кто свободен для дела ----
function freeGuy(prefer){
  let who=null;
  for (const m of men){
    if (!m.grounded || m.act==='grab' || m.pidx>=0) continue;
    if (m.act==='spar'||m.act==='nap'||m.act==='campfire') continue;
    if (SCENE3.includes(m.act)) continue;
    if (!who) who=m;
    if (prefer===1 && (m.name==='Red'||m.name==='Orange')) return m;
    if (prefer===2 && m.name==='Green') return m;
  }
  return who;
}
// спрятать (нарисованную) иконку и вернуть на место
function startHideIcon(){ const s=freeGuy(1); if(!s) return;
  const x=clamp(s.x+s.face*rnd(28,90),46,W-46), y=floor-40*scale;
  props3.push({k:'icon',x,y,ox:x,oy:y,life:rnd(12,18),ph:0,held:true,rot:0,vx:0,vy:0,spin:0});
  s.pidx=props3.length-1; s.hidden=true; s.blocks=0;
  s.act='hideicon'; s.timer=rnd(6,12); s.anim=0; s.face=x>=s.x?1:-1; }
// пнуть нарисованный ярлык
function startIconKick(){ const s=freeGuy(1); if(!s) return;
  const x=clamp(s.x+s.face*rnd(40,130),40,W-40);
  props3.push({k:'icon',x,y:floor-80,vx:s.face*60,vy:0,ox:x,oy:floor-80,rot:0,spin:2,life:rnd(10,15),ph:0});
  s.pidx=props3.length-1; s.act='kickicon'; s.timer=rnd(7,12); s.anim=0;
  s.x=clamp(x-s.face*22*scale,20,W-20); s.face=x>s.x?1:-1;
  if(!anyoneSpeaking()) sayNote(s,18); }
// принести папку к курсору
function startBring(){ const s=freeGuy(2); if(!s) return;
  const x=Math.random()<.5?52*scale:W-52*scale;
  props3.push({k:'folder',x,y:floor,ox:x,oy:floor,life:26,ph:0});
  s.pidx=props3.length-1; s.blocks=0; s.carry=0; s.face=x>=s.x?1:-1;
  travel(s,'folder',x); }
// сорвать вкладку и подклеить
function startTabOops(){ const s=freeGuy(1); if(!s) return;
  const x=clamp(s.x+s.face*rnd(30,120),60,W-60), y=30*scale;
  props3.push({k:'tab',x,y,ox:x,oy:y,life:rnd(16,22),ph:0,held:true,rot:0,vx:0,vy:0,spin:0});
  s.pidx=props3.length-1; s.blocks=0;
  s.act='taboops'; s.timer=6; s.anim=0; s.face=x>=s.x?1:-1; }
// воткнуть вилку в розетку (перегретый ПК / севшая батарея)
function startPlug(){ const s=freeGuy(2); if(!s) return;
  const sx=s.x<W*.5?24*scale:W-24*scale;
  travel(s,'plug',sx); s.carry=9; s.blocks=0; s.timer=rnd(8,11);
  if(!anyoneSpeaking()) sayNote(s,13); }
// сесть на стражу кнопки «Отправить»
function startGuard(){ const s=freeGuy(2); if(!s) return;
  guard={x:clamp(cur.x+90*scale,60,W-60), y:floor, t:0};
  s.blocks=0; travel(s,'guard',guard.x);
  if(!anyoneSpeaking()) sayNote(s,9); }
// зонт: дождь или снег
function startUmbrella(){ const s=freeGuy(0); if(!s) return;
  travel(s,'umbrella',clamp(s.x+rnd(-160,160),80,W-80)); s.carry=10; }
// пас мячом вдвоём
function startPass(){ const s=freeGuy(0); if(!s) return;
  if (!balls.length) balls.push({x:s.x,y:s.y-70*scale,vx:-s.face*60,vy:-120,r:11*scale,rot:0,spin:0,life:34,kick:0});
  const o=men.find(m=>m!==s && m.grounded && m.act!=='grab' && SCENE3.indexOf(m.act)<0 && Math.abs(m.x-s.x)>140);
  if (!o) return;
  s.partner=o.i; o.partner=s.i;
  s.act='pass'; s.timer=rnd(9,15); o.act='pass'; o.timer=rnd(9,15);
  s.face=o.x>=s.x?1:-1; o.face=s.x>=o.x?1:-1; }
// показать бой на мечах для всех
function startShowDuel(){
  let a=freeGuy(1)||men[0], b=men.find(m=>m!==a && m.grounded);
  if(!a||!b) return;
  setAct(a,'spar',12); setAct(b,'spar',12);
  a.partner=b.i; b.partner=a.i; a.carry=0; b.carry=0;
  a.weapon=1; b.weapon=1;
  props3.push({k:'banner',sub:3,x:(a.x+b.x)/2,y:floor,life:13,ph:0});
  for (const o of men) if (o!==a&&o!==b&&o.grounded&&o.act!=='grab'&&SCENE3.indexOf(o.act)<0){
    o.act='watch'; o.timer=rnd(12,18); } }
// написать записку
function startNote(){ const s=freeGuy(0); if(!s) return;
  s.act='speak'; s.timer=rnd(5,8); s.blocks=0; s.face=cur.x>=s.x?1:-1;
  sayNote(s, squadMood>.5?0:7); }
// украсть курсор (его нарисованную копию)
function startTheft(){ const s=freeGuy(1); if(!s) return;
  s.act='theft'; s.timer=7; s.floatT=0; s.blocks=0; s.rides=2; s.carry=13;
  ghost={x:cur.x,y:cur.y}; sayNote(s,15); }

function treat(kind){""")

# ---------------------------------------------------------------- scene behaviour
sub("""          if (s.timer<=0){ if (s.carry===1 && s.act==='tea'){ s.carry=0; } setAct(s,'idle'); s.pet=0; s.inVideo=false; }""",
    """          // ---------- v4: курсор, записки, реквизит ----------
          const sp=Math.hypot(cur.vx,cur.vy);
          if (s.act==='chase'){ s.prop=(s.prop||0)+dt*2.6;
            const dxc=cur.x-s.x, lunge=clamp(-Math.cos(s.prop*2.6)-.30,0,1);
            s.face=dxc>=0?1:-1; s.vx=clamp(dxc*(.8+2.4*lunge),-470,470);
            if (lunge>.72 && Math.abs(dxc)<34*scale){ s.sqV=-2.8; burst(cur.x,cur.y,255,244,150,5);
              if (Math.random()<.2 && !anyoneSpeaking()) sayNote(s,13); } }
          else if (s.act==='ride'){ s.x=lerp(s.x,cur.x,clamp(dt*15,0,1)); s.y=lerp(s.y,cur.y+26*scale,clamp(dt*17,0,1));
            s.grounded=true; s.vx=0; s.vy=0;
            if (Math.abs(cur.vx)>30) s.face=cur.vx>0?1:-1;
            if (sp>800&&Math.random()<.5) parts.push({x:s.x-s.face*12*scale,y:s.y+3,vx:-s.face*120,vy:-34,r:3,l:.4,c:'235,242,250',k:'dust'});
            if (sp>2500){ s.rides=0; s.act='air'; s.grounded=false; s.vy=-60; s.timer=1; } }
          else if (s.act==='theft'){ s.floatT+=dt;
            ghost.x=lerp(ghost.x,J0(s).hR[0]+s.face*5*scale,clamp(dt*11,0,1));
            ghost.y=lerp(ghost.y,J0(s).hR[1]+3*scale,clamp(dt*11,0,1));
            if (s.floatT>2 && !s.blocks){ s.blocks=1; ghost={x:cur.x,y:cur.y}; s.carry=0; s.rides=0;
              sayNote(s,4); burst(cur.x,cur.y,250,252,240,7); s.timer=Math.min(s.timer,1.2); } }
          else if (s.act==='fight'){ s.prop=(s.prop||0)+dt*5.2;
            const dxc=cur.x-s.x, dyc=cur.y-(s.y-46*scale), reach=Math.hypot(dxc,dyc);
            s.face=dxc>=0?1:-1; s.vx=clamp(dxc*.8,-230,230);
            if (Math.sin(s.prop)>.80 && reach<66*scale){ s.blocks++; burst(cur.x,cur.y,255,226,90,4); s.sqV=-.9; }
            if (sp>700 && reach<48*scale && Math.random()<.12){ s.sqV=-2.2; s.vx=-s.face*150;
              burst(J0(s).head[0],J0(s).head[1],250,120,90,4); if(Math.random()<.3&&!anyoneSpeaking()) sayNote(s,12); } }
          else if (s.act==='guard'){ guard.t+=dt; s.x=lerp(s.x,guard.x,clamp(dt*6,0,1)); s.y=guard.y;
            s.grounded=true; s.vx=0; s.vy=0; s.face=-1;
            const d=Math.hypot(cur.x-guard.x,cur.y-(guard.y-30*scale));
            if (d<52*scale){ if(!s.blocks){ s.blocks=1; if(!anyoneSpeaking()) sayNote(s,10); } s.sqV=-1.5;
              if(Math.random()<.25) burst(cur.x,cur.y,255,210,90,3); } else if (d>130*scale) s.blocks=0; }
          else if (s.act==='hideicon'){ const w=props3[s.pidx];
            if (w){ w.held=true;
              if (s.timer>1.8){ w.y=lerp(w.y,H+70,clamp(dt*1.1,0,1)); s.hidden=true;
                if (Math.random()<.02 && !anyoneSpeaking()) sayNote(s,20); }
              else { w.x=lerp(w.x,w.ox,clamp(dt*4.5,0,1)); w.y=lerp(w.y,w.oy,clamp(dt*4.5,0,1));
                if (!s.blocks){ s.blocks=1; s.hidden=false; w.held=false; burst(w.ox,w.oy,255,250,200,7);
                  if(!anyoneSpeaking()) sayNote(s,21); s.timer=Math.min(s.timer,1.2); } }
              s.x=lerp(s.x,w.ox-15*scale,clamp(dt*4,0,1)); s.y=floor; s.face=1; } }
          else if (s.act==='kickicon'){ const w=props3[s.pidx];
            if (w){ const dxp=w.x-s.x; s.face=dxp>=0?1:-1; s.vx=clamp(dxp*1.1,-300,300);
              if (Math.sin(s.anim*3.4)>.93 && Math.abs(dxp)<42*scale){
                w.vx=s.face*rnd(200,360); w.vy=-rnd(120,260); w.spin=s.face*rnd(1.6,4);
                s.sqV=-1; burst(w.x,w.y,255,240,150,3); if(Math.random()<.17&&!anyoneSpeaking()) sayNote(s,11); } } }
          else if (s.act==='folder'){ const w=props3[s.pidx];
            if (!s.blocks && w && Math.abs(w.x-s.x)<42*scale){ s.blocks=1; s.carry=8; props3.splice(s.pidx,1); s.pidx=-1;
              travel(s,'folder',clamp(cur.x,30,W-30)); s.timer=rnd(4.5,8); }
            else if (s.blocks===1 && Math.abs(cur.x-s.x)<38*scale){ s.blocks=2; s.carry=0;
              props3.push({k:'folder',x:clamp(cur.x,40,W-40),y:floor,ox:cur.x,oy:floor,life:11,ph:0});
              burst(clamp(cur.x,40,W-40),floor-10,250,226,120,8); if(!anyoneSpeaking()) sayNote(s,5);
              s.timer=Math.min(s.timer,2); } }
          else if (s.act==='taboops'){ const w=props3[s.pidx];
            if (w){ s.face=w.x>=s.x?1:-1; s.x=lerp(s.x,w.x-s.face*26*scale,clamp(dt*3,0,1)); s.y=floor;
              if (!s.blocks){ s.blocks=1; w.held=false; w.vx=s.face*rnd(40,110); w.vy=20; w.spin=s.face*1.4;
                if(!anyoneSpeaking()) sayNote(s,4); burst(w.x,w.y,240,242,250,5); s.sqV=-1.6; }
              if (s.prop>0 && (s.anim>2.4) && s.blocks===1){ s.blocks=2; s.carry=6; }
              if (s.anim>4 && s.blocks===2){ s.blocks=3; s.carry=0; w.held=true; w.x=w.ox; w.y=w.oy; w.vx=w.vy=w.spin=0;
                burst(w.x,w.y,250,250,210,7); if(!anyoneSpeaking()) sayNote(s,5); s.timer=Math.min(s.timer,1.4); } } }
          else if (s.act==='couchd'){ const target=clamp(couchGoal,130,W-130), dir=target>couch.x?1:-1;
            s.face=dir>0?1:-1; couch.x=lerp(couch.x,target,clamp(dt*.9,0,1)); couch.x=clamp(couch.x,couch.w/2+10,W-couch.w/2-10);
            s.x=lerp(s.x,couch.x-dir*96*scale,clamp(dt*8,0,1)); s.y=floor; s.vx=0; s.vy=0;
            if (Math.abs(couch.x-target)>2*scale && Math.random()<.16)
              parts.push({x:s.x-s.face*8*scale,y:s.y+2,vx:-s.face*40,vy:-20,r:2.4,l:.4,c:'220,210,190',k:'dust'});
            if (Math.abs(couch.x-target)<3*scale && !s.blocks){ s.blocks=1; couchGoal=0;
              if(!anyoneSpeaking()) sayNote(s,17); s.timer=Math.min(s.timer,1.4); } }
          else if (s.act==='plug'){ const sx=s.x<W*.5?16*scale:W-16*scale, dxs=sx-s.x;
            if (!s.blocks){ s.face=dxs>=0?1:-1; s.vx=clamp(dxs*1.3,-320,320);
              if (Math.abs(dxs)<30*scale){ s.blocks=1; s.carry=9; s.timer=Math.min(s.timer,5); } }
            else { s.vx=0; s.face=sx>=s.x?1:-1;
              if (Math.random()<.14) parts.push({x:sx,y:s.y-30*scale,vx:rnd(-50,50),vy:rnd(-90,-20),r:3,l:.35,c:'255,235,120',k:'code'});
              if (s.blocks===1 && s.anim>2.2){ s.blocks=2; battExt=true; battPct=Math.min(100,battPct+18); squadMood=clamp(squadMood+.15,0,1);
                burst(sx,s.y-30*scale,255,240,140,12); } } }
          else if (s.act==='umbrella'){ if (Math.abs(cur.vx)>40) s.face=cur.vx>0?1:-1; s.carry=10;
            if (raining && Math.random()<.5){ for (let i=0;i<2;i++)
              parts.push({x:s.x+rnd(-70,70)*scale,y:s.y-118*scale,vx:rnd(-12,12),vy:rnd(300,460),r:1.6,l:.55,c:'140,195,250',k:'rain'}); }
            if (snowing && Math.random()<.2) parts.push({x:s.x+rnd(-90,90)*scale,y:s.y-130*scale,vx:rnd(-24,24),vy:rnd(26,60),r:3,l:1.7,c:'245,250,255',k:'dust'});
            if (!raining && !snowing && s.timer>1.5) s.timer=1.5; }
          else if (s.act==='pass'){ let b=balls[0]; if (balls.length>1) b=balls.reduce((p,c)=>
              Math.hypot(c.x-s.x,c.y-(s.y-40*scale))<Math.hypot(p.x-s.x,p.y-(s.y-40*scale))?c:p);
            if (!b){ s.timer=0; } else { const o=men.find(m=>m.i===s.partner);
              s.face=b.x>=s.x?1:-1; s.vx=clamp((b.x-s.x)*1.1,-200,200);
              if (s.J){ const dh=Math.hypot(b.x-s.J.hR[0],b.y-s.J.hR[1]);
                if (dh<46*scale && b.kick<=0){ b.kick=1.1;
                  const tx=o?o.x:cur.x, ty=o?o.y-70*scale:cur.y-40*scale;
                  b.vx=clamp((tx-b.x)*1.5,-430,430); b.vy=-rnd(250,380); b.spin=b.vx/b.r; s.sqV=-1.3;
                  burst(b.x,b.y,255,200,90,3);
                  if (o && !SCENE3.includes(o.act)){ o.act='pass'; o.timer=Math.max(o.timer,6); o.partner=s.i; }
                  if (Math.random()<.15 && !anyoneSpeaking()) sayNote(s,19); } } } }
          if (s.speech){ s.speechT+=dt; const tot=s.speech.length; s.seen=Math.min(tot,(s.seen||0)+dt*9);
            if (s.speechT>6.4) s.speech=''; }
          if (s.timer<=0){ if (s.carry===1 && s.act==='tea'){ s.carry=0; }
            const done=s.act; setAct(s,'idle'); s.pet=0; s.inVideo=false;
            if (done==='hideicon'||done==='taboops'){ const w=props3[s.pidx];
              if (w){ w.held=false; w.x=w.ox; w.y=w.oy; } s.pidx=-1; }
            s.hidden=false; s.rides=0; s.carry=0; }""")

# ---------------------------------------------------------------- props physics
sub("""    if (w.k==='box'){ w.vy=(w.vy||0)+1350*dt; w.x+=w.vx*dt; w.y+=w.vy*dt; w.rot=(w.rot||0)+(w.spin||0)*dt;
      if (w.y>floor-9){ w.y=floor-9; w.vy*=-.5; w.vx*=.86; w.spin=w.vx/10; } }""",
    """    if (w.k==='box'||((w.k==='icon'||w.k==='tab')&&!w.held)){
      const grav=w.k==='tab'?340:1350;
      w.vy=(w.vy||0)+grav*dt; if (w.k==='tab'){ w.vx=(w.vx||0)+Math.sin((w.ph||0)*4)*60*dt; w.rot=(w.rot||0)+2.2*dt; }
      w.x+=w.vx*dt; w.y+=w.vy*dt; w.rot=(w.rot||0)+(w.spin||0)*dt; w.spin=(w.spin||0)*Math.pow(.7,dt);
      const fl=floor-(w.k==='tab'?4:9);
      if (w.y>fl){ w.y=fl; if (w.vy>60) burst(w.x,w.y+8,210,215,225,3);
        w.vy*=-((w.k==='tab')?.24:.5); w.vx*=.86; w.spin=w.vx/10; }
      if (w.x<14){ w.x=14; w.vx=Math.abs(w.vx)*.7; }
      if (w.x>W-14){ w.x=W-14; w.vx=-Math.abs(w.vx)*.7; } }""")

# ---------------------------------------------------------------- prop artwork
sub("""    } else if (w.k==='banner'){""",
    """    } else if (w.k==='icon'){
      const k=sc, w0=26*k, h0=21*k;
      ctx.save(); ctx.translate(w.x,w.y); ctx.rotate((w.rot||0)*0.0175);
      ctx.fillStyle='rgba(249,249,246,.96)'; ctx.strokeStyle='rgba(150,156,168,.85)'; ctx.lineWidth=1.4*k;
      ctx.fillRect(-w0/2,-h0/2,w0,h0); ctx.strokeRect(-w0/2,-h0/2,w0,h0);
      ctx.fillStyle='rgba(110,176,240,.94)'; ctx.fillRect(-w0*.38,-h0*.34,w0*.76,h0*.5);
      ctx.fillStyle='rgba(96,190,120,.94)'; ctx.beginPath();
      ctx.moveTo(w0*.30,h0*.14); ctx.lineTo(w0*.02,-h0*.16); ctx.lineTo(-w0*.30,h0*.14); ctx.closePath(); ctx.fill();
      ctx.fillStyle='rgba(250,208,60,.96)'; ctx.beginPath(); ctx.arc(w0*.20,-h0*.16,2.5*k,0,6.283); ctx.fill();
      ctx.fillStyle='rgba(86,190,96,.96)'; ctx.beginPath();
      ctx.moveTo(-w0*.5,h0*.5); ctx.lineTo(-w0*.5,h0*.16); ctx.lineTo(-w0*.16,h0*.5); ctx.closePath(); ctx.fill();
      ctx.restore();
    } else if (w.k==='folder'){
      const k=sc;
      ctx.fillStyle='rgba(246,190,60,.97)'; roundRect(w.x-12*k,w.y-15*k,24*k,7*k,2*k); ctx.fill();
      ctx.fillStyle='rgba(252,214,96,.98)'; roundRect(w.x-13*k,w.y-12*k,26*k,12*k,2.4*k); ctx.fill();
      ctx.strokeStyle='rgba(150,104,40,.8)'; ctx.lineWidth=1.3*k;
      roundRect(w.x-13*k,w.y-12*k,26*k,12*k,2.4*k); ctx.stroke();
    } else if (w.k==='tab'){
      const k=sc, a=clamp(w.life/1.4,0,1);
      ctx.save(); ctx.translate(w.x,w.y); ctx.rotate(w.rot||0);
      ctx.fillStyle=`rgba(236,240,246,${.96*a})`; roundRect(-24*k,-9*k,48*k,18*k,5*k); ctx.fill();
      ctx.strokeStyle=`rgba(150,158,170,${.9*a})`; ctx.lineWidth=1.3*k; roundRect(-24*k,-9*k,48*k,18*k,5*k); ctx.stroke();
      ctx.fillStyle=`rgba(96,176,236,${.95*a})`; ctx.beginPath(); ctx.arc(-16*k,0,3.4*k,0,6.283); ctx.fill();
      ctx.strokeStyle=`rgba(90,96,106,${.9*a})`; ctx.lineWidth=1.5*k;
      ctx.beginPath(); ctx.moveTo(14*k,-3.4*k); ctx.lineTo(19*k,1.6*k); ctx.moveTo(19*k,-3.4*k); ctx.lineTo(14*k,1.6*k); ctx.stroke();
      ctx.restore();
    } else if (w.k==='banner'){""")

# banner 'VS' for the duel
sub("""      ctx.fillText(w.sub===2?'Пора размяться!':(w.sub===1?'миром!':'готово!'), w.x, by-2*sc);""",
    """      ctx.fillText(w.sub===2?'Пора размяться!':(w.sub===1?'миром!':(w.sub===3?'VS ⚔':'готово!')), w.x, by-2*sc);""")

# ---------------------------------------------------------------- carried items
sub("""  if (s.carry===1){ const hx=J.hR[0], hy=J.hR[1]-2*sc;
    ctx.fillStyle='rgba(246,248,250,.97)'; ctx.fillRect(hx-5*sc,hy-8*sc,10*sc,9*sc);
    ctx.fillStyle='rgba(190,120,40,.95)'; ctx.beginPath(); ctx.ellipse(hx,hy-8*sc,4.2*sc,1.3*sc,0,0,6.283); ctx.fill(); }""",
    """  if (s.carry===1){ const hx=J.hR[0], hy=J.hR[1]-2*sc;
    ctx.fillStyle='rgba(246,248,250,.97)'; ctx.fillRect(hx-5*sc,hy-8*sc,10*sc,9*sc);
    ctx.fillStyle='rgba(190,120,40,.95)'; ctx.beginPath(); ctx.ellipse(hx,hy-8*sc,4.2*sc,1.3*sc,0,0,6.283); ctx.fill(); }
  if (s.carry===8){ const hx=J.hR[0], hy=J.hR[1]+4*sc;              // нарисованная папка
    ctx.fillStyle='rgba(246,190,60,.96)'; ctx.fillRect(hx-10*sc,hy-9*sc,20*sc,9*sc);
    ctx.fillStyle='rgba(252,214,96,.97)'; ctx.fillRect(hx-11*sc,hy-6*sc,22*sc,6*sc); }
  if (s.carry===9){ const hx=J.hR[0], hy=J.hR[1];                    // вилка + кабель
    ctx.fillStyle='rgba(60,66,78,.97)'; ctx.fillRect(hx-4*sc,hy-5*sc,9*sc,10*sc);
    ctx.strokeStyle='rgba(230,44,48,.9)'; ctx.lineWidth=2.2*sc; ctx.beginPath();
    ctx.moveTo(J.hL[0],J.hL[1]); ctx.quadraticCurveTo(hx-s.face*22*sc,hy+22*sc,s.x-s.face*40*sc,s.y-4*sc); ctx.stroke(); }
  if (s.carry===10){ const hx=J.hR[0], hy=J.hR[1]-16*sc;             // зонтик
    ctx.strokeStyle='rgba(92,78,66,.97)'; ctx.lineWidth=2*sc;
    ctx.beginPath(); ctx.moveTo(hx,hy-8*sc); ctx.lineTo(hx,hy+18*sc); ctx.stroke();
    ctx.fillStyle='rgba(88,156,236,.95)'; ctx.beginPath(); ctx.arc(hx,hy-8*sc,17*sc,Math.PI,0); ctx.fill(); }
  if (s.carry===13){                                                  // украденный (нарисованный) курсор
    const gx=ghost.x, gy=ghost.y; ctx.fillStyle='rgba(250,250,252,.98)';
    ctx.strokeStyle='rgba(30,32,38,.95)'; ctx.lineWidth=1.4*sc; ctx.beginPath();
    ctx.moveTo(gx,gy); ctx.lineTo(gx,gy+15*sc); ctx.lineTo(gx+4*sc,gy+11*sc); ctx.lineTo(gx+7*sc,gy+18*sc);
    ctx.lineTo(gx+10*sc,gy+16.5*sc); ctx.lineTo(gx+7*sc,gy+10*sc); ctx.lineTo(gx+12*sc,gy+9.5*sc);
    ctx.closePath(); ctx.fill(); ctx.stroke(); }
  // записка на планшете
  if (s.speech){ const shown=Math.max(1,Math.min(s.speech.length,s.seen||1)), a=clamp(s.speechT/.25,0,1)*clamp((6.4-s.speechT)/.6,0,1);
    const bx=clamp(J.head[0]-13*sc-shown*3.05*sc,4,W-8*sc-shown*6.1*sc), by=Math.max(6,J.head[1]-70*scale);
    const bw=26+6.1*shown, bh=22*scale;
    ctx.fillStyle=`rgba(252,250,244,${.96*a})`; ctx.strokeStyle=`rgba(120,116,108,${.9*a})`; ctx.lineWidth=1.6*scale;
    roundRect(bx,by,bw,bh,4.5*scale); ctx.fill(); ctx.stroke();
    ctx.fillStyle=`rgba(46,44,40,${a})`; ctx.font=`600 ${10*scale}px system-ui,sans-serif`; ctx.textAlign='center';
    ctx.fillText(s.speech.slice(0,shown), bx+bw/2, by+bh*0.63); ctx.textAlign='left'; }""")

# ---------------------------------------------------------------- pointer tease
sub("""  if (Math.abs(cur.vx)>1500){ for (const s of men){ if (!s.grounded||s.act==='grab'||!s.J) continue;
    if (Math.hypot(x-s.x,y-s.J.head[1])<60*scale){ s.act='hang'; s.timer=1.8; s.anim=0; break; } } } });""",
    """  if (Math.abs(cur.vx)>1500){ for (const s of men){ if (!s.grounded||s.act==='grab'||!s.J) continue;
    if (Math.hypot(x-s.x,y-s.J.head[1])<60*scale){ s.act='hang'; s.timer=1.8; s.anim=0; break; } } }
  // v4: помахать курсором возле кого-нибудь -> погоня / поездка / кража
  const spd=Math.hypot(cur.vx,cur.vy);
  for (const s of men){ if (!s.grounded||s.act==='grab'||!s.J) continue;
    const d=Math.hypot(x-s.x,y-(s.y-46*scale));
    if (d<150*scale && spd>260) tease=Math.min(3,tease+0.09);
    if ((s.act==='idle'||s.act==='walk'||s.act==='travel') && d<120*scale && tease>1.15 && spd>260 && Math.random()<.02){
      s.act='chase'; s.timer=rnd(2.6,4.6); s.anim=0; s.prop=0; s.face=x>=s.x?1:-1; s.sqV=-1.6; tease=0; }
    if (s.act==='idle' && d<60*scale && spd<60 && y-(s.y-46*scale)>6*scale && y-(s.y-46*scale)<40*scale && Math.random()<.004){
      s.act='ride'; s.timer=rnd(4,9); s.anim=0; s.rides=1; s.sqV=-2.2; }
    if (s.act==='idle' && tease>2.1 && Math.random()<.02){
      s.act='theft'; s.timer=7; s.anim=0; s.floatT=0; s.blocks=0; s.rides=2; s.carry=13;
      ghost={x,y}; tease=0; sayNote(s,15); } }
  tease=Math.max(0,tease-0.012); });""")

# ---------------------------------------------------------------- poke back
sub("""      if (e.button===2){ s.act='flinch'; s.timer=1.1; s.vy=-300; s.vx=-s.face*160; s.grounded=false; s.sqV=-2.6; burst(x,y,255,232,120,8); break; }""",
    """      if (e.button===2){ s.act='flinch'; s.timer=1.1; s.vy=-300; s.vx=-s.face*160; s.grounded=false; s.sqV=-2.6; burst(x,y,255,232,120,8);
        pokes++; pokeT=0; s.mood=Math.max(0,s.mood-.08); squadMood=Math.max(0,squadMood-.04);
        if (pokes>=3){ pokes=0;
          if (Math.random()<.5){ s.act='fight'; s.timer=rnd(4,7); s.anim=0; s.face=x>=s.x?1:-1; sayNote(s,3); }
          else if (!anyoneSpeaking()){ s.act='speak'; s.timer=rnd(4,6); sayNote(s,3); } }
        for (const o of men){ if (o!==s && Math.abs(o.x-s.x)<240 && Math.random()<.34){ o.act='laugh'; o.timer=rnd(2.6,4.6); } }
        break; }""")

# ---------------------------------------------------------------- weather + mood tick
sub("""function step(dt){""",
    """function moodTick(dt){
  // батарейка (если браузер её отдаёт) + погода по сезону, либо вручную
  if (navigator.getBattery && !battProbed){ battProbed=true;
    try{ navigator.getBattery().then(b=>{ const upd=()=>{ if(!battForced){ battPct=Math.round(b.level*100); battExt=b.charging; } };
      upd(); b.addEventListener('levelchange',upd); b.addEventListener('chargingchange',upd); }); }catch(e){} }
  pokes=Math.max(0,pokes-dt*.22);
  const low = battPct>=0 && !battExt && battPct<=15;
  if (low && !battLow){ battLow=true; const s=freeGuy(1); if (s){ s.act='speak'; s.timer=6.5; sayNote(s,15); }
    for (const o of men) if (o.act==='idle'&&Math.random()<.5){ o.act='run'; o.timer=1.6; } }
  if (!low) battLow=false;
  wTimer-=dt;
  if (wTimer<=0){ wTimer=240;
    if (weatherOn){ const m=new Date().getMonth()+1, snow=(m===12||m===1||m===2), wet=(m===3||m===4||m===10||m===11);
      if (weatherMode===1){ raining=true; snowing=false; }
      else if (weatherMode===2){ raining=false; snowing=true; }
      else if (weatherMode===3){ raining=false; snowing=false; }
      else { snowing=snow&&Math.random()<.5; raining=!snowing&&(wet||m<=3)&&Math.random()<.34; }
      if (raining||snowing) wTimer=6; }
    else { raining=false; snowing=false; } }
  if ((raining||snowing) && !couchOn){ wTimer=Math.max(0,wTimer-dt);
    if (wTimer<.05){ const s=freeGuy(0); if (s && s.act!=='umbrella') startUmbrella(); } }
  const target=(performance.now()<afkUntil?.35:.55)+(couchOn||deskOn?.12:0)+(snacks.length||balls.length?.12:0);
  squadMood=lerp(squadMood,clamp(target,0,1),clamp(dt*.06,0,1));
  for (const s of men){ s.mood=lerp(s.mood,squadMood,clamp(dt*.25,0,1));
    if (raining&&Math.random()<.08) parts.push({x:s.x+rnd(-60,60),y:Math.min(s.y-120*scale,0)-6,vx:rnd(-10,10),vy:rnd(320,460),r:1.5,l:1.2,c:'140,195,250',k:'rain'}); }
  if (raining||snowing){ for (let i=0;i<(raining?2:1);i++)
    parts.push({x:rnd(0,W),y:-6,vx:rnd(-14,14),vy:raining?rnd(420,620):rnd(26,60),r:raining?1.6:rnd(2,4),
      l:raining?1.4:rnd(2.4,4.6),c:raining?'140,195,250':'245,250,255',k:raining?'rain':'dust'}); }
}
function step(dt){
  moodTick(dt);""")

# rain particle rendering + J0 helper
sub("""    else { ctx.beginPath(); ctx.arc(p.x,p.y,p.r*a,0,6.283); ctx.fill(); } }""",
    """    else if (p.k==='rain'){ ctx.lineWidth=1.5; ctx.beginPath(); ctx.moveTo(p.x,p.y); ctx.lineTo(p.x-1.2,p.y+9); ctx.stroke(); }
    else { ctx.beginPath(); ctx.arc(p.x,p.y,p.r*a,0,6.283); ctx.fill(); } }""")

sub("function drawMan(s){", "function J0(s){ return s.J||{head:[s.x,s.y-46*scale],hR:[s.x,s.y-40*scale],hL:[s.x,s.y-40*scale]}; }\nfunction drawMan(s){")

# ---------------------------------------------------------------- buttons
sub("""    <select id="selScale">""",
    """    <button id="bChase">🎯 Погоня за курсором</button>
    <button id="bRide">🛴 Прокатиться на курсоре</button>
    <button id="bFight">🥊 Бой с курсором</button>
    <button id="bTheft">🖱️ Украсть курсор</button>
    <button id="bNote">📝 Написать записку</button>
    <button id="bGuard">🛡️ Охранять «Отправить»</button>
    <button id="bHide">🫥 Спрятать иконку</button>
    <button id="bKickIcon">🦵 Пнуть ярлык</button>
    <button id="bFolder">📁 Принести папку</button>
    <button id="bTab">🗂️ Уронить вкладку</button>
    <button id="bCouchDrag">🛋️ Оттащить диван</button>
    <button id="bPlug">🔌 Воткнуть в розетку</button>
    <button id="bUmb">☔ Зонт (погода)</button>
    <button id="bPass">⚽ Пас мячом</button>
    <button id="bDuelShow">⚔️ Шоу: бой на мечах</button>
    <button id="bBatt">🔋 Батарея 12%</button>
    <select id="selScale">""")

sub("document.getElementById('selScale').onchange=",
    """document.getElementById('bChase').onclick=()=>{ const s=freeGuy(0); if(!s) return;
  s.act='chase'; s.timer=rnd(2.6,4.6); s.anim=0; s.prop=0; s.face=cur.x>=s.x?1:-1; s.sqV=-1.6; };
document.getElementById('bRide').onclick=()=>{ const s=freeGuy(0); if(!s) return;
  s.act='ride'; s.timer=rnd(4,9); s.anim=0; s.rides=1; s.sqV=-2.2; };
document.getElementById('bFight').onclick=()=>{ const s=freeGuy(1); if(!s) return;
  s.act='fight'; s.timer=rnd(4,7.5); s.anim=0; s.prop=0; s.blocks=0; s.face=cur.x>=s.x?1:-1; sayNote(s,3); };
document.getElementById('bTheft').onclick=()=>startTheft();
document.getElementById('bNote').onclick=()=>startNote();
document.getElementById('bGuard').onclick=()=>startGuard();
document.getElementById('bHide').onclick=()=>startHideIcon();
document.getElementById('bKickIcon').onclick=()=>startIconKick();
document.getElementById('bFolder').onclick=()=>startBring();
document.getElementById('bTab').onclick=()=>startTabOops();
document.getElementById('bCouchDrag').onclick=()=>{ const s=freeGuy(0); if(!s) return;
  couchGoal=clamp(cur.x,150,W-150); s.blocks=0; travel(s,'couchd',couch.x+(couchGoal>couch.x?-1:1)*110*scale);
  s.timer=6+Math.abs(couchGoal-couch.x)/80; if(!anyoneSpeaking()) sayNote(s,15); };
document.getElementById('bPlug').onclick=()=>startPlug();
document.getElementById('bUmb').onclick=e=>{ weatherOn=!weatherOn; if(weatherOn){ weatherMode=(weatherMode+1)%4;
    raining=weatherMode===1; snowing=weatherMode===2; wTimer=weatherMode===0?0.05:600; } else { raining=snowing=false; }
  e.target.classList.toggle('on',weatherOn);
  e.target.textContent=['☔ Погода: выкл','☀️ Погода: ясно','🌧 Погода: дождь','❄️ Погода: снег'][weatherOn?weatherMode:0];
  if (weatherOn){ const s=freeGuy(0); if (s) startUmbrella(); } };
document.getElementById('bPass').onclick=()=>startPass();
document.getElementById('bDuelShow').onclick=()=>startShowDuel();
document.getElementById('bBatt').onclick=e=>{ const on=!battForced; battForced=on; if(on){ battPct=12; battExt=false; } else { battPct=-1; battExt=true; battProbed=false; }
  e.target.classList.toggle('on',on); };
document.getElementById('selScale').onchange=""")

io.open(PATH, "w", encoding="utf-8").write(s)
print("preview patched;", len(orig), "->", len(s), "bytes")
