#!/usr/bin/env python3
"""Reconstructed working Atlas 310B1 3×U-Net OM + OpenCV pipeline.

Output layout remains compatible with the previous standard-library GUI:
 output_pipeline/masks/*_mask.png; overlays/*_composite_overlay.jpg;
 summary/{image_trait_summary.csv, image_trait_summary_detail.json,
 vigor_score.json, growth_curve_skeleton_length.png}

The score is an UNCALIBRATED, heuristic baseline. It is not a validated
seed-vigor scale and does not recover the 4 YOLO germination stages.
"""
import argparse,csv,json,re,os
from pathlib import Path
from datetime import datetime
import cv2,numpy as np

ROOT=Path(os.environ.get('SEEDGERM_ATLAS_ROOT','/root/seedgerm_atlas'))
COLORS={'overall':(255,0,0),'seed':(0,255,0),'seedling':(0,0,255)}

def datetime_from_name(path):
 m=re.search(r'(\d{8})[_-](\d{4})',Path(path).stem)
 return datetime.strptime(m.group(1)+m.group(2),'%Y%m%d%H%M') if m else None

def preprocess_unet(img):
 rgb=cv2.cvtColor(img,cv2.COLOR_BGR2RGB)
 x=cv2.resize(rgb,(512,512),interpolation=cv2.INTER_LINEAR).astype(np.float32)/255
 return np.ascontiguousarray(x[None])

def run_unet(session,x):
 y=np.asarray(session.infer(feeds=[x])[0])
 if y.shape==(1,512,512,2):
  return np.argmax(y[0],axis=-1).astype(np.uint8)
 if y.shape==(1,2,512,512):
  return np.argmax(y[0],axis=0).astype(np.uint8)
 raise RuntimeError('Unexpected OM U-Net output shape: '+str(y.shape))

def find_components(mask,min_area):
 n,labels,stats,centroids=cv2.connectedComponentsWithStats((mask>0).astype(np.uint8),connectivity=8)
 return [dict(label=int(k),area=int(stats[k,cv2.CC_STAT_AREA]),
              x=int(stats[k,cv2.CC_STAT_LEFT]),y=int(stats[k,cv2.CC_STAT_TOP]),
              w=int(stats[k,cv2.CC_STAT_WIDTH]),h=int(stats[k,cv2.CC_STAT_HEIGHT]),
              cx=float(centroids[k][0]),cy=float(centroids[k][1]))
         for k in range(1,n) if int(stats[k,cv2.CC_STAT_AREA])>=min_area]

def thinning_zhang_suen(binary):
 """Numpy implementation, used when cv2.ximgproc is missing on Atlas."""
 im=(binary>0).astype(np.uint8).copy()
 im[0,:]=0;im[-1,:]=0;im[:,0]=0;im[:,-1]=0
 if im.sum()==0:return im
 while True:
  changed=False
  for step in [1,2]:
   p2=im[:-2,1:-1];p3=im[:-2,2:];p4=im[1:-1,2:];p5=im[2:,2:]
   p6=im[2:,1:-1];p7=im[2:,:-2];p8=im[1:-1,:-2];p9=im[:-2,:-2]
   center=im[1:-1,1:-1]
   B=p2+p3+p4+p5+p6+p7+p8+p9
   A=sum(((x==0)&(y==1)).astype(np.uint8) for x,y in [
     (p2,p3),(p3,p4),(p4,p5),(p5,p6),(p6,p7),(p7,p8),(p8,p9),(p9,p2)])
   if step==1:
    m1=(p2*p4*p6)==0;m2=(p4*p6*p8)==0
   else:
    m1=(p2*p4*p8)==0;m2=(p2*p6*p8)==0
   marker=(center==1)&(B>=2)&(B<=6)&(A==1)&m1&m2
   if marker.any():center[marker]=0;changed=True
  if not changed:break
 return im

def skeletonize_mask(mask):
 img=(mask>0).astype(np.uint8)
 if img.sum()==0:return img
 if hasattr(cv2,'ximgproc') and hasattr(cv2.ximgproc,'thinning'):
  return (cv2.ximgproc.thinning(img*255)>0).astype(np.uint8)
 return thinning_zhang_suen(img)

def save_overlay(img,mask,path,color):
 out=img.copy()
 fg=(mask>0)
 if fg.any():out[fg]=np.uint8(np.clip(out[fg].astype(np.float32)*.65+np.array(color)*.35,0,255))
 if not cv2.imwrite(str(path),out):raise IOError(path)

