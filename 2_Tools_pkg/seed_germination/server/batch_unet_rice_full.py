"""Batch TensorFlow U-Net inference producing exact historical folder/file names."""
import argparse
from pathlib import Path
import tensorflow as tf
import cv2,numpy as np,pandas as pd
from seedgerm_common import ROOT,MODELS,IMAGE_DIR,images_from_dir,preprocess_unet,save_overlay
COLORS={'overall':(255,0,0),'seed':(0,255,0),'seedling':(0,0,255)}

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--input-dir',default=str(IMAGE_DIR))
 p.add_argument('--output',default=str(ROOT/'_run_unet_rice_batch'))
 a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
 print('TensorFlow:',tf.__version__)
 # Crucial TF 2.2 fix: keep loaded models referenced during the entire batch.
 models={k:tf.saved_model.load(str(MODELS/f'U-net_rice_{k}_detection')) for k in COLORS}
 signatures={k:m.signatures['serving_default'] for k,m in models.items()}
 files=images_from_dir(a.input_dir);print('num images:',len(files));rows=[]
 for image in files:
  print('Processing:',image.name)
  img=cv2.imread(str(image))
  if img is None:raise FileNotFoundError(image)
  h,w=img.shape[:2];x=tf.constant(preprocess_unet(img),dtype=tf.float32)
  dest=out/image.stem;dest.mkdir(parents=True,exist_ok=True)
  masks={};comp=img.copy()
  for name,color in COLORS.items():
   y=signatures[name](input_1=x)
   arr=next(iter(y.values())).numpy()[0]
   mask512=np.argmax(arr,axis=-1).astype(np.uint8)
   mask=cv2.resize(mask512,(w,h),interpolation=cv2.INTER_NEAREST)
   masks[name]=mask
   cv2.imwrite(str(dest/f'{name}_mask.png'),mask*255)
   save_overlay(img,mask,dest/f'{name}_overlay.jpg',color)
   fg=mask.astype(bool)
   comp[fg]=np.uint8(np.clip(comp[fg].astype(np.float32)*.65+np.array(color)*.35,0,255))
   rows.append(dict(image=image.name,model=name,area_pixels=int(mask.sum()),area_ratio=float(mask.mean())))
  cv2.imwrite(str(dest/'composite_overlay.jpg'),comp)
 pd.DataFrame(rows).to_csv(out/'summary.csv',index=False)
 print('done; records:',len(rows),'output:',out)

if __name__=='__main__':main()
