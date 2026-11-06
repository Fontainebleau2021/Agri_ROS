#!/usr/bin/env bash
# Run only on a node with NVIDIA GPU + matching CUDA-enabled PyTorch.
set -euo pipefail
cd "${SEEDGERM_ROOT:-/lustre/home/acct-stu/stu702/SeedGerm-VIG}"
python - <<'PY'
import torch
print('PyTorch:',torch.__version__,'CUDA:',torch.cuda.is_available())
if not torch.cuda.is_available():
 raise SystemExit('No NVIDIA CUDA device detected. Run this on a GPU node or use --device cpu instead.')
print('GPU:',torch.cuda.get_device_name(0))
PY
python batch_yolo_rice_cpu.py --device 0 --imgsz 640 --conf .25
