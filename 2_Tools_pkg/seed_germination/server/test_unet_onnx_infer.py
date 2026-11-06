"""ONNX Runtime smoke test (does not require TensorFlow)."""
import argparse
from pathlib import Path
import numpy as np,cv2,onnxruntime as ort
from seedgerm_common import ROOT,IMAGE_DIR,preprocess_unet,save_overlay
p=argparse.ArgumentParser();p.add_argument('--image',default=str(IMAGE_DIR/'G7_20221029_2018.jpg'))
p.add_argument('--output',default=str(ROOT/'_deploy_export/unet_onnx_test'));a=p.parse_args()
img=cv2.imread(a.image)
if img is None:raise FileNotFoundError(a.image)
h,w=img.shape[:2];x=preprocess_unet(img)
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
for name,color in [('overall',(255,0,0)),('seed',(0,255,0)),('seedling',(0,0,255))]:
 f=ROOT/f'_deploy_export/unet_rice_{name}_bs1_512.onnx'
 sess=ort.InferenceSession(str(f),providers=['CPUExecutionProvider'])
 y=sess.run(None,{sess.get_inputs()[0].name:x})[0]
 if y.ndim!=4 or y.shape[-1]!=2:raise ValueError((name,y.shape))
 mask=cv2.resize(np.argmax(y[0],axis=-1).astype(np.uint8),(w,h),interpolation=cv2.INTER_NEAREST)
 cv2.imwrite(str(out/f'{name}_mask_onnx.png'),mask*255)
 save_overlay(img,mask,out/f'{name}_overlay_onnx.jpg',color)
 print(name,'shape:',y.shape,'foreground:',int(mask.sum()))
