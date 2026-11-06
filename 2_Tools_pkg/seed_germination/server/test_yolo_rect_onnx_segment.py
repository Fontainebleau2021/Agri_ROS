"""IMPORTANT: task='segment' (otherwise ONNX may be misparsed as detect)."""
import argparse,json
from pathlib import Path
from ultralytics import YOLO
from seedgerm_common import ROOT,IMAGE_DIR
p=argparse.ArgumentParser()
p.add_argument('--model',default=str(ROOT/'_deploy_export/YOLOv8x-Germ_rice_bs1_512x640.onnx'))
p.add_argument('--image',default=str(IMAGE_DIR/'G7_20221029_2018.jpg'))
p.add_argument('--output',default=str(ROOT/'_deploy_export/yolo_rect_onnx_segment_test'))
a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
model=YOLO(a.model,task='segment')
r=model.predict(a.image,imgsz=(512,640),conf=.25,device='cpu',save=True,
                project=str(out),name='predict',exist_ok=True)[0]
rows=[]
for b in r.boxes:
 rows.append(dict(cls_id=int(b.cls.item()),conf=float(b.conf.item()),xyxy=[float(x) for x in b.xyxy[0].tolist()]))
(out/'boxes.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
print('num boxes:',len(rows));print('first 10:',rows[:10])
