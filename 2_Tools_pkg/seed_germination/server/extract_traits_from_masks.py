"""Reconstructed YOLO-box + U-Net full-resolution mask trait extraction baseline.
Important: boxes are not temporal seed IDs; ROI sums can double-count overlapping boxes.
"""
import argparse,math
from pathlib import Path
import cv2,numpy as np,pandas as pd
from seedgerm_common import ROOT,IMAGE_DIR,parse_time
try:
 from skimage.morphology import skeletonize
except ImportError:
 skeletonize=None

def component_filter(binary,min_area=10):
 n,labels,stats,_=cv2.connectedComponentsWithStats((binary>0).astype(np.uint8),8)
 keep=[i for i in range(1,n) if int(stats[i,cv2.CC_STAT_AREA])>=min_area]
 out=np.isin(labels,keep).astype(np.uint8)
 return out,keep,stats

def seed_traits(binary):
 clean,ids,stats=component_filter(binary,20)
 if not ids:return dict(seed_area=0,seed_perimeter=0.,seed_length=0.,seed_width=0.,seed_roundness=0.,seed_component_count=0)
 lab=max(ids,key=lambda i:stats[i,cv2.CC_STAT_AREA])
 n,labels,_,_=cv2.connectedComponentsWithStats(clean,8)
 # IDs from a relabelled mask might differ, so select largest actual component.
 sizes=np.bincount(labels.reshape(-1));sizes[0]=0
 one=(labels==sizes.argmax()).astype(np.uint8)
 contours,_=cv2.findContours(one,cv2.RETR_EXTERNAL,cv2.CHAIN_APPROX_SIMPLE)
 c=max(contours,key=cv2.contourArea)
 per=float(cv2.arcLength(c,True));rw,rh=cv2.minAreaRect(c)[1]
 area=int(one.sum());roundness=float(4*math.pi*area/per**2) if per>0 else 0.
 return dict(seed_area=area,seed_perimeter=per,seed_length=float(max(rw,rh)),seed_width=float(min(rw,rh)),seed_roundness=roundness,seed_component_count=len(ids))

def seedling_traits(binary):
 clean,ids,_=component_filter(binary,10)
 length=int(np.count_nonzero(skeletonize(clean.astype(bool)))) if skeletonize is not None and clean.any() else 0
 return dict(seedling_area=int(clean.sum()),seedling_skeleton_length=length,seedling_component_count=len(ids))

def main():
 ap=argparse.ArgumentParser()
 ap.add_argument('--boxes',default=str(ROOT/'_run_yolo_rice_batch_cpu/batch_boxes.csv'))
 ap.add_argument('--masks',default=str(ROOT/'_run_unet_rice_batch'))
 ap.add_argument('--images',default=str(IMAGE_DIR))
 ap.add_argument('--output',default=str(ROOT/'_run_traits_baseline'))
 ap.add_argument('--padding',type=float,default=.2)
 a=ap.parse_args();o=Path(a.output);o.mkdir(parents=True,exist_ok=True)
 if skeletonize is None:print('WARNING skimage unavailable; skeleton lengths set to zero')
 b=pd.read_csv(a.boxes);rows=[]
 for image,g in b.groupby('image'):
  src=cv2.imread(str(Path(a.images)/image));
  if src is None:raise FileNotFoundError(image)
  h,w=src.shape[:2];folder=Path(a.masks)/Path(image).stem
  masks={}
  for name in ('overall','seed','seedling'):
   m=cv2.imread(str(folder/f'{name}_mask.png'),cv2.IMREAD_GRAYSCALE)
   if m is None:raise FileNotFoundError(folder/f'{name}_mask.png')
   masks[name]=(m>0).astype(np.uint8)
   if masks[name].shape!=(h,w):raise ValueError('masks and images differ in shape')
  overall=np.maximum.reduce([masks['overall'],masks['seed'],masks['seedling']])
  for _,r in g.iterrows():
   x1,y1,x2,y2=[float(r[k]) for k in ['x1','y1','x2','y2']]
   dx=(x2-x1)*a.padding;dy=(y2-y1)*a.padding
   xx1=max(0,int(round(x1-dx)));yy1=max(0,int(round(y1-dy)))
   xx2=min(w,int(round(x2+dx)));yy2=min(h,int(round(y2+dy)))
   if xx2<=xx1 or yy2<=yy1:continue
   dt=parse_time(image)
   rec=dict(image=image,time=dt.isoformat() if dt else '',box_id=int(r.box_id),
    phase_class=str(r.class_name),yolo_confidence=float(r.confidence),
    x1=x1,y1=y1,x2=x2,y2=y2,crop_x1=xx1,crop_y1=yy1,crop_x2=xx2,crop_y2=yy2,
    crop_w=xx2-xx1,crop_h=yy2-yy1)
   rec.update(seed_traits(masks['seed'][yy1:yy2,xx1:xx2]))
   rec.update(seedling_traits(masks['seedling'][yy1:yy2,xx1:xx2]))
   rec['overall_area_in_roi']=int(overall[yy1:yy2,xx1:xx2].sum())
   rows.append(rec)
 per=pd.DataFrame(rows)
 if per.empty:raise RuntimeError('No ROIs')
 per.to_csv(o/'per_seed_traits.csv',index=False)
 summary=[]
 for image,g in per.groupby('image'):
  dt=parse_time(image);c=g['phase_class'].value_counts()
  summary.append(dict(image=image,time=dt.isoformat() if dt else '',detected_boxes=len(g),
    **{f'phase_{i}_count':int(c.get(str(i),0)) for i in range(1,5)},
    **{f'{col}_{agg}':float(getattr(g[col],agg)()) for col in ['seed_area','seedling_area','seedling_skeleton_length','overall_area_in_roi'] for agg in ['sum','mean']}))
 s=pd.DataFrame(summary).sort_values('time').reset_index(drop=True)
 if s['time'].ne('').all():
  times=pd.to_datetime(s.time);s['hours_from_start']=(times-times.iloc[0]).dt.total_seconds()/3600
  s['delta_hours']=s.hours_from_start.diff()
  for col in ['seed_area_sum','seedling_area_sum','seedling_skeleton_length_sum','overall_area_in_roi_sum']:
   s[f'{col}_rate_per_hour']=s[col].diff()/s.delta_hours
 else: s['hours_from_start']=np.arange(len(s),dtype=float)
 s.to_csv(o/'image_trait_summary.csv',index=False)
 s.to_csv(o/'image_trait_summary_rates.csv',index=False)
 print('per_seed rows:',len(per),'images:',len(s),'->',o)
 print(s[['image','detected_boxes','seedling_area_sum','seedling_skeleton_length_sum']].to_string(index=False))

if __name__=='__main__':main()
