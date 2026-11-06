"""Single-image rice YOLOv8x-Germ (segmentation-model) detector. CPU or CUDA."""
import argparse,json
from pathlib import Path
from ultralytics import YOLO
from seedgerm_common import ROOT,MODELS,IMAGE_DIR

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--model',default=str(MODELS/'YOLOv8x-Germ_rice.pt'))
    p.add_argument('--image',default=str(IMAGE_DIR/'G7_20221026_1637.jpg'))
    p.add_argument('--device',default='cpu',help='cpu, 0, 1, ...')
    p.add_argument('--imgsz',type=int,default=640)
    p.add_argument('--conf',type=float,default=.25)
    p.add_argument('--output',default=str(ROOT/'_run_yolo_rice_cpu'))
    a=p.parse_args()
    out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
    model=YOLO(a.model)
    print('model task:',model.task,'classes:',model.names)
    res=model.predict(source=a.image,device=a.device,imgsz=a.imgsz,conf=a.conf,
                      save=True,project=str(out),name='predict_one',exist_ok=True,verbose=True)
    boxes=[]
    for r in res:
        for b in r.boxes:
            cls=int(b.cls.item())
            boxes.append(dict(image=Path(r.path).name,class_id=cls,class_name=str(r.names[cls]),
                              confidence=float(b.conf.item()),xyxy=[float(z) for z in b.xyxy[0].tolist()]))
    (out/'predict_one_boxes.json').write_text(json.dumps(boxes,indent=2),encoding='utf8')
    print('num boxes:',len(boxes),'saved to',out)

if __name__=='__main__':main()
