"""Normalize names, static shapes and duplicate *unused* opset imports for old ATC.
Fails safely if the graph depends on nondefault operator domains.
"""
import argparse
from pathlib import Path
import onnx
from seedgerm_common import ROOT

def rename_io(model,old,new):
 for vi in list(model.graph.input)+list(model.graph.output)+list(model.graph.value_info):
  if vi.name==old:vi.name=new
 for node in model.graph.node:
  for i,v in enumerate(node.input):
   if v==old:node.input[i]=new
  for i,v in enumerate(node.output):
   if v==old:node.output[i]=new

def fix_shape(vi,shape):
 dims=vi.type.tensor_type.shape.dim
 if len(dims)!=len(shape):raise ValueError('Unexpected rank: '+vi.name)
 for d,v in zip(dims,shape):
  d.ClearField('dim_param');d.dim_value=int(v)

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--input-dir',default=str(ROOT/'_deploy_export'))
 a=p.parse_args();folder=Path(a.input_dir)
 for name in ['overall','seed','seedling']:
  src=folder/f'unet_rice_{name}_bs1_512.onnx'
  dst=folder/f'unet_rice_{name}_bs1_512_atc.onnx'
  m=onnx.load(str(src))
  print(src.name,'opsets:',[(x.domain,x.version) for x in m.opset_import])
  used_domains={node.domain for node in m.graph.node if node.domain not in ('','ai.onnx')}
  if used_domains:raise RuntimeError(f'Cannot delete custom operator domain(s): {used_domains}')
  defaults=[int(x.version) for x in m.opset_import if x.domain in ('','ai.onnx')]
  if not defaults:raise RuntimeError('No default ONNX opset')
  if len(set(defaults))!=1:raise RuntimeError(f'Conflicting default opsets: {defaults}')
  del m.opset_import[:]
  imp=m.opset_import.add();imp.domain='';imp.version=defaults[0]
  if len(m.graph.input)!=1 or len(m.graph.output)!=1:raise RuntimeError('Expected one input/output')
  rename_io(m,m.graph.input[0].name,'input_1')
  rename_io(m,m.graph.output[0].name,'output')
  fix_shape(m.graph.input[0],[1,512,512,3])
  fix_shape(m.graph.output[0],[1,512,512,2])
  onnx.checker.check_model(m)
  onnx.save(m,str(dst));print('saved',dst)

if __name__=='__main__':main()
