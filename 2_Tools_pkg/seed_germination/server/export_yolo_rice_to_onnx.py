"""Export published rice YOLOv8x-Germ segmentation model, CPU or CUDA.
Historical working rectangular export: 512x640, opset=11, static batch=1.
"""
import argparse,shutil
from pathlib import Path
from ultralytics import YOLO
from seedgerm_common import ROOT,MODELS

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--model',default=str(MODELS/'YOLOv8x-Germ_rice.pt'))
 p.add_argument('--output-dir',default=str(ROOT/'_deploy_export'))
 p.add_argument('--height',type=int,default=512)
 p.add_argument('--width',type=int,default=640)
 p.add_argument('--device',default='cpu')
 a=p.parse_args();out=Path(a.output_dir);out.mkdir(parents=True,exist_ok=True)
 m=YOLO(a.model)
 print('model task:',m.task,'names:',m.names)
 src=Path(m.export(format='onnx',imgsz=(a.height,a.width),opset=11,
                   simplify=False,dynamic=False,half=False,device=a.device))
 dest=out/f'YOLOv8x-Germ_rice_bs1_{a.height}x{a.width}.onnx'
 if src.resolve()!=dest.resolve():shutil.copy2(src,dest)
 print('ONNX:',dest)

if __name__=='__main__':main()
