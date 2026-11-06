from pathlib import Path
import sys
import os
import json
import cv2
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QLabel, QPushButton, QLineEdit, QComboBox, QListWidget,
                             QListWidgetItem, QMessageBox)
from PyQt5.QtCore import Qt, QTimer, QRect
from PyQt5.QtGui import QPixmap, QImage, QPainter, QPen, QIcon
from PyQt5.QtWidgets import QApplication
# 全局统一字体大小
def set_global_font():
    font = QApplication.font()
    font.setPointSize(11)
    QApplication.setFont(font)

# ===================== 自定义绘图画布类 =====================
class DrawPreviewLabel(QLabel):
    def __init__(self, parent_win):
        super().__init__()
        self.parent_win = parent_win
        self.setMinimumSize(800, 600)
        self.setStyleSheet("border:1px solid #888;")
        self.setAlignment(Qt.AlignCenter)
        # 绘图变量
        self.draw_flag = False
        self.draw_start = (0, 0)
        self.draw_end = (0, 0)
        self.min_drag = 10
        # 原图与缩放缓存
        self.origin_img = None
        self.scale_x = 1.0
        self.scale_y = 1.0

    def set_origin_image(self, cv_frame):
        # 保存原始cv图片，计算缩放比例
        self.origin_img = cv_frame.copy()
        h_img, w_img = self.origin_img.shape[:2]
        label_w = self.width()
        label_h = self.height()
        # 计算等比例缩放
        scale_w = label_w / w_img
        scale_h = label_h / h_img
        self.scale_x = min(scale_w, scale_h)
        self.scale_y = min(scale_w, scale_h)
        # 生成缩放后pixmap填充label
        rgb = cv2.cvtColor(self.origin_img, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        bytes_per_line = ch * w
        qimg = QImage(rgb.data, w, h, bytes_per_line, QImage.Format_RGB888)
        pix = QPixmap.fromImage(qimg)
        self.setPixmap(pix.scaled(self.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation))

    def paintEvent(self, event):
        super().paintEvent(event)
        painter = QPainter(self)
        pen = QPen(Qt.red, 2)
        painter.setPen(pen)
        # 绘制保存的标注（原图坐标转缩放画布坐标，转int）
        for box in self.parent_win.box_list:
            sx1 = int(box["xmin"] * self.scale_x)
            sy1 = int(box["ymin"] * self.scale_y)
            sx2 = int(box["xmax"] * self.scale_x)
            sy2 = int(box["ymax"] * self.scale_y)
            rect = QRect(sx1, sy1, sx2 - sx1, sy2 - sy1)
            painter.drawRect(rect)
            painter.drawText(sx1, sy1 - 5, box["class"])
        # 拖拽临时框
        if self.draw_flag:
            x1, y1 = self.draw_start
            x2, y2 = self.draw_end
            temp_rect = QRect(x1, y1, x2 - x1, y2 - y1)
            painter.drawRect(temp_rect)

    def mousePressEvent(self, event):
        if not self.parent_win.is_annotate_mode:
            return
        if event.button() == Qt.MouseButton.LeftButton:
            self.draw_flag = True
            self.draw_start = (event.x(), event.y())
            self.draw_end = (event.x(), event.y())
            self.update()

    def mouseMoveEvent(self, event):
        if not self.parent_win.is_annotate_mode or not self.draw_flag:
            return
        self.draw_end = (event.x(), event.y())
        self.update()

    def mouseReleaseEvent(self, event):
        if not self.parent_win.is_annotate_mode or not self.draw_flag:
            return
        if event.button() == Qt.MouseButton.LeftButton:
            self.draw_flag = False
            x1, y1 = self.draw_start
            x2, y2 = self.draw_end
            drag_w = abs(x2 - x1)
            drag_h = abs(y2 - y1)
            if drag_w < self.min_drag or drag_h < self.min_drag:
                self.update()
                return
            # 画布缩放坐标还原为原图像素坐标保存标注
            x_min = min(x1, x2) / self.scale_x
            y_min = min(y1, y2) / self.scale_y
            x_max = max(x1, x2) / self.scale_x
            y_max = max(y1, y2) / self.scale_y
            cls_name = self.parent_win.combo_class.currentText()
            self.parent_win.box_list.append({
                "class": cls_name,
                "xmin": x_min,
                "ymin": y_min,
                "xmax": x_max,
                "ymax": y_max
            })
            self.parent_win.save_label_json()
            msg = QMessageBox(self.parent_win)
            msg.setIcon(QMessageBox.Information)
            msg.setWindowTitle("标注完成")
            msg.setText(f"已添加类别：{cls_name}")
            msg.setStyleSheet("QPushButton{min-width:110px;min-height:38px;font-size:12px;}")
            msg.exec()
            self.update()
# 模拟采集类
class SimCapture:
    def get_frame(self):
        return None
def get_capture():
    return SimCapture()

# ===================== 主窗口 =====================
class MainToolWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowIcon(QIcon(str(Path(__file__).resolve().parent / "assets" / "app_icon.png")))
        self.setWindowTitle("Azure Kinect 图像采集标注工具")
        self.resize(1200, 750)
        self.setup_ui()

        # 采集轮播原有变量
        self.capture = get_capture()
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_preview_frame)
        self.timer.start(500)
        self.current_frame = None
        self.is_annotate_mode = False
        self.annotate_img = None
        self.img_list = []
        self.cur_img_idx = 0

        # 绘图控制变量 
        self.draw_flag = False
        self.draw_start = (0, 0)
        self.draw_end = (0, 0)
        self.min_drag = 10  # 拖动小于10像素判定单击，不生成标注
        self.box_list = []
        self.canvas_pix = None

        # 基础路径变量
        self.current_img_path = ""
        self.statusBar = self.statusBar()

    def setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)

        # 左侧控制面板
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_panel.setFixedWidth(260)

        # 采集设置
        left_layout.addWidget(QLabel("===== 采集设置 ====="))
        left_layout.addWidget(QLabel("任务名称"))
        self.edit_task = QLineEdit("test_task")
        left_layout.addWidget(self.edit_task)
        left_layout.addWidget(QLabel("场景名称"))
        self.edit_scene = QLineEdit("scene_01")
        left_layout.addWidget(self.edit_scene)

        self.btn_save_img = QPushButton("保存当前画面")
        self.btn_save_img.clicked.connect(self.on_save_image)
        left_layout.addWidget(self.btn_save_img)

        # 新增返回实时预览按钮
        self.btn_back_preview = QPushButton("返回实时预览")
        self.btn_back_preview.clicked.connect(self.back_to_preview)
        left_layout.addWidget(self.btn_back_preview)

        # 标注设置
        left_layout.addWidget(QLabel("===== 标注设置 ====="))
        left_layout.addWidget(QLabel("标注类别"))
        self.combo_class = QComboBox()
        self.combo_class.addItems(["person", "box", "cup"])
        left_layout.addWidget(self.combo_class)

        self.btn_clear_all = QPushButton("清空整张图标注")
        self.btn_clear_all.clicked.connect(self.on_clear_all_label)
        left_layout.addWidget(self.btn_clear_all)

        # 导出区域
        left_layout.addWidget(QLabel("===== 数据集导出 ====="))
        self.btn_export_yolo = QPushButton("导出YOLO标注")
        self.btn_export_voc = QPushButton("导出VOC标注")
        left_layout.addWidget(self.btn_export_yolo)
        left_layout.addWidget(self.btn_export_voc)
        left_layout.addStretch()

        # 中间绘图画布（替换原生QLabel，使用自定义DrawPreviewLabel）
        self.label_preview = DrawPreviewLabel(self)

        # 右侧图片列表
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_panel.setFixedWidth(240)
        right_layout.addWidget(QLabel("当前场景图片列表"))
        self.list_img = QListWidget()
        self.list_img.itemDoubleClicked.connect(self.load_selected_image)
        right_layout.addWidget(self.list_img)
        self.btn_refresh = QPushButton("刷新图片列表")
        self.btn_refresh.clicked.connect(self.refresh_img_list)
        right_layout.addWidget(self.btn_refresh)

        # 整体布局组装
        main_layout.addWidget(left_panel)
        main_layout.addWidget(self.label_preview)
        main_layout.addWidget(right_panel)

    # ===================== 轮播/采集原有函数 =====================
    def back_to_preview(self):
        self.is_annotate_mode = False
        self.annotate_img = None
        self.box_list.clear()
        self.label_preview.update()

    def update_preview_frame(self):
        if self.is_annotate_mode and self.annotate_img is not None:
            self.show_frame_on_label(self.annotate_img)
            return
        sim_dir = "./sim_img"
        if os.path.exists(sim_dir):
            self.img_list = [f for f in os.listdir(sim_dir) if f.endswith((".jpg",".jpeg",".png"))]
        if len(self.img_list) == 0:
            self.current_frame = self.capture.get_frame()
        else:
            img_path = os.path.join(sim_dir, self.img_list[self.cur_img_idx])
            self.current_frame = cv2.imread(img_path)
            self.cur_img_idx = (self.cur_img_idx + 1) % len(self.img_list)
        self.show_frame_on_label(self.current_frame)

    def show_frame_on_label(self, frame):
        if frame is None or frame.size == 0:
            return
    # 调用画布自适应缩放填充完整图片
        self.label_preview.set_origin_image(frame)
    def on_save_image(self):
        task = self.edit_task.text().strip()
        scene = self.edit_scene.text().strip()
        save_dir = os.path.join("./dataset", task, scene)
        os.makedirs(save_dir, exist_ok=True)
        import time
        save_name = f"{int(time.time())}.jpg"
        save_path = os.path.join(save_dir, save_name)
        cv2.imwrite(save_path, self.current_frame)
        QMessageBox.information(self, "保存成功", f"图片已保存：{save_name}")
        self.refresh_img_list()

    def refresh_img_list(self):
        self.list_img.clear()
        task = self.edit_task.text().strip()
        scene = self.edit_scene.text().strip()
        img_dir = os.path.join("./dataset", task, scene)
        if not os.path.exists(img_dir):
            return
        img_files = [f for f in os.listdir(img_dir) if f.endswith((".jpg",".jpeg",".png"))]
        for name in img_files:
            QListWidgetItem(name, self.list_img)

    # ===================== 修复后的双击加载图片函数 =====================
    def load_selected_image(self, item):
        task = self.edit_task.text().strip()
        scene = self.edit_scene.text().strip()
        self.current_img_path = os.path.join("./dataset", task, scene, item.text())
        if not os.path.exists(self.current_img_path):
            QMessageBox.warning(self, "错误", "图片文件不存在！")
            return
        img = cv2.imread(self.current_img_path)
        if img is None:
            QMessageBox.warning(self, "错误", "图片损坏，无法读取！")
            return
        self.is_annotate_mode = True
        self.annotate_img = img
        self.show_frame_on_label(img)
        # 新增缓存画布+加载历史标注
        self.load_exist_labels()

    # ===================== 新增：加载历史标注 =====================
    def load_exist_labels(self):
        self.box_list.clear()
        if not self.current_img_path:
            return
        img_name = os.path.splitext(os.path.basename(self.current_img_path))[0]
        json_path = os.path.join(os.path.dirname(self.current_img_path), f"{img_name}.json")
        if os.path.exists(json_path):
            with open(json_path, "r", encoding="utf-8") as f:
                self.box_list = json.load(f)
        self.label_preview.update()

    # ===================== 新增：保存标注JSON =====================
    def save_label_json(self):
        if not self.current_img_path:
            return
        img_name = os.path.splitext(os.path.basename(self.current_img_path))[0]
        json_path = os.path.join(os.path.dirname(self.current_img_path), f"{img_name}.json")
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(self.box_list, f, ensure_ascii=False, indent=2)

    # ===================== 替换完成的清空标注函数 =====================
    def on_clear_all_label(self):
        if not self.current_img_path:
            QMessageBox.warning(self, "提示", "未打开标注图片！")
            return
        self.box_list.clear()
        img_name = os.path.splitext(os.path.basename(self.current_img_path))[0]
        json_path = os.path.join(os.path.dirname(self.current_img_path), f"{img_name}.json")
        if os.path.exists(json_path):
            os.remove(json_path)
        self.label_preview.update()
        QMessageBox.information(self, "完成", "已清空全部标注")

    # ===================== 新增标准Qt鼠标事件（替换旧mouse_down/move/up） =====================
    def mousePressEvent(self, event):
        if not self.is_annotate_mode:
            return
        if event.button() == Qt.MouseButton.LeftButton:
            self.draw_flag = True
            self.draw_start = (event.x(), event.y())
            self.draw_end = (event.x(), event.y())
            self.label_preview.update()

    def mouseMoveEvent(self, event):
        if not self.is_annotate_mode or not self.draw_flag:
            return
        self.draw_end = (event.x(), event.y())
        self.label_preview.update()

    def mouseReleaseEvent(self, event):
        if not self.is_annotate_mode or not self.draw_flag:
            return
        if event.button() == Qt.MouseButton.LeftButton:
            self.draw_flag = False
            x1, y1 = self.draw_start
            x2, y2 = self.draw_end
            drag_w = abs(x2 - x1)
            drag_h = abs(y2 - y1)
            # 过滤单击，拖动不足阈值不生成标注
            if drag_w < self.min_drag or drag_h < self.min_drag:
                self.label_preview.update()
                return
            x_min = min(x1, x2)
            y_min = min(y1, y2)
            x_max = max(x1, x2)
            y_max = max(y1, y2)
            cls_name = self.combo_class.currentText()
            self.box_list.append({
                "class": cls_name,
                "xmin": x_min,
                "ymin": y_min,
                "xmax": x_max,
                "ymax": y_max
            })
            self.save_label_json()
            QMessageBox.information(self, "标注完成", f"已添加类别：{cls_name}")
            self.label_preview.update()
    def resizeEvent(self, event):
        if self.current_frame is not None:
            self.show_frame_on_label(self.current_frame)
        super().resizeEvent(event)
# 程序入口
if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    def set_global_font():
        font = QApplication.font()
        font.setPointSize(11)
        QApplication.setFont(font)
    set_global_font()
    win = MainToolWindow()
    win.show()
    sys.exit(app.exec_())