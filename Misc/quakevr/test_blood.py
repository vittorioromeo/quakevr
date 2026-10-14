"""Blood-feedback regression against the production engine and GLSL (local kit)."""
import argparse,json,re,subprocess
from pathlib import Path
ap=argparse.ArgumentParser()
ap.add_argument("--name",default="bloodfx")
ap.add_argument("--kit",default="C:/OHWorkspace/qvr-kit")
ap.add_argument("--images",action="store_true")
args=ap.parse_args()
root=Path(__file__).resolve().parents[2]
scratch=root/"scratch"
scratch.mkdir(exist_ok=True)
bash="C:/Program Files/Git/bin/bash.exe"
def run(label,commands,extra=()):
    p=subprocess.run([bash,args.kit+"/run.sh",args.name,"-Script",";".join(commands+["toggleconsole","quit"]),
        "-Filter","blood|BLOOD_CASE|missing|ERROR|Error|FATAL|ENGINE",*extra],capture_output=True,text=True)
    (scratch/(label+".log")).write_text(p.stdout+p.stderr)
    assert p.returncode==0,(label,p.stdout[-2000:],p.stderr[-1000:])
    assert not re.search(r"Host_Error|Sys_Error|FATAL|ERROR:",p.stdout+p.stderr)
    return p.stdout
def mark(label,cmd):
    return ["echo BLOOD_CASE_"+label]+cmd+["vr_blood_info"]
init=["vr_enabled 1","vr_fixed_frames 1","vr_fixed_frames_rate 72","vr_mock_look 0 0","v_kickpitch 0","v_kickroll 0","map e1m1","wait120","god","vr_blood_hit 1","vr_blood_health 1","vr_blood_hit_duration 1",
      "vr_blood_hit_intensity 0.6","vr_blood_hit_bias 1","vr_blood_health_threshold 35",
      "vr_blood_health_intensity 0.25","vr_blood_health_rate 0.75","vr_blood_health_rate_scale 2",
      "vr_blood_health_curve 1","vr_blood_health_duration 0.25","give h 100","wait2"]
commands=init+["vr_menu_path_check maps/vrcalibration.map"]
for mode in ["left","right","front","rear","up","down","neutral","armor"]:
    commands+=mark(mode,["vr_blood_test clear","vr_blood_test packet "+mode,"wait2"])
commands+=mark("turn_before",["vr_blood_test clear","vr_blood_test right","wait2"])
commands+=mark("turn_after",["vr_mock_look 0 180","wait10"])
commands+=["vr_mock_look 0 0","vr_blood_test clear"]
commands+=mark("pause_start",["vr_blood_test right","pause","wait2"])
commands+=mark("pause_end",["wait72"])
commands+=mark("expire",["pause","wait90"])
commands+=mark("timing_start",["vr_blood_test clear","vr_blood_test right"])
commands+=mark("timing_half",["wait36"])
commands+=mark("intensity_half",["vr_blood_hit_intensity 0.3"])
commands+=mark("duration_double",["vr_blood_hit_duration 2"])
commands+=["vr_blood_hit_duration 1","vr_blood_hit_intensity 0.6","vr_blood_test clear"]
commands+=mark("hit_off",["vr_blood_hit 0","vr_blood_test right","wait2"])
commands+=mark("health20",["give h 20","wait2"])
commands+=mark("health_off",["vr_blood_health 0","wait2"])
commands+=mark("health10",["vr_blood_health 1","give h 10","wait2"])
commands+=mark("healed",["give h 35","wait2"])
commands+=mark("death",["give h 0","wait2"])
commands+=mark("respawn",["give h 100","wait2"])
commands+=["vr_blood_hit 1"]
commands+=mark("save",["map e1m1","wait120","give h 20","wait2","save blood_lifecycle"])
commands+=mark("load",["vr_blood_test right","load blood_lifecycle","wait120"])
commands+=mark("map",["map e1m1","wait120"])
commands+=mark("disconnect",["disconnect","wait2"])
for health in [35,34,30,20,10,1,0]:
    commands+=["echo BLOOD_SAMPLE_"+str(health),"vr_blood_test sample "+str(health)+" 0"]
commands+=["vr_blood_health_curve 2","echo BLOOD_CURVE","vr_blood_test sample 20 0",
           "vr_blood_health_curve 1","vr_blood_health_rate_scale 0","echo BLOOD_RATE_FIXED","vr_blood_test sample 1 0",
           "vr_blood_health_rate_scale 2","vr_blood_health_intensity 0","echo BLOOD_INTENSITY_ZERO","vr_blood_test sample 1 0"]
