"""Three rice U-Net SavedModel segmenters, single original-size image."""
import argparse,json
from pathlib import Path
import tensorflow as tf
import cv2,numpy as np
from seedgerm_common import ROOT,MODELS,IMAGE_DIR,preprocess_unet,save_overlay

COLORS={'overall':(255,0,0),'seed':(0,255,0),'seedling':(0,0,255)}
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--image',default=str(IMAGE_DIR/'G7_20221028_1851.jpg'))
    p.add_argument('--output',default=str(ROOT/'_run_unet_rice_one'))
    a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
    img=cv2.imread(a.image)
    if img is None:raise FileNotFoundError(a.image)
    h,w=img.shape[:2];x=preprocess_unet(img);summary={};masks={}
    # Keep model objects alive while signature runs (TF 2.2 resource-variables issue).
    for name,color in COLORS.items():
        model=tf.saved_model.load(str(MODELS/f'U-net_rice_{name}_detection'))
        sig=model.signatures['serving_default']
        y=sig(input_1=tf.constant(x))
        arr=np.asarray(next(iter(y.values())).numpy())
        if arr.shape[-1]!=2:raise RuntimeError((name,arr.shape))
        mask=cv2.resize(np.argmax(arr[0],axis=-1).astype(np.uint8),(w,h),interpolation=cv2.INTER_NEAREST)
        masks[name]=mask
        cv2.imwrite(str(out/f'{name}_mask.png'),mask*255)
        cv2.imwrite(str(out/f'{name}_prob.png'),np.uint8(np.clip(arr[0,:,:,1],0,1)*255))
        save_overlay(img,mask,out/f'{name}_overlay.jpg',color)
        summary[name]={'area_pixels':int(mask.sum()),'area_ratio':float(mask.mean())}
        print(name,summary[name]);del sig,model
    composite=img.copy()
    for name,color in COLORS.items():
        foreground=masks[name].astype(bool)
        composite[foreground]=np.uint8(np.clip(composite[foreground].astype(np.float32)*.65+np.array(color)*.35,0,255))
    cv2.imwrite(str(out/'composite_overlay.jpg'),composite)
    (out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')

if __name__=='__main__':main()
