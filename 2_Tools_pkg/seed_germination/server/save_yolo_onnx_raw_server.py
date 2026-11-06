"""Save identical test input and raw ONNX outputs for comparison with Atlas OM."""
import argparse
from pathlib import Path
import cv2,numpy as np,onnxruntime as ort
from seedgerm_common import ROOT,IMAGE_DIR,preprocess_yolo
p=argparse.ArgumentParser()
p.add_argument('--model',default=str(ROOT/'_deploy_export/YOLOv8x-Germ_rice_bs1_512x640.onnx'))
p.add_argument('--image',default=str(IMAGE_DIR/'G7_20221029_2018.jpg'))
p.add_argument('--output',default=str(ROOT/'_deploy_export/server_yolo_rect_raw.npz'))
a=p.parse_args();img=cv2.imread(a.image)
if img is None:raise FileNotFoundError(a.image)
x,r,px,py=preprocess_yolo(img,(512,640));session=ort.InferenceSession(a.model,providers=['CPUExecutionProvider'])
y=session.run(None,{session.get_inputs()[0].name:x})
np.savez_compressed(a.output,input=x,output0=y[0],output1=y[1],ratio=np.float32(r),pad_x=np.float32(px),pad_y=np.float32(py))
print('saved',a.output,'input',x.shape,'out0',y[0].shape,'out1',y[1].shape)
