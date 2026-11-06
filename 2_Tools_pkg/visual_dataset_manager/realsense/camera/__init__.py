# camera/__init__.py
# 根据 config.USE_MOCK 自动选择相机实现，
# 上层代码只需: from camera import create_camera

from .base_camera import BaseCamera


def create_camera() -> BaseCamera:
    """
    工厂函数：按照 config.py 的设置返回正确的相机实例。
    只需改 config.USE_MOCK 即可切换真机 / 模拟，其余代码无需修改。
    """
    import config as cfg

    if cfg.USE_MOCK:
        from .mock_camera import MockCamera
        return MockCamera(
            image_folder=cfg.MOCK_IMAGE_FOLDER,
            width=cfg.CAMERA_WIDTH,
            height=cfg.CAMERA_HEIGHT,
            fps=cfg.CAMERA_FPS,
        )
    else:
        from .realsense_camera import RealSenseCamera
        return RealSenseCamera(
            width=cfg.CAMERA_WIDTH,
            height=cfg.CAMERA_HEIGHT,
            fps=cfg.CAMERA_FPS,
        )
