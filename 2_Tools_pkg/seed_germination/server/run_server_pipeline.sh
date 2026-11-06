#!/usr/bin/env bash
# Existing conda envs must be installed; this script uses them without reinstalling torch.
set -euo pipefail
PROJECT="${SEEDGERM_ROOT:-/lustre/home/acct-stu/stu702/SeedGerm-VIG}"
cd "$PROJECT"
# Set YOLO_DEVICE=0 on an available NVIDIA GPU node, or leave cpu.
YOLO_DEVICE="${YOLO_DEVICE:-cpu}"
conda run --no-capture-output -n seedgerm_detect python batch_yolo_rice_cpu.py --device "$YOLO_DEVICE"
conda run --no-capture-output -n seedgerm_detect python crop_yolo_rois.py
CUDA_VISIBLE_DEVICES="${CUDA_VISIBLE_DEVICES:-}" conda run --no-capture-output -n seedgerm_segment_tf22 python batch_unet_rice_full.py
conda run --no-capture-output -n seedgerm_segment_tf22 python extract_traits_from_masks.py
conda run --no-capture-output -n seedgerm_segment_tf22 python plot_traits_baseline.py
conda run --no-capture-output -n seedgerm_segment_tf22 python advanced_seedgerm_postprocess.py