out=run("blood_quantitative",commands)
cases={}
for block in re.split("BLOOD_CASE_",out)[1:]:
    label=block.split()[0]
    edges=re.search(r"blood edges: left ([0-9.]+) right ([0-9.]+) up ([0-9.]+) down ([0-9.]+) yaw ([-0-9.]+)",block)
    state=re.search(r"blood: health (-?\d+) severity ([0-9.]+) peak ([0-9.]+) rate ([0-9.]+) pulse ([0-9.]+) hit ([0-9.]+) clock ([0-9.]+) phase ([0-9.]+) paused (\d+)",block)
    if edges and state: cases[label]={"edges":list(map(float,edges.groups())),"state":list(map(float,state.groups()))}
assert len(cases)==28,(len(cases),list(cases))
for a,b,axis in [("left","right",0),("up","down",2),("front","rear",2)]:
    assert cases[a]["edges"][axis]>0.3 and cases[b]["edges"][axis]<0.01,(a,b,cases[a],cases[b])
assert abs(cases["neutral"]["edges"][0]-cases["neutral"]["edges"][1])<0.001
assert cases["armor"]["state"][5]>0.3
assert cases["turn_before"]["edges"][1]>cases["turn_before"]["edges"][0]
assert cases["turn_after"]["edges"][0]>cases["turn_after"]["edges"][1]
assert cases["pause_start"]["state"][6]==cases["pause_end"]["state"][6]
assert 0.10 < cases["timing_half"]["state"][5] < 0.20,cases["timing_half"]
assert abs(cases["intensity_half"]["state"][5]/cases["timing_half"]["state"][5]-0.5)<0.02
assert cases["duration_double"]["state"][5]>cases["intensity_half"]["state"][5]*1.8
for name in ["expire","hit_off","health_off","healed","death","respawn","map","disconnect"]:
    assert max(cases[name]["edges"][:4])<0.001,(name,cases[name])
assert cases["load"]["state"][0]==20 and cases["load"]["state"][5]==0
samples=[]
for block in re.split("BLOOD_SAMPLE_",out)[1:]:
    label=int(block.split()[0])
    m=re.search(r"blood sample: severity ([0-9.]+) peak ([0-9.]+) rate ([0-9.]+) pulse ([0-9.]+)",block)
    samples.append([label,*map(float,m.groups())])
assert all(samples[i][2]<samples[i+1][2] and samples[i][3]<samples[i+1][3] for i in range(1,5)),samples
assert samples[0][2]==0 and samples[-1][2]==0
assert re.search(r"0 missing",out),out[-2000:]
report={"cases":cases,"samples":samples}
(scratch/"blood_results.json").write_text(json.dumps(report,indent=2))
print("blood: quantitative direction, armor, pause, expiry, toggles, threshold, healing, lifecycle and curves PASS")
if args.images:
    base=["vr_enabled 1","vr_fixed_frames 1","vr_fixed_frames_rate 72","vr_mock_look 0 0","map e1m1","wait120","god","give h 100","wait2","pause","vr_window_view 1",
          "vr_blood_hit_duration 1","vr_blood_hit_intensity 0.6","vr_blood_hit_bias 1",
          "vr_blood_health_threshold 35","vr_blood_health_intensity 0.6","vr_blood_health_rate 0.75",
          "vr_blood_health_rate_scale 2","vr_blood_health_curve 1","vr_blood_health_duration 0.25"]
    shots=["vr_blood_hit 0","vr_blood_health 0","wait2","screenshot","vr_eyeshot 1","wait2",
           "vr_blood_hit 1","vr_blood_health 1","wait2","screenshot","vr_eyeshot 1","wait2",
           "vr_blood_test left","wait2","screenshot","vr_eyeshot 1","wait2",
           "vr_blood_test clear","vr_blood_test right","wait2","screenshot","vr_eyeshot 1","wait2",
           "vr_blood_test clear","pause","give h 1","wait2","pause","wait2","screenshot","vr_eyeshot 1","wait2",
           "vr_blood_info","vr_comfort_fade 5","wait2","screenshot","vr_eyeshot 1","wait2",
           "v_cshift 0 0 0 255","wait2","screenshot","vr_eyeshot 1","wait2"]
    run("blood_visual",base+shots,["-Clean","-Out","blood_visual.png"])
    run("blood_flat",["vr_enabled 0"]+base[1:]+shots,["-Clean","-Out","blood_flat.png"])
    print("blood: visual evidence captured (off / inactive / left / right / health1 / fade)")
