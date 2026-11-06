"""Save expanded YOLO ROIs; one image crop per YOLO detection."""
import argparse
from pathlib import Path
import cv2,pandas as pd
from seedgerm_common import ROOT,IMAGE_DIR

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--csv',default=str(ROOT/'_run_yolo_rice_batch_cpu/batch_boxes.csv'))
    ap.add_argument('--input-dir',default=str(IMAGE_DIR))
    ap.add_argument('--output',default=str(ROOT/'_run_yolo_rice_rois'))
    ap.add_argument('--padding-ratio',type=float,default=.2)
    a=ap.parse_args();o=Path(a.output);o.mkdir(parents=True,exist_ok=True)
    df=pd.read_csv(a.csv);rows=[]
    for name,g in df.groupby('image',sort=False):
        img=cv2.imread(str(Path(a.input_dir)/name))
        if img is None: raise FileNotFoundError(name)
        H,W=img.shape[:2]
        for _,r in g.iterrows():
            x1,y1,x2,y2=(float(r[k]) for k in ['x1','y1','x2','y2'])
            dx=(x2-x1)*a.padding_ratio;dy=(y2-y1)*a.padding_ratio
            xx1=max(0,int(round(x1-dx)));xx2=min(W,int(round(x2+dx)))
            yy1=max(0,int(round(y1-dy)));yy2=min(H,int(round(y2+dy)))
            if xx2<=xx1 or yy2<=yy1:continue
            filename=f'{Path(name).stem}_box{int(r.box_id):03d}_cls{r.class_name}_conf{r.confidence:.3f}.png'
            cv2.imwrite(str(o/filename),img[yy1:yy2,xx1:xx2])
            d=r.to_dict();d.update(roi=filename,crop_x1=xx1,crop_y1=yy1,crop_x2=xx2,crop_y2=yy2,crop_w=xx2-xx1,crop_h=yy2-yy1)
            rows.append(d)
    pd.DataFrame(rows).to_csv(o/'rois.csv',index=False)
    print('ROI数量:',len(rows),'output:',o)

if __name__=='__main__':main()
