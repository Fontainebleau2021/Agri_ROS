"""Heuristic seed tracking + tip estimation + exploratory vigor metrics.
NOT author's validated SeedGerm-VIG graph-based root-tip tracker.
In particular, farthest skeleton endpoint is only a root-tip proxy; not true root length.
"""
import argparse,math
from pathlib import Path
import numpy as np,pandas as pd,cv2
from scipy.optimize import linear_sum_assignment
from skimage.morphology import skeletonize
from seedgerm_common import ROOT,IMAGE_DIR,parse_time

def nearest_fixed_anchors(df,max_distance=900):
 df=df.copy();df['cx']=(df.x1+df.x2)/2;df['cy']=(df.y1+df.y2)/2
 names=sorted(df.image.unique(),key=lambda x:parse_time(x))
 first=df[df.image==names[0]].sort_values(['cy','cx']).reset_index(drop=True)
 # Fixed positions anchored to the first frame; stable for fixed-camera trays.
 xy=first[['cx','cy']].values.astype(float);rows=[]
 for image in names:
  group=df[df.image==image].reset_index(drop=True)
  cand=group[['cx','cy']].values.astype(float)
  if len(cand):
   dist=np.linalg.norm(xy[:,None,:]-cand[None,:,:],axis=2)
   rr,cc=linear_sum_assignment(dist)
   match={int(i):int(j) for i,j in zip(rr,cc) if dist[i,j]<max_distance}
  else:match={}
  for i in range(len(xy)):
   rec=group.iloc[match[i]].to_dict() if i in match else dict(image=image)
   rec.update(seed_id=f'S{i+1:02d}',anchor_cx=float(xy[i,0]),anchor_cy=float(xy[i,1]),
              tracking_status='matched' if i in match else 'missing',
              match_distance=float(np.linalg.norm(xy[i]-cand[match[i]])) if i in match else np.nan)
   rows.append(rec)
 tracks=pd.DataFrame(rows); t0=parse_time(names[0]);tracks['hours_from_start']=tracks.image.map(lambda x:(parse_time(x)-t0).total_seconds()/3600)
 return tracks.sort_values(['seed_id','hours_from_start'])

def tip_estimation(tracks, masks_dir, images_dir, output):
 rows=[];output=Path(output);ov=output/'tracking_overlays';ov.mkdir(parents=True,exist_ok=True)
 for image,frame in tracks.groupby('image'):
  im=cv2.imread(str(Path(images_dir)/image));mask=cv2.imread(str(Path(masks_dir)/Path(image).stem/'seedling_mask.png'),0)
  if im is None or mask is None:raise FileNotFoundError(image)
  binary=(mask>0).astype(np.uint8);H,W=binary.shape;canvas=im.copy()
  for _,r in frame.iterrows():
   rec=dict(image=image,seed_id=r.seed_id,root_tip_status='no_seedling',root_tip_x=np.nan,root_tip_y=np.nan,
            root_length_from_seed_center=0.,seedling_area_crop=0,seedling_skeleton_length_crop=0)
   if r.tracking_status!='matched':rec['root_tip_status']='missing_detection';rows.append(rec);continue
   x1,y1,x2,y2=[float(r[k]) for k in ['x1','y1','x2','y2']]
   dx=(x2-x1)*1.5;dy=(y2-y1)*1.5
   xa=max(0,int(x1-dx));ya=max(0,int(y1-dy));xb=min(W,int(x2+dx));yb=min(H,int(y2+dy))
   crop=binary[ya:yb,xa:xb]
   if crop.size:
    n,lab,stats,_=cv2.connectedComponentsWithStats(crop,8)
    clean=np.zeros_like(crop)
    for j in range(1,n):
     if stats[j,cv2.CC_STAT_AREA]>=10:clean[lab==j]=1
    rec['seedling_area_crop']=int(clean.sum())
    if clean.any():
     sk=skeletonize(clean.astype(bool)).astype(np.uint8)
     rec['seedling_skeleton_length_crop']=int(sk.sum())
     neighbors=cv2.filter2D(sk,-1,np.ones((3,3),np.uint8))
     ys,xs=np.where((sk==1)&(neighbors==2))
     if len(xs)==0:ys,xs=np.where(sk==1)
     if len(xs):
      cx=(x1+x2)/2-xa;cy=(y1+y2)/2-ya
      dist=np.hypot(xs-cx,ys-cy);index=int(dist.argmax())
      tipx=float(xs[index]+xa);tipy=float(ys[index]+ya)
      rec.update(root_tip_status='proxy_detected',root_tip_x=tipx,root_tip_y=tipy,
                 root_length_from_seed_center=float(dist[index]))
      cv2.circle(canvas,(int(tipx),int(tipy)),8,(0,0,255),-1)
   rows.append(rec)
  cv2.imwrite(str(ov/(Path(image).stem+'_root_tip_overlay.jpg')),canvas)
 return pd.DataFrame(rows)

def first_phase(g,k):
 phase=pd.to_numeric(g.phase_class,errors='coerce')
 ok=g.loc[phase>=k,'hours_from_start']
 return float(ok.min()) if len(ok) else np.nan

