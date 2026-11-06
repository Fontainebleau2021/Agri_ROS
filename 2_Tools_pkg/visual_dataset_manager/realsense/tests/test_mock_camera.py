# tests/test_mock_camera.py
# 测试 MockCamera（完全不依赖硬件）
# 运行：python -m pytest tests/ -v

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np
import pytest
from camera.mock_camera import MockCamera


@pytest.fixture
def cam():
    c = MockCamera(width=640, height=480, fps=60)   # 高帧率让测试快一点
    c.start()
    yield c
    c.stop()


class TestMockCameraLifecycle:
    def test_start_returns_true(self):
        c = MockCamera()
        assert c.start() is True
        c.stop()

    def test_is_running_after_start(self):
        c = MockCamera()
        c.start()
        assert c.is_running is True
        c.stop()

    def test_is_not_running_after_stop(self):
        c = MockCamera()
        c.start()
        c.stop()
        assert c.is_running is False

    def test_context_manager(self):
        with MockCamera() as c:
            assert c.is_running is True
        assert c.is_running is False


class TestMockCameraFrames:
    def test_get_frame_returns_tuple(self, cam):
        color, depth = cam.get_frame()
        assert color is not None
        assert depth is not None

    def test_color_frame_shape(self, cam):
        color, _ = cam.get_frame()
        assert color.shape == (480, 640, 3)

    def test_color_frame_dtype(self, cam):
        color, _ = cam.get_frame()
        assert color.dtype == np.uint8

    def test_depth_frame_shape(self, cam):
        _, depth = cam.get_frame()
        assert depth.shape == (480, 640)

    def test_depth_frame_dtype(self, cam):
        _, depth = cam.get_frame()
        assert depth.dtype == np.uint16

    def test_depth_values_in_range(self, cam):
        _, depth = cam.get_frame()
        assert depth.min() >= 0
        assert depth.max() <= 65535

    def test_get_frame_when_stopped(self):
        c = MockCamera()
        c.start()
        c.stop()
        color, depth = c.get_frame()
        assert color is None
        assert depth is None

    def test_frames_are_different(self, cam):
        """连续两帧不应完全相同（动态场景）"""
        f1, _ = cam.get_frame()
        f2, _ = cam.get_frame()
        # 帧计数器变化，overlay 文字不同，所以帧必然不同
        assert not np.array_equal(f1, f2)

    def test_multiple_frames_no_crash(self, cam):
        """连续读取10帧不报错"""
        for _ in range(10):
            color, depth = cam.get_frame()
            assert color is not None


class TestMockCameraFromFolder:
    def test_load_from_folder(self, tmp_path):
        """从文件夹加载图片：能正常读取并循环播放"""
        import cv2
        # 写两张测试图
        for i in range(2):
            img = np.full((480, 640, 3), i * 100, dtype=np.uint8)
            cv2.imwrite(str(tmp_path / f"img_{i:03d}.jpg"), img)

        cam = MockCamera(image_folder=str(tmp_path), fps=60)
        cam.start()
        assert len(cam._images) == 2
        # 读 4 帧（循环播放）
        for _ in range(4):
            color, _ = cam.get_frame()
            assert color is not None
        cam.stop()
