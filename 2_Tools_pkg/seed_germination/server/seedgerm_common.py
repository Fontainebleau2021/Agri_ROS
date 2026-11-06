"""Shared filesystem conventions / image helpers for reconstructed SeedGerm-VIG scripts."""
import os
from pathlib import Path
from datetime import datetime
import re
import cv2
import numpy as np

ROOT = Path(os.environ.get('SEEDGERM_ROOT', Path(__file__).resolve().parent)).expanduser().resolve()
MODELS = ROOT / '_models_rice_barley' / 'models_rice_barley'
IMAGE_DIR = ROOT / 'example_G7'


def parse_time(name):
    m = re.search(r'(\d{8})[_-](\d{4})', Path(name).stem)
    return datetime.strptime(''.join(m.groups()), '%Y%m%d%H%M') if m else None


def images_from_dir(directory):
    return sorted((p for p in Path(directory).iterdir() if p.suffix.lower() in {'.jpg','.jpeg','.png','.bmp'}), key=lambda p: (parse_time(p.name) or datetime.max, p.name))


def preprocess_unet(img_bgr, size=512):
    rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    rgb = cv2.resize(rgb, (size,size), interpolation=cv2.INTER_LINEAR)
    return (rgb.astype(np.float32) / 255.0)[None]


def letterbox(img, new_shape=(512,640), color=(114,114,114)):
    h,w = img.shape[:2]; nh,nw = new_shape
    r=min(nh/h,nw/w)
    rw,rh=int(round(w*r)),int(round(h*r))
    small=cv2.resize(img,(rw,rh),interpolation=cv2.INTER_LINEAR)
    dw,dh=nw-rw,nh-rh
    left,top=dw//2,dh//2
    out=cv2.copyMakeBorder(small,top,dh-top,left,dw-left,cv2.BORDER_CONSTANT,value=color)
    return out,r,left,top


def preprocess_yolo(img, shape=(512,640)):
    im,r,px,py=letterbox(img,shape)
    rgb=cv2.cvtColor(im,cv2.COLOR_BGR2RGB).astype(np.float32)/255
    return np.ascontiguousarray(rgb.transpose(2,0,1)[None]),r,px,py


def save_overlay(img, mask, out, color_bgr):
    out=Path(out); out.parent.mkdir(parents=True,exist_ok=True)
    dst=img.copy(); pos=mask.astype(bool)
    if pos.any():
        c=np.array(color_bgr, dtype=np.float32)
        dst[pos]=np.clip(dst[pos].astype(np.float32)*.65 + c*.35,0,255).astype(np.uint8)
    if not cv2.imwrite(str(out),dst):
        raise IOError(f'cv2.imwrite failed: {out}')
