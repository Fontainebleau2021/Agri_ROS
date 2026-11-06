import cv2
import os
import time
import numpy as np

# 捕获pyk4a缺失异常，程序正常运行
try:
    import pyk4a
    HAS_K4A = True
except ModuleNotFoundError:
    print("【提示】未安装pyk4a，程序仅启用模拟采集模式")
    HAS_K4A = False


class CaptureBase:
    def get_frame(self):
        pass

    def close(self):
        pass


# 仅存在pyk4a时才加载硬件采集类
if HAS_K4A:
    class KinectCapture(CaptureBase):
        def __init__(self):
            self.device = None
            config = pyk4a.Config()
            config.color_resolution = pyk4a.ColorResolution.RES_1080P
            try:
                self.device = pyk4a.PyK4a()
                self.device.start(config)
            except Exception as e:
                raise Exception(f"Kinect设备启动失败：{e}")

        def get_frame(self):
            capture = self.device.get_capture()
            rgb_img = capture.color[:, :, :3]
            return rgb_img

        def close(self):
            if self.device is not None:
                self.device.stop()


# 模拟相机采集（无硬件时使用）
class SimCapture(CaptureBase):
    def __init__(self, test_img_dir="./sim_img"):
        self.img_list = []
        self.idx = 0
        if not os.path.exists(test_img_dir):
            os.makedirs(test_img_dir)
        file_list = [f for f in os.listdir(test_img_dir) if f.endswith((".jpg", ".png", ".jpeg"))]
        # 文件夹无图片则自动生成测试图
        if len(file_list) == 0:
            for i in range(3):
                blank_img = np.ones((720, 1280, 3), dtype=np.uint8) * 240
                cv2.imwrite(os.path.join(test_img_dir, f"demo_{i}.jpg"), blank_img)
            file_list = os.listdir(test_img_dir)
        self.img_list = [os.path.join(test_img_dir, f) for f in file_list]

    def get_frame(self):
        path = self.img_list[self.idx % len(self.img_list)]
        frame = cv2.imread(path)
        self.idx += 1
        return frame

    def close(self):
        pass


# 统一采集入口
def get_capture():
    if HAS_K4A:
        try:
            return KinectCapture()
        except Exception:
            print("【提示】未检测到Azure Kinect硬件，切换模拟采集")
            return SimCapture()
    else:
        return SimCapture()


# 按任务、场景分层保存图像
def save_rgb_image(frame, task_name, scene_name):
    root = "./dataset"
    save_dir = os.path.join(root, task_name, scene_name)
    os.makedirs(save_dir, exist_ok=True)
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"{timestamp}.jpg"
    full_path = os.path.join(save_dir, filename)
    cv2.imwrite(full_path, frame)
    return full_path