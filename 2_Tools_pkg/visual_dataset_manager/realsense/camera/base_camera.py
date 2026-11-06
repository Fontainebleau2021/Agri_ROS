# camera/base_camera.py
# 相机抽象基类：所有相机实现（真机 / 模拟）都继承此类，
# 保证上层代码无需关心底层是哪种相机。

from abc import ABC, abstractmethod
from typing import Tuple, Optional
import numpy as np


class BaseCamera(ABC):
    """相机抽象基类，定义统一接口"""

    def __init__(self):
        self._is_running = False

    # ── 必须由子类实现的接口 ────────────────────────────────
    @abstractmethod
    def start(self) -> bool:
        """
        启动相机。
        Returns:
            True  — 成功
            False — 失败（设备未找到、权限不足等）
        """

    @abstractmethod
    def stop(self):
        """停止相机，释放资源"""

    @abstractmethod
    def get_frame(self) -> Tuple[Optional[np.ndarray], Optional[np.ndarray]]:
        """
        读取一帧。
        Returns:
            (color_frame, depth_frame)
            color_frame : HxWx3 uint8，BGR 格式（与 OpenCV 一致）
            depth_frame : HxW   uint16，单位 mm；如设备不支持深度则为 None
            任意一项读取失败时对应位置返回 None
        """

    # ── 公共属性 ────────────────────────────────────────────
    @property
    def is_running(self) -> bool:
        return self._is_running

    # ── 上下文管理器支持（with 语句）──────────────────────────
    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
