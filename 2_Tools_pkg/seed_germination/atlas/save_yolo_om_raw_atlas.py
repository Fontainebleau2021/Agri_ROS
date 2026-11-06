"""Diagnostic only: YOLO OM was NOT accepted for final Atlas production in this project."""
import argparse
from pathlib import Path
import cv2,numpy as np
from ais_bench.infer.interface import InferSession
from run_seedgerm_unet_cv_pipeline import ROOT

def preprocess(img):
 h,w=img.shape[:2];nh,nw=512,640;r=min(nh/h,nw/w)
 sw,sh=int(round(w*r)),int(round(h*r))
 image=cv2.resize(img,(sw,sh));dw=nw-sw;dh=nh-sh;px=dw//2;py=dh//2
 image=cv2.copyMakeBorder(image,py,dh-py,px,dw-px,cv2.BORDER_CONSTANT,value=(114,114,114))
 x=cv2.cvtColor(image,cv2.COLOR_BGR2RGB).astype(np.float32)/255
 return np.ascontiguousarray(x.transpose(2,0,1)[None]),r,px,py

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--model',default=str(ROOT/'models_om/yolov8x_germ_rice_bs1_512x640.om'))
 p.add_argument('--image',default=str(ROOT/'input_images/G7_20221029_2018.jpg'))
 p.add_argument('--output',default=str(ROOT/'atlas_yolo_rect_raw.npz'))
 a=p.parse_args();im=cv2.imread(a.image)
 if im is None:raise FileNotFoundError(a.image)
 x,r,px,py=preprocess(im)
 sess=InferSession(device_id=0,model_path=a.model);o=sess.infer(feeds=[x]);del sess
 np.savez_compressed(a.output,input=x,output0=np.asarray(o[0]),output1=np.asarray(o[1]),ratio=np.float32(r),pad_x=np.float32(px),pad_y=np.float32(py))
 print('saved',a.output,'output0:',np.asarray(o[0]).shape,'output1:',np.asarray(o[1]).shape)

if __name__=='__main__':main()