def rank_scores(v):
 v=v.astype(float);finite=np.isfinite(v)
 out=np.zeros(len(v))
 if finite.sum() and v[finite].max()>v[finite].min():out[finite]=(v[finite]-v[finite].min())/(v[finite].max()-v[finite].min())*100
 elif finite.sum():out[finite]=100
 return out

def create_vigor(tracks,tips):
 merged=tracks.merge(tips,on=['image','seed_id'],how='left')
 records=[]
 for seed_id,g in merged.groupby('seed_id'):
  g=g.sort_values('hours_from_start')
  t3=first_phase(g,3);t4=first_phase(g,4)
  rlen=pd.to_numeric(g.root_length_from_seed_center,errors='coerce').fillna(0)
  phase=pd.to_numeric(g.phase_class,errors='coerce')
  records.append(dict(seed_id=seed_id,time_to_phase3_h=t3,time_to_phase4_h=t4,
   final_phase=float(phase.max()) if phase.notna().any() else 0,
   final_root_length_from_seed_center=float(rlen.max()),
   final_seedling_skeleton_length=float(pd.to_numeric(g.seedling_skeleton_length_crop,errors='coerce').fillna(0).max())))
 v=pd.DataFrame(records)
 v['completion_score']=np.clip(v.final_phase/4*100,0,100)
 v['speed_score']=100-rank_scores(v.time_to_phase3_h)
 v.loc[v.time_to_phase3_h.isna(),'speed_score']=0
 v['growth_score']=rank_scores(v.final_root_length_from_seed_center)
 v['seed_vigor_score_baseline']=.4*v.completion_score+.3*v.speed_score+.3*v.growth_score
 times=v.time_to_phase3_h.dropna()
 std=float(times.std()) if len(times)>1 else np.nan
 factor=1/(1+std/10) if np.isfinite(std) else np.nan
 batch=dict(num_seeds=len(v),phase3_fraction=float(v.time_to_phase3_h.notna().mean()),
  phase4_fraction=float(v.time_to_phase4_h.notna().mean()),
  mean_time_to_phase3_h=float(times.mean()) if len(times) else np.nan,
  std_time_to_phase3_h=std,mean_seed_score=float(v.seed_vigor_score_baseline.mean()),
  uniformity_factor_phase3=factor,
  batch_vigor_score_baseline=float(v.seed_vigor_score_baseline.mean()*(factor if np.isfinite(factor) else 1)))
 return merged,v,pd.DataFrame([batch])

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--traits',default=str(ROOT/'_run_traits_baseline/per_seed_traits.csv'))
 p.add_argument('--masks',default=str(ROOT/'_run_unet_rice_batch'))
 p.add_argument('--images',default=str(IMAGE_DIR))
 p.add_argument('--output',default=str(ROOT/'_run_advanced_postprocess'))
 a=p.parse_args();o=Path(a.output);o.mkdir(parents=True,exist_ok=True)
 tracks=nearest_fixed_anchors(pd.read_csv(a.traits));tracks.to_csv(o/'seed_tracks.csv',index=False)
 tips=tip_estimation(tracks,a.masks,a.images,o);tips.to_csv(o/'root_tip_tracks.csv',index=False)
 merged,v,b=create_vigor(tracks,tips);v.to_csv(o/'per_seed_vigor_matrix.csv',index=False);b.to_csv(o/'batch_vigor_summary.csv',index=False)
 # Restore the three diagnostic figures from the historical workflow.
 try:
  import matplotlib
  matplotlib.use('Agg')
  import matplotlib.pyplot as plt
  matched=tracks[tracks.tracking_status=='matched'].copy()
  matched['phase_class']=matched.phase_class.astype(str)
  counts=pd.crosstab(matched.hours_from_start,matched.phase_class)
  plt.figure(figsize=(7,4))
  for phase in sorted(counts.columns):plt.plot(counts.index,counts[phase],marker='o',label='phase '+str(phase))
  plt.xlabel('Hours from start');plt.ylabel('Tracked seed count');plt.legend();plt.tight_layout()
  plt.savefig(o/'tracked_phase_counts.png',dpi=180);plt.close()
  plt.figure(figsize=(8,5))
  for seed_id,g in merged.groupby('seed_id'):
   g=g.sort_values('hours_from_start');plt.plot(g.hours_from_start,g.root_length_from_seed_center,alpha=.45)
  plt.xlabel('Hours from start');plt.ylabel('Tip displacement proxy (px)');plt.tight_layout()
  plt.savefig(o/'root_growth_by_seed.png',dpi=180);plt.close()
  plt.figure(figsize=(8,4));plt.bar(v.seed_id,v.seed_vigor_score_baseline)
  plt.xticks(rotation=90);plt.ylabel('Uncalibrated baseline score');plt.tight_layout()
  plt.savefig(o/'seed_vigor_scores.png',dpi=180);plt.close()
 except ImportError as e:
  print('Plotting skipped: matplotlib unavailable',e)
 print('tracks:',len(tracks),'tips:',len(tips));print(b.to_string(index=False))
 print('NOTE: Experimental heuristic score, not a calibrated vigor measurement.')

if __name__=='__main__':main()
