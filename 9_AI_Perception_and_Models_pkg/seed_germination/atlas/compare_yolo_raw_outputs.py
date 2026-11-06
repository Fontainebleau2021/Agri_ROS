"""Use saved server_yolo_rect_raw.npz and atlas_yolo_rect_raw.npz on Atlas."""
import argparse,numpy as np
p=argparse.ArgumentParser();p.add_argument('--server',default='server_yolo_rect_raw.npz');p.add_argument('--atlas',default='atlas_yolo_rect_raw.npz');a=p.parse_args()
s=np.load(a.server);t=np.load(a.atlas)
for key in ['input','output0','output1']:
 x=np.asarray(s[key],dtype=np.float32);y=np.asarray(t[key],dtype=np.float32)
 print('\n',key,'server',x.shape,'atlas',y.shape)
 if x.shape!=y.shape:continue
 d=np.abs(x-y)
 print('abs diff mean',float(d.mean()),'max',float(d.max()),'p95',float(np.percentile(d,95)),'p99',float(np.percentile(d,99)))
for name,obj in [('server',s),('atlas',t)]:
 pred=obj['output0'][0]
 if pred.shape[0]!=40:raise RuntimeError(pred.shape)
 score=pred[4:8].max(axis=0)
 print(name,'top20',np.sort(score)[-20:][::-1].tolist(),
       'count>=0.25',int((score>=.25).sum()),'count>=0.005',int((score>=.005).sum()))
