import tensorflow as tf
from seedgerm_common import MODELS
for name in ['overall','seed','seedling']:
 p=MODELS/f'U-net_rice_{name}_detection'
 print('\nMODEL:',p)
 m=tf.saved_model.load(str(p));print('signatures:',list(m.signatures))
 f=m.signatures['serving_default']
 print('inputs:',f.structured_input_signature)
 print('outputs:',f.structured_outputs)
