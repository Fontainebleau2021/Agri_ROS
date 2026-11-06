"""TF 2.2 SavedModel -> frozen graph -> ONNX, avoiding tf2onnx _DictWrapper bug."""
import argparse
from pathlib import Path
import tensorflow as tf
import tf2onnx
from tensorflow.python.framework.convert_to_constants import convert_variables_to_constants_v2
from seedgerm_common import ROOT,MODELS

def main():
 p=argparse.ArgumentParser();p.add_argument('--output-dir',default=str(ROOT/'_deploy_export'));a=p.parse_args()
 out=Path(a.output_dir);out.mkdir(parents=True,exist_ok=True)
 print('TensorFlow',tf.__version__,'tf2onnx',tf2onnx.__version__)
 for name in ['overall','seed','seedling']:
  folder=MODELS/f'U-net_rice_{name}_detection'
  print('loading:',folder)
  m=tf.saved_model.load(str(folder));sig=m.signatures['serving_default']
  frozen=convert_variables_to_constants_v2(sig)
  graph=frozen.graph.as_graph_def(add_shapes=True)
  input_names=[frozen.inputs[0].name];output_names=[z.name for z in frozen.outputs]
  print(name,'inputs:',input_names,'outputs:',output_names)
  dest=out/f'unet_rice_{name}_bs1_512.onnx'
  tf2onnx.convert.from_graph_def(graph,input_names=input_names,
    output_names=output_names,opset=11,output_path=str(dest))
  print('saved:',dest)

if __name__=='__main__':main()
