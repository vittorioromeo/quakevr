"""Run wrist seam, blood, skin tone and menu regression; make a compact visual contact sheet."""
from pathlib import Path
import subprocess,re,sys,time,shutil
from PIL import Image,ImageDraw
import numpy as np
ROOT=Path(__file__).resolve().parents[2]
KIT=Path('C:/OHWorkspace/qvr-kit')
BASH='C:/Program Files/Git/bin/bash.exe'
base=('vr_fixed_frames 1;vr_particle_seed 7;map vrfiringrange;wait60;god;notarget;fog 0;'
      'vr_exposure 1;vr_bloom 0;vr_tonemap 0;vr_weapon_reflections 0;vr_body_mode 3;vr_hand_rig 1;vr_body_blood 0;vr_body_blood_floor 0;'
      'vr_wounds 1;vr_skin_tone 0;vr_shot_hide 0;impulse 150;vr_body_powerups 0;'
      'vr_mock_hand off 0.10 1.22 -0.34 0 -75 0;vr_mock_hand main -0.08 1.02 -0.30 0 75 0;'
      'vr_mock_hand head 0 1.6 0 -45 0 0;')
if '--visual-only' not in sys.argv:
 parts=[]
 for bracers,source,hand in [(0,'hand','main'),(0,'arm','off'),(1,'hand','main'),(1,'arm','off')]:
  parts.append(base+f'vr_body_bracers {bracers};wait20;vr_wrist_blood_test {source} {hand};wait10;vr_body_wrist_info;vr_gore_hands_info')
 script=';'.join(parts)+';vr_menu_path_check maps/vrcalibration.map;toggleconsole;quit'
 (ROOT/'scratch/wrist_runtime.cfg').write_text(script)
 r=subprocess.run([BASH,str(KIT/'run.sh'),'wrist2','-Script',script,'-ExtraArgs','-nomapindex',
                   '-Filter','wrist|straps|missing|blood|ERROR|rror|Fatal'],capture_output=True,text=True)
 text=r.stdout+r.stderr;(ROOT/'scratch/wrist_runtime.log').write_text(text)
 assert r.returncode==0 and 'exit=0' in text,text
 assert not re.search('ENGINE CRASH|Host_Error|Sys_Error|Fatal|ENGINE ERROR',text),text
 blood=re.findall(r'wrist blood: from (arm|hand) (main|off), body (\d+) -> (\d+), hand (\d+) -> (\d+)',text)
 assert len(blood)==4,text
 for source,hand,b0,b1,h0,h1 in blood:
  assert int(b1)>int(b0) and int(h1)>int(h0),(source,hand,b0,b1,h0,h1)
  print(f'PASS blood from {source} {hand}: body +{int(b1)-int(b0)}, hand +{int(h1)-int(h0)} texels')
 mounts=re.findall(r'wrist mount: bracers (\d), valid 1, origin ([\d.-]+) ([\d.-]+) ([\d.-]+), straps (\d)',text)
 assert len(mounts)==4 and all(m[-1]=='2' for m in mounts),text
 for m in mounts[1:]:
  assert max(abs(float(a)-float(b)) for a,b in zip(mounts[0][1:4],m[1:4]))<.002,(mounts[0],m)
 assert text.count("arm's wrist off the hand's 0.00 mm")==8,text
 assert '0 missing' in text,text
 print('PASS all modes: two straps, stable mount, eight aligned wrists, calibration 0 missing')
# Four clean poses followed by dark/authored/light skin and the bloody version in the two bracer modes.
script=base+'vr_wounds 0;vr_dither 0;vr_foveated 0;'
for pitch,yaw in [(25,0),(0,75)]:
 script+=f'vr_mock_hand main -0.08 1.02 -0.30 {pitch} {yaw} 0;'
 for bracers in (0,1):
  script+=f'vr_body_bracers {bracers};wait20;vr_body_wrist_info;vr_eyeshot 1;wait2;'
for bracers in (0,1):
 script+=f'vr_body_bracers {bracers};'
 for tone in (-1,0,1):
  script+=f'vr_skin_tone {tone};wait10;vr_eyeshot 1;wait2;'
