#!/usr/bin/env python3
"""多传感器自动标定工具入口：在 project 目录下运行 ``python main.py``。"""

from __future__ import annotations

import sys
from pathlib import Path

# 保证可直接运行脚本时找到 calibration_tool 包
_ROOT = Path(__file__).resolve().parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from calibration_tool.main_window import run  # noqa: E402

if __name__ == "__main__":
    run()
