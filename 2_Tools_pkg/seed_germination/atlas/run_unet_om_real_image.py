"""Single-image 3×U-Net OM inference, same filenames as early Atlas tests."""
import argparse,json,os
from pathlib import Path
import cv2,numpy as np
from run_seedgerm_unet_cv_pipeline import ROOT,COLORS,preprocess_unet,run_unet,save_overlay

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--image',default=str(ROOT/'input_images/G7_20221029_2018.jpg'))
 p.add_argument('--output',default=str(ROOT/'output_unet_real'))
 a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
 from ais_bench.infer.interface import InferSession
 img=cv2.imread(a.image)
 if img is None:raise FileNotFoundError(a.image)
 h,w=img.shape[:2];x=preprocess_unet(img);comp=img.copy();summary={}
 for name,color in COLORS.items():
  session=InferSession(device_id=0,model_path=str(ROOT/f'models_om/unet_rice_{name}_bs1_512.om'))
  mask=run_unet(session,x)
  del session  # old ais_bench has no free_resource() method
  full=cv2.resize(mask,(w,h),interpolation=cv2.INTER_NEAREST)
  cv2.imwrite(str(out/f'{name}_mask.png'),full*255)
  save_overlay(img,full,out/f'{name}_overlay.jpg',color)
  fg=full>0;comp[fg]=np.uint8(np.clip(comp[fg].astype(np.float32)*.65+np.array(color)*.35,0,255))
  summary[name]=dict(area_pixels=int(full.sum()),area_ratio=float(full.mean()))
  print(name,summary[name])
 cv2.imwrite(str(out/'composite_overlay.jpg'),comp)
 (out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
 print('saved to',out)

if __name__=='__main__':main()
