# camera/mock_camera.py
# 模拟相机：在没有 RealSense 设备时完全替代真机，用于开发与测试。
# 支持两种数据源：
#   1. 指定本地图片文件夹 → 循环播放其中的图片
#   2. 不指定            → 实时生成带动态色块的测试帧

import os
import time
import numpy as np
import cv2
from typing import Tuple, Optional

from .base_camera import BaseCamera
from core.logger import get_logger

logger = get_logger(__name__)


class MockCamera(BaseCamera):
    """模拟相机"""

    def __init__(
        self,
        image_folder: str = "",
        width: int = 640,
        height: int = 480,
        fps: int = 30,
    ):
        super().__init__()
        self.width        = width
        self.height       = height
        self.fps          = fps
        self.image_folder = image_folder

        self._images: list       = []
        self._img_index: int     = 0
        self._frame_count: int   = 0
        self._last_time: float   = 0.0

        # 用于生成动态场景的随机种子目标（固定位置，只色彩变化）
        rng = np.random.default_rng(seed=2024)
        self._boxes = [
            {
                "x": int(rng.integers(40, width  - 160)),
                "y": int(rng.integers(40, height - 120)),
                "w": int(rng.integers(80, 180)),
                "h": int(rng.integers(60, 140)),
            }
            for _ in range(4)
        ]

    # ── 生命周期 ─────────────────────────────────────────────────
    def start(self) -> bool:
        if self.image_folder:
            self._load_from_folder(self.image_folder)

        self._is_running  = True
        self._last_time   = time.time()
        self._frame_count = 0

        src = f"文件夹 {self.image_folder}（{len(self._images)} 张）" if self._images else "动态生成"
        logger.info(f"MockCamera 启动  {self.width}×{self.height}@{self.fps}fps  数据源：{src}")
        return True

    def stop(self):
        self._is_running = False
        logger.info("MockCamera 已停止")

    # ── 获取帧 ────────────────────────────────────────────────────
    def get_frame(self) -> Tuple[Optional[np.ndarray], Optional[np.ndarray]]:
        if not self._is_running:
            return None, None

        # ⚠️ 注意：不在这里 sleep 做帧率控制。
        # 调用方（capture_widget 的 QTimer）已经按 1000/fps ms 间隔触发，
        # 在 Qt 主线程里 sleep 会阻塞整个事件循环，导致 UI 冻结。
        # _last_time / _frame_count 仅用于动画渐变计算，不作限速用途。
        self._last_time = time.time()

        color = self._next_color_frame()
        depth = self._make_depth_frame()
        self._frame_count += 1
        return color, depth

    # ── 内部实现 ──────────────────────────────────────────────────
    def _load_from_folder(self, folder: str):
        exts = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".webp"}
        for name in sorted(os.listdir(folder)):
            if os.path.splitext(name)[1].lower() in exts:
                path = os.path.join(folder, name)
                img  = cv2.imread(path)
                if img is not None:
                    self._images.append(cv2.resize(img, (self.width, self.height)))

    def _next_color_frame(self) -> np.ndarray:
        if self._images:
            frame = self._images[self._img_index].copy()
            self._img_index = (self._img_index + 1) % len(self._images)
        else:
            frame = self._generate_dynamic_frame()
        self._draw_overlay(frame)
        return frame

    def _generate_dynamic_frame(self) -> np.ndarray:
        """生成渐变背景 + 彩色矩形的动态测试帧"""
        # 渐变背景
        t     = self._frame_count / self.fps
        frame = np.zeros((self.height, self.width, 3), dtype=np.uint8)
        for row in range(self.height):
            hue        = int((row / self.height * 90 + t * 15) % 180)
            frame[row] = [hue, 160, 200]
        frame = cv2.cvtColor(frame, cv2.COLOR_HSV2BGR)

        # 彩色色块（模拟可标注目标）
        for i, b in enumerate(self._boxes):
            hue   = int((i * 45 + t * 30) % 180)
            color = cv2.cvtColor(np.uint8([[[hue, 220, 220]]]), cv2.COLOR_HSV2BGR)[0][0]
            color = (int(color[0]), int(color[1]), int(color[2]))
            cv2.rectangle(frame, (b["x"], b["y"]),
                          (b["x"] + b["w"], b["y"] + b["h"]), color, -1)
            cv2.rectangle(frame, (b["x"], b["y"]),
                          (b["x"] + b["w"], b["y"] + b["h"]), (255, 255, 255), 2)
        return frame

    def _make_depth_frame(self) -> np.ndarray:
        """生成模拟深度图：背景渐变 + 色块区域赋予物体深度"""
        depth = np.zeros((self.height, self.width), dtype=np.uint16)
        # 基础背景（模拟地面到天花板）
        for row in range(self.height):
            depth[row] = int(500 + 3500 * row / self.height)
        
        # 色块区域赋予不同深度（模拟前景物体）
        t = self._frame_count / self.fps
        for i, b in enumerate(self._boxes):
            obj_depth = 800 + i * 400 + int(150 * np.sin(t * 2 + i))
            y1, y2 = max(0, b["y"]), min(self.height, b["y"] + b["h"])
            x1, x2 = max(0, b["x"]), min(self.width, b["x"] + b["w"])
            depth[y1:y2, x1:x2] = obj_depth
        
        # 物体边缘加轻微噪声模拟RealSense特性
        noise = np.random.randint(-20, 20, depth.shape, dtype=np.int32)
        depth = np.clip(depth.astype(np.int32) + noise, 0, 65535).astype(np.uint16)
        return depth

    def _draw_overlay(self, frame: np.ndarray):
        """在帧左上角叠加时间戳和帧计数"""
        ts  = time.strftime("%Y-%m-%d %H:%M:%S")
        txt = f"[MOCK]  {ts}  Frame:{self._frame_count}"
        cv2.putText(frame, txt, (8, 24),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 0, 0),   3, cv2.LINE_AA)
        cv2.putText(frame, txt, (8, 24),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 80), 1, cv2.LINE_AA)