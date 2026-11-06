"""Run without Ascend hardware, Torch, TensorFlow or model weights.
Uses deterministic mock InferSession producing seed/seedling masks.
"""
import tempfile,sys
from pathlib import Path
import numpy as np,cv2
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'atlas'))
from run_seedgerm_unet_cv_pipeline import pipeline,calculate_vigor,thinning_zhang_suen,find_components

class FakeSession:
 def __init__(self,name):self.name=Path(name).name
 def infer(self,feeds):
  assert feeds[0].shape==(1,512,512,3)
  x=np.zeros((1,512,512,2),np.float32);x[...,0]=1
  if 'seedling' in self.name:
   x[0,240:310,300:307,0]=0;x[0,240:310,300:307,1]=1
  elif 'seed_' in self.name or 'overall' in self.name:
   x[0,210:260,285:330,0]=0;x[0,210:260,285:330,1]=1
  return [x]

with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);(d/'input').mkdir()
 for i,name in enumerate(['G7_20221026_1637.jpg','G7_20221026_2138.jpg']):
  arr=np.zeros((160,240,3),np.uint8);arr[:,:,1]=i*30
  cv2.imwrite(str(d/'input'/name),arr)
 rows,score=pipeline(d/'input',d/'output',model_dir=d/'models',session_factory=FakeSession)
 assert len(rows)==2
 assert rows[0]['seed_count']>=1
 assert rows[0]['seedling_area_512']>0
 assert score['score_type']=='U-Net + CV baseline vigor score'
 assert 0<=score['batch_vigor_score']<=100
 for f in ['summary/vigor_score.json','summary/image_trait_summary.csv','summary/growth_curve_skeleton_length.png','overlays/G7_20221026_1637_composite_overlay.jpg']:
  assert (d/'output'/f).exists(),f
 assert thinning_zhang_suen(np.zeros((512,512),np.uint8)).sum()==0
 print('OFFLINE ATLAS PIPELINE SMOKE TEST PASSED')
