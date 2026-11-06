#!/usr/bin/env bash
# CAUTION: historical YOLOv8x-Germ OM parsed but output differed dramatically from ONNX.
# This script documents the failed deployment experiment; do not use this OM as validated.
set -euo pipefail
cd "${SEEDGERM_ATLAS_ROOT:-/root/seedgerm_atlas}"
source /usr/local/Ascend/ascend-toolkit/set_env.sh
mkdir -p models_om logs_atc
atc --model='models_onnx/YOLOv8x-Germ_rice_bs1_512x640.onnx' \
    --framework=5 --output='models_om/yolov8x_germ_rice_bs1_512x640' \
    --input_format=NCHW --input_shape='images:1,3,512,640' \
    --soc_version="${SOC_VERSION:-Ascend310B1}" --log=info 2>&1 | tee logs_atc/atc_yolo.log
