# camera/realsense_camera.py
# Intel RealSense 真机实现。
# 当 config.USE_MOCK = False 时由工厂函数自动选用。
# 依赖：pip install pyrealsense2

import numpy as np
from typing import Tuple, Optional

from .base_camera import BaseCamera
from core.logger import get_logger

logger = get_logger(__name__)


class RealSenseCamera(BaseCamera):
    """Intel RealSense D4xx 系列相机"""

    def __init__(
        self,
        width: int  = 640,
        height: int = 480,
        fps: int    = 30,
        align_to_color: bool = True,
    ):
        """
        Args:
            width, height, fps : 流分辨率与帧率
            align_to_color     : True → 深度图对齐到彩色图坐标系（推荐）
        """
        super().__init__()
        self.width          = width
        self.height         = height
        self.fps            = fps
        self.align_to_color = align_to_color

        self._pipeline = None
        self._align    = None

        # 延迟导入，避免没安装库时整个模块报错
        try:
            import pyrealsense2 as rs
            self._rs = rs
        except ImportError as e:
            raise ImportError(
                "未找到 pyrealsense2。\n"
                "  安装命令: pip install pyrealsense2\n"
                "  若设备尚未到位，请在 config.py 中保持 USE_MOCK = True"
            ) from e

    # ── 生命周期 ─────────────────────────────────────────────────
    def start(self) -> bool:
        rs = self._rs
        try:
            self._pipeline = rs.pipeline()
            cfg = rs.config()
            cfg.enable_stream(rs.stream.color, self.width, self.height,
                              rs.format.bgr8, self.fps)
            cfg.enable_stream(rs.stream.depth, self.width, self.height,
                              rs.format.z16,  self.fps)

            profile = self._pipeline.start(cfg)

            if self.align_to_color:
                self._align = rs.align(rs.stream.color)

            # 打印设备信息
            dev  = profile.get_device()
            name = dev.get_info(rs.camera_info.name)
            sn   = dev.get_info(rs.camera_info.serial_number)
            fw   = dev.get_info(rs.camera_info.firmware_version)
            logger.info(f"RealSenseCamera 已连接：{name}  SN={sn}  FW={fw}")
            logger.info(f"分辨率 {self.width}×{self.height}@{self.fps}fps  "
                        f"深度对齐={'开' if self.align_to_color else '关'}")

            self._is_running = True
            return True

        except Exception as exc:
            logger.error(f"RealSenseCamera 启动失败：{exc}")
            return False

    def stop(self):
        if self._pipeline and self._is_running:
            try:
                self._pipeline.stop()
            except Exception:
                pass
            self._is_running = False
            logger.info("RealSenseCamera 已停止")

    # ── 获取帧 ────────────────────────────────────────────────────
    def get_frame(self) -> Tuple[Optional[np.ndarray], Optional[np.ndarray]]:
        if not self._is_running or self._pipeline is None:
            return None, None
        try:
            frames = self._pipeline.wait_for_frames(timeout_ms=5000)

            if self._align:
                frames = self._align.process(frames)

            color_f = frames.get_color_frame()
            depth_f = frames.get_depth_frame()

            if not color_f:
                return None, None

            color_img = np.asanyarray(color_f.get_data())          # HxWx3 uint8
            depth_img = np.asanyarray(depth_f.get_data()) if depth_f else None  # HxW uint16

            return color_img, depth_img

        except Exception as exc:
            logger.warning(f"RealSenseCamera 读帧失败：{exc}")
            return None, None

    # ── 额外工具 ──────────────────────────────────────────────────
    def get_depth_scale(self) -> float:
        """返回深度比例系数（pixel value × scale = 距离/米）"""
        if not self._is_running or self._pipeline is None:
            return 0.001
        try:
            profile = self._pipeline.get_active_profile()
            sensor  = profile.get_device().first_depth_sensor()
            return sensor.get_depth_scale()
        except Exception:
            return 0.001