script+='vr_wounds_blood_alpha 1;vr_skin_tone 0;vr_wounds 1;vr_body_bracers 0;wait10;vr_gore_hands_test main 1.5;vr_gore_hands_test off 1.5;wait10;'
for bracers in (0,1):
 for tone in (0,1):
  script+=f'vr_body_bracers {bracers};vr_skin_tone {tone};wait10;vr_eyeshot 1;wait2;'
script+='toggleconsole;quit'
(ROOT/'scratch/wrist_visual.cfg').write_text(script)
started=time.time()
r=subprocess.run([BASH,str(KIT/'run.sh'),'wrist2','-Script',script,'-ExtraArgs','-nomapindex',
                  '-Filter','wrist|ERROR|rror|Fatal|shader'],capture_output=True,text=True)
text=r.stdout+r.stderr;(ROOT/'scratch/wrist_visual.log').write_text(text)
assert r.returncode==0 and 'exit=0' in text,text
assert not re.search('ENGINE CRASH|Host_Error|Sys_Error|Fatal|ENGINE ERROR',text),text
print('PASS visual capture: four bent/straight bracer poses, six skin tones, four bloody modes')

shots=KIT/'bases/wrist2/qbase/quakevr/eyeshots'
files=sorted((p for p in shots.glob('*_L.png') if p.stat().st_mtime>=started-1),key=lambda p:p.stat().st_mtime)
assert len(files)==14,[(p.name,p.stat().st_mtime-started) for p in files]
images=[]
for i,p in enumerate(files):
 out=ROOT/f'scratch/wrist_visual_{i:02d}.png';shutil.copy2(p,out);images.append(Image.open(out).convert('RGB'))
print('Eye captures:',len(images),images[0].size)
for dark,light in [(4,6),(7,9)]:
 delta=np.max(np.abs(np.array(images[dark]).astype(np.int16)-np.array(images[light]).astype(np.int16)),axis=2)
 changed=int((delta>10).sum()); assert changed>1000,changed
 print(f'tone pixels changed {dark}/{light}: {changed}')
# Blood-covered interiors should retain colour when tone goes from authored to light. Erode the colour selection
# so silhouette anti-aliasing and tiny differences in settled hand placement do not count as a colour change.
for i in (10,12):
 a=np.array(images[i]).astype(np.int16);b=np.array(images[i+1]).astype(np.int16)
 mask=(a[:,:,0]>a[:,:,1]*4)&(a[:,:,0]>a[:,:,2]*4)&(a[:,:,0]>40)
 for _ in range(3):
  pad=np.pad(mask,1);mask=np.logical_and.reduce([pad[y:y+mask.shape[0],x:x+mask.shape[1]] for y in range(3) for x in range(3)])
 delta=np.max(np.abs(a-b),axis=2)[mask]
 print(f'blood exclusion bracers {(i-10)//2}: {len(delta)} interior pixels, p95 delta {np.percentile(delta,95):.1f}, unchanged<=2 {(delta<=2).mean()*100:.2f}%')
 assert len(delta)>100 and (delta<=2).mean()>.95
# Full-eye pairs keep the hands and both wrists in view.
rows=[(0,1,'Straight: bare / bracers'),(2,3,'Bent: bare / bracers'),(4,6,'Bare: dark / light'),(7,9,'Bracers: dark / light'),(10,11,'Blood: authored / light'),(12,13,'Bracer blood: authored / light')]
sheet=Image.new('RGB',(1200,6*330),(12,12,12));draw=ImageDraw.Draw(sheet)
for row,(left,right,label) in enumerate(rows):
 draw.text((8,row*330+5),label,fill='white')
 for col,i in enumerate((left,right)):
  im=images[i];w,h=im.size;crop=im.copy();crop.thumbnail((600,305));sheet.paste(crop,(col*600,row*330+24))
sheet.save(ROOT/'scratch/wrist_visual.png')
print('PASS fourteen eye captures; saved wrist_visual.png')
