"""YOLO inference on time-series; output schema compatible with trait scripts."""
import argparse,json
from pathlib import Path
import pandas as pd
from ultralytics import YOLO
from seedgerm_common import ROOT,MODELS,IMAGE_DIR,images_from_dir

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--model',default=str(MODELS/'YOLOv8x-Germ_rice.pt'))
    p.add_argument('--input-dir',default=str(IMAGE_DIR))
    p.add_argument('--output',default=str(ROOT/'_run_yolo_rice_batch_cpu'))
    p.add_argument('--device',default='cpu',help='cpu or CUDA GPU index, e.g. 0')
    p.add_argument('--imgsz',type=int,default=640)
    p.add_argument('--conf',type=float,default=.25)
    a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
    imgs=images_from_dir(a.input_dir)
    if not imgs: raise SystemExit('No images')
    model=YOLO(a.model)
    allrows=[]
    for image in imgs:
        r=model.predict(str(image),device=a.device,imgsz=a.imgsz,conf=a.conf,
                        save=True,project=str(out),name='predict',exist_ok=True,verbose=False)[0]
        for idx,b in enumerate(r.boxes):
            cls=int(b.cls.item());x1,y1,x2,y2=[float(x) for x in b.xyxy[0].tolist()]
            allrows.append(dict(image=image.name,box_id=idx,class_id=cls,class_name=str(r.names[cls]),
                confidence=float(b.conf.item()),x1=x1,y1=y1,x2=x2,y2=y2))
        print(image.name,'boxes',len(r.boxes))
    cols=['image','box_id','class_id','class_name','confidence','x1','y1','x2','y2']
    pd.DataFrame(allrows,columns=cols).to_csv(out/'batch_boxes.csv',index=False)
    (out/'batch_boxes.json').write_text(json.dumps(allrows,indent=2),encoding='utf8')
    print('TOTAL',len(allrows),'CSV:',out/'batch_boxes.csv')

if __name__=='__main__':main()