def draw_growth_curve(rows,dest):
 if not rows:return
 canvas=np.full((500,900,3),255,np.uint8)
 l,r,t,b=80,40,50,80
 xx=np.asarray([q['hours_from_start'] for q in rows]); yy=np.asarray([q['seedling_skeleton_length_512'] for q in rows])
 x0=float(xx.min());xr=max(float(xx.max()-xx.min()),1e-6)
 ymax=max(float(yy.max()),1.0)
 def xy(x,y):return (int(l+(x-x0)/xr*(900-l-r)),int(500-b-y/ymax*(500-t-b)))
 cv2.line(canvas,(l,500-b),(900-r,500-b),(0,0,0),2)
 cv2.line(canvas,(l,t),(l,500-b),(0,0,0),2)
 points=[xy(float(x),float(y)) for x,y in zip(xx,yy)]
 for i in range(1,len(points)):cv2.line(canvas,points[i-1],points[i],(0,0,255),2)
 for point in points:cv2.circle(canvas,point,4,(0,0,255),-1)
 cv2.putText(canvas,'Seedling skeleton length over time',(115,30),cv2.FONT_HERSHEY_SIMPLEX,.75,(0,0,0),2)
 cv2.putText(canvas,'Hours from start',(350,470),cv2.FONT_HERSHEY_SIMPLEX,.7,(0,0,0),2)
 cv2.putText(canvas,f'Max length = {int(ymax)} px (512-grid)',(95,65),cv2.FONT_HERSHEY_SIMPLEX,.5,(70,70,70),1)
 cv2.imwrite(str(dest),canvas)

def calculate_vigor(rows):
 """Matches original May-2026 demonstration score formula, NOT a calibrated assay."""
 if not rows:return {}
 rows=sorted(rows,key=lambda r:r['hours_from_start']);last=rows[-1]
 counts=[r['seed_count'] for r in rows if r['seed_count']>0]
 n=int(round(float(np.median(counts)))) if counts else 0
 final_area=last['seedling_area_512'];seed_area=max(last['seed_area_512'],1)
 length=last['seedling_skeleton_length_512'];components=last['seedling_component_count']
 tmax=max(rows[-1]['hours_from_start']-rows[0]['hours_from_start'],1e-6)
 component_completion=min(100.,components/max(n,1)*100.) if n else 0.
 area_completion=min(100.,final_area/seed_area/.2*100.)
 completion=max(component_completion,area_completion)
 half=next((q['hours_from_start'] for q in rows if length>0 and q['seedling_skeleton_length_512']>=length/2),None)
 speed=np.clip((1-half/tmax)*100,0,100) if half is not None else 0.
 growth=min(100.,length/max(n*15.,1.)*100.) if n else 0.
 areas=last.get('seedling_component_areas',[])
 if len(areas)>=2:
  area_arr=np.asarray(areas,dtype=float);cv=float(area_arr.std()/max(area_arr.mean(),1e-6));uniformity=100/(1+cv)
 elif len(areas)==1:uniformity=50.
 else:uniformity=0.
 emergence=next((q['hours_from_start'] for q in rows if q['seedling_skeleton_length_512']>=5),None)
 score=.30*completion+.25*speed+.30*growth+.15*uniformity
 return dict(score_type='U-Net + CV baseline vigor score',seed_count_estimated=n,
  final_seedling_area_512=int(final_area),final_seedling_skeleton_length_512=int(length),
  final_seedling_component_count=int(components),
  time_to_seedling_emergence_h=emergence,time_to_half_growth_h=half,
  completion_score=round(float(completion),3),speed_score=round(float(speed),3),
  growth_score=round(float(growth),3),uniformity_score=round(float(uniformity),3),
  batch_vigor_score=round(float(score),3),
  note='Uncalibrated heuristic score for pipeline demonstration only. Connected components are not equivalent to germinated seeds; do not interpret this as official SeedGerm-VIG vigor or a validated 0-100 quality scale.')

