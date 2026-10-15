"""GPU regression for independent blood coverage (run with the bundled image Python)."""
import argparse,json,re,shutil,subprocess
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
ap=argparse.ArgumentParser()
ap.add_argument("--name",default="bloodfx")
ap.add_argument("--kit",default="C:/OHWorkspace/qvr-kit")
ap.add_argument("--tracy-smoke",action="store_true")
args=ap.parse_args()
root=Path(__file__).resolve().parents[2]
out=root/"scratch/blood_size"
out.mkdir(parents=True,exist_ok=True)
bash="C:/Program Files/Git/bin/bash.exe"
cases=[
 ("off",1,1,0,0),
 ("hit_small",1,0,.2,.65),("hit_default",1,0,.65,.65),("hit_wide",1,0,1,.65),
 ("hit_other_size",1,0,1,1),("hit_toggle_off",0,0,1,1),
 ("health_small",0,1,.65,.2),("health_default",0,1,.65,.65),("health_wide",0,1,.65,1),
 ("health_other_size",0,1,1,1),("health_toggle_off",0,0,1,1),
 ("hit_only_size",1,1,1,0),("health_only_size",1,1,0,1),
 ("combined_default",1,1,.65,.65),("combined_wide",1,1,1,1),
 ("clamp_low",1,1,-99999,-99999),("clamp_high",1,1,99999,99999),
 ("invalid",1,1,"nan","nan")]
base=["vr_fixed_frames 1","vr_fixed_frames_rate 72","vr_mock_look 0 0","vr_window_view 0",
      "map e1m1","wait120","god","give h 1","wait2","pause","vr_blood_test clear",
      "vr_blood_hit_duration 1","vr_blood_hit_intensity 0.22","vr_blood_hit_bias 0.75",
      "vr_blood_health_threshold 35","vr_blood_health_intensity 0.25","vr_blood_health_duration 0.25",
      "vr_blood_health_rate 0.75","vr_blood_health_rate_scale 2","vr_blood_health_curve 1",
      "vr_blood_hit 1","vr_blood_test neutral"]
def run(label,cmds,extra):
 p=subprocess.run([bash,args.kit+"/run.sh",args.name,"-Script",";".join(cmds+["toggleconsole","quit"]),
     "-Filter","blood|BLOOD|missing|Error|ERROR|FATAL|ENGINE",
     "-ExtraArgs","-noconfigwrite -noaddons",*extra],capture_output=True,text=True)
 (out/(label+".log")).write_text(p.stdout+p.stderr,encoding="utf-8")
 assert p.returncode==0 and not re.search("Host_Error|Sys_Error|ERROR:|FATAL",p.stdout+p.stderr),(label,p.stdout[-2000:])
 return p.stdout
images={}
for mode in ["vr","flat"]:
 cmds=["vr_enabled "+("1" if mode=="vr" else "0")]+base
 cmds+=["vr_menu_path_check maps/vrcalibration.map"]
 for label,hit,health,hit_size,health_size in cases:
  cmds+=["echo BLOOD_SIZE_"+label,"vr_blood_hit "+str(hit),"vr_blood_health "+str(health),
      "vr_blood_hit_size "+str(hit_size),"vr_blood_health_size "+str(health_size),"vr_blood_test clear"]
  if hit: cmds+=["vr_blood_test neutral"]
  cmds+=["wait3",
      "vr_blood_info","screenshot"]
  if mode=="vr": cmds+=["vr_eyeshot 1","wait3"]
 log=run(mode,cmds,["-Clean"])
 assert "0 missing" in log
 # Copy raw evidence before the next run overwrites the normal screenshot folder.
 paths=sorted((root/"quakevr/screenshots").glob("*.png"),key=lambda p:p.stat().st_mtime_ns)
 assert len(paths)==len(cases),(mode,len(paths))
 for i,(label,*_) in enumerate(cases):
  shutil.copyfile(paths[i],out/(mode+"_"+label+".png"))
  if mode=="vr":
   for eye in ["L","R"]:
    src=root/f"quakevr/eyeshots/e1m1_{i:03d}_{eye}.png"
    shutil.copyfile(src,out/(eye+"_"+label+".png"))
for view in ["L","R","flat"]:
 data={label:np.asarray(Image.open(out/(view+"_"+label+".png")).convert("RGB")).astype(float) for label,*_ in cases}
 baseline=data["off"];height,width=baseline.shape[:2]
 yy,xx=np.mgrid[:height,:width]
 centre=((xx/width*2-1)**2+(yy/height*2-1)**2)<.12**2
 scene=np.ones((height,width),bool)
 if view=="flat":
  scene[yy/height>.68]=False # HUD/hands are not part of a screen-space coverage invariant.
  scene[(xx/width>.3)&(xx/width<.65)&(yy/height>.38)&(yy/height<.58)]=False # PAUSED banner.
 delta={label:np.max(np.abs(a-baseline),axis=2) for label,a in data.items()}
 report={}
 for effect in ["hit","health"]:
  names=[effect+"_small",effect+"_default",effect+"_wide"]
  counts=[int(np.sum((delta[name]>2)&scene)) for name in names]
  assert counts[0]<counts[1]<counts[2],(view,effect,counts)
  for a,b in zip(names,names[1:]):
   assert int(np.sum((delta[a]>delta[b]+2)&scene))==0,(view,a,b)
  report[effect+"_pixels"]=counts
 for a,b in [("hit_wide","hit_other_size"),("health_wide","health_other_size"),
             ("hit_wide","hit_only_size"),("health_wide","health_only_size"),
             ("off","hit_toggle_off"),("off","health_toggle_off"),("off","clamp_low"),
             ("off","invalid"),("combined_wide","clamp_high")]:
  difference=float(np.max(np.abs(data[a]-data[b])[scene]))
  assert difference<=1,(view,a,b,difference)
 for label in data:
  assert float(np.max(np.abs(data[label]-baseline)[centre]))<=1,(view,label,"centre changed")
 report["independence_max"]=max(float(np.max(np.abs(data[a]-data[b])[scene])) for a,b in [
       ("hit_wide","hit_other_size"),("health_wide","health_other_size"),
       ("hit_wide","hit_only_size"),("health_wide","health_only_size")])
 report["centre_max"]=max(float(np.max(np.abs(a-baseline)[centre])) for a in data.values())
 images[view]=report
# Compact rows: hit off/default/wide, health off/default/wide. Original defaults for intensity.
for mode in ["vr","flat"]:
 board=Image.new("RGB",(960,584),(12,12,12));draw=ImageDraw.Draw(board)
 for row,effect in enumerate(["hit","health"]):
  for col,label in enumerate(["off",effect+"_default",effect+"_wide"]):
   source=("L" if mode=="vr" else mode)+"_"+label+".png"
   image=Image.open(out/source).convert("RGB").resize((320,270))
   x=col*320;y=row*292
   board.paste(image,(x,y+22))
   draw.text((x+8,y+5),effect+" "+("off" if col==0 else "default 0.65" if col==1 else "wide 1.00"),fill=(255,255,255))
 board.save(out/(mode+"_comparison.png"))
(out/"results.json").write_text(json.dumps(images,indent=2))
print(json.dumps(images))
print("blood coverage: GPU monotonicity, independent sizes/toggles, bounds and clear centre PASS")
if args.tracy_smoke:
 log=run("tracy_smoke",["vr_enabled 1"]+base+["vr_blood_hit_size .65","vr_blood_health_size 1","wait3","vr_blood_info","screenshot"],["-Tracy"])
 assert "blood size: hit 0.650 health 1.000" in log
 print("blood coverage: Tracy smoke PASS")
