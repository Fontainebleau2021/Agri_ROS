#!/usr/bin/env bash
set -euo pipefail
cd "${SEEDGERM_ATLAS_ROOT:-/root/seedgerm_atlas}"
source /usr/local/Ascend/ascend-toolkit/set_env.sh
mkdir -p models_om logs_atc
SOC_VERSION="${SOC_VERSION:-Ascend310B1}"
for name in overall seed seedling; do
 echo "Converting rice ${name} on ${SOC_VERSION}..."
 atc --model="models_onnx/unet_rice_${name}_bs1_512_atc.onnx" --framework=5 \
     --output="models_om/unet_rice_${name}_bs1_512" \
     --input_format=ND --input_shape='input_1:1,512,512,3' \
     --soc_version="${SOC_VERSION}" --log=info 2>&1 | tee "logs_atc/atc_unet_${name}.log"
done