def pipeline(input_dir,output_dir,model_dir=None,session_factory=None):
 """session_factory injectable to support hardware-free tests."""
 input_dir=Path(input_dir);output_dir=Path(output_dir)
 for sub in ['masks','overlays','summary']:(output_dir/sub).mkdir(parents=True,exist_ok=True)
 files=sorted([f for f in input_dir.iterdir() if f.suffix.lower() in ['.jpg','.jpeg','.png','.bmp']],
              key=lambda f:(datetime_from_name(f) or datetime.max,f.name))
 if not files:raise RuntimeError('No images in '+str(input_dir))
 times=[datetime_from_name(p) for p in files]
 real_time=all(t is not None for t in times)
 if not real_time:print('WARNING: Missing timestamps; using frame indices as pseudo-time, not hours.')
 if session_factory is None:
  from ais_bench.infer.interface import InferSession
  session_factory=lambda path:InferSession(device_id=0,model_path=str(path))
 model_dir=Path(model_dir or (ROOT/'models_om'))
 sessions={name:session_factory(model_dir/f'unet_rice_{name}_bs1_512.om') for name in COLORS}
 records=[]
 try:
  for i,img_file in enumerate(files):
   img=cv2.imread(str(img_file))
   if img is None:raise FileNotFoundError(img_file)
   h,w=img.shape[:2];x=preprocess_unet(img);masks={};comp=img.copy()
   for name,color in COLORS.items():
    mask512=run_unet(sessions[name],x)
    masks[name]=mask512
    full=cv2.resize(mask512,(w,h),interpolation=cv2.INTER_NEAREST)
    cv2.imwrite(str(output_dir/'masks'/f'{img_file.stem}_{name}_mask.png'),full*255)
    save_overlay(img,full,output_dir/'overlays'/f'{img_file.stem}_{name}_overlay.jpg',color)
    fg=full>0
    comp[fg]=np.uint8(np.clip(comp[fg].astype(np.float32)*.65+np.array(color)*.35,0,255))
   cv2.imwrite(str(output_dir/'overlays'/f'{img_file.stem}_composite_overlay.jpg'),comp)
   seed_comps=find_components(masks['seed'],20)
   seedling_comps=find_components(masks['seedling'],5)
   sk=skeletonize_mask(masks['seedling'])
   cv2.imwrite(str(output_dir/'masks'/f'{img_file.stem}_seedling_skeleton_512.png'),sk*255)
   hour=(times[i]-times[0]).total_seconds()/3600 if real_time else float(i)
   record=dict(image=img_file.name,time=times[i].isoformat() if real_time else '',
    hours_from_start=round(hour,4),seed_count=len(seed_comps),seed_area_512=int(masks['seed'].sum()),
    seedling_area_512=int(masks['seedling'].sum()),overall_area_512=int(masks['overall'].sum()),
    seedling_skeleton_length_512=int(sk.sum()),seedling_component_count=len(seedling_comps),
    seedling_component_areas=[c['area'] for c in seedling_comps])
   records.append(record)
   print(img_file.name,'seeds:',record['seed_count'],'seedling area:',record['seedling_area_512'],
         'skeleton:',record['seedling_skeleton_length_512'],flush=True)
 finally:
  sessions.clear()
 columns=['image','time','hours_from_start','seed_count','seed_area_512','seedling_area_512',
          'overall_area_512','seedling_skeleton_length_512','seedling_component_count']
 with (output_dir/'summary/image_trait_summary.csv').open('w',newline='',encoding='utf8') as f:
  w=csv.DictWriter(f,fieldnames=columns);w.writeheader();w.writerows([{k:r[k] for k in columns} for r in records])
 (output_dir/'summary/image_trait_summary_detail.json').write_text(json.dumps(records,indent=2),encoding='utf8')
 vigor=calculate_vigor(records)
 if not real_time:
  vigor['note']+=' No actual timestamps were available; time-based scores use frame indexes and should not be interpreted as hours.'
 (output_dir/'summary/vigor_score.json').write_text(json.dumps(vigor,indent=2,ensure_ascii=False),encoding='utf8')
 draw_growth_curve(records,output_dir/'summary/growth_curve_skeleton_length.png')
 print('DONE',output_dir,'vigor:',vigor.get('batch_vigor_score'))
 return records,vigor

def main():
 ap=argparse.ArgumentParser()
 ap.add_argument('--input_dir',default=str(ROOT/'input_images'))
 ap.add_argument('--output_dir',default=str(ROOT/'output_pipeline'))
 ap.add_argument('--model_dir',default=str(ROOT/'models_om'))
 args=ap.parse_args();pipeline(args.input_dir,args.output_dir,args.model_dir)

if __name__=='__main__':main()
