# ui/capture_widget.py
# 采集界面：
#   - 左侧实时预览（彩色图 + 可切换深度图）
#   - 右侧控制面板（任务选择、单张采集、连拍、保存路径）
#   - 底部状态栏（帧率、已采集数）
 
import os
import cv2
import numpy as np
from PyQt5.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLabel, QPushButton,
    QLineEdit, QComboBox, QSpinBox, QGroupBox, QCheckBox,
    QSizePolicy, QMessageBox, QFrame,
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal
from PyQt5.QtGui import QImage, QPixmap, QFont
 
import config as cfg
from camera import create_camera
from core import DatasetManager
from core import settings
from core.logger import get_logger
 
logger = get_logger(__name__)
 
 
class PreviewLabel(QLabel):
    """可自动缩放的图像预览控件"""
 
    def __init__(self):
        super().__init__()
        self.setAlignment(Qt.AlignCenter)
        self.setMinimumSize(480, 360)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setStyleSheet("background:#111; border-radius:6px;")
        self.setText("相机未启动")
        self.setFont(QFont("Arial", 14))
        self.setStyleSheet("background:#1a1a1a; color:#888; border-radius:6px;")
 
    def set_frame(self, bgr: np.ndarray):
        h, w, ch = bgr.shape
        rgb       = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
        qi        = QImage(rgb.data, w, h, ch * w, QImage.Format_RGB888)
        px        = QPixmap.fromImage(qi).scaled(
            self.width(), self.height(),
            Qt.KeepAspectRatio, Qt.SmoothTransformation
        )
        self.setPixmap(px)
 
 
class CaptureWidget(QWidget):
    """图像采集选项卡"""
 
    # 信号：采集到新图像后通知其他 tab（可用于自动跳转到标注）
    image_saved = pyqtSignal(str, dict)   # (task_path, image_record)
 
    def __init__(self, dataset_manager: DatasetManager, parent=None):
        super().__init__(parent)
        self.dm      = dataset_manager
        self.camera  = None
        self._timer  = QTimer(self)
        self._timer.timeout.connect(self._update_frame)
        self._show_depth    = False
        self._current_task  = ""
        self._task_path     = ""
        self._frame_color   = None
        self._frame_depth   = None
        self._fps_counter   = 0
        self._fps_timer     = QTimer(self)
        self._fps_timer.timeout.connect(self._calc_fps)
        self._fps_val       = 0
        self._first_frame   = True   # 用于诊断日志
 
        self._build_ui()
        self._refresh_tasks()
 
    # ─────────────────────────────────────────────────────────────
    # UI 构建
    # ─────────────────────────────────────────────────────────────
    def _build_ui(self):
        root = QHBoxLayout(self)
        root.setSpacing(12)
        root.setContentsMargins(12, 12, 12, 12)
 
        # 左：预览区
        left = QVBoxLayout()
        self.preview = PreviewLabel()
        left.addWidget(self.preview)
 
        # 深度图切换
        self.depth_cb = QCheckBox("显示深度图（伪彩色）")
        self.depth_cb.toggled.connect(self._on_depth_toggle)
        left.addWidget(self.depth_cb)
 
        # 状态条
        self.status_lbl = QLabel("帧率: -- fps  |  已采集: 0 张")
        self.status_lbl.setAlignment(Qt.AlignCenter)
        self.status_lbl.setStyleSheet("color:#aaa; font-size:12px;")
        left.addWidget(self.status_lbl)
 
        root.addLayout(left, 3)
 
        # 右：控制面板
        right = QVBoxLayout()
        right.setSpacing(10)
 
        # ── 相机控制 ──
        cam_box = QGroupBox("相机")
        cam_lay = QVBoxLayout(cam_box)
        mode_lbl = QLabel(f"当前模式：{'模拟相机' if cfg.USE_MOCK else 'RealSense 真机'}")
        mode_lbl.setStyleSheet("color:#4fc3f7; font-weight:bold;")
        cam_lay.addWidget(mode_lbl)
 
        self.start_btn = QPushButton("▶ 启动相机")
        self.start_btn.setFixedHeight(36)
        self.start_btn.clicked.connect(self._on_start)
        self.start_btn.setStyleSheet(self._btn_style("#2e7d32", "#43a047"))
        cam_lay.addWidget(self.start_btn)
 
        self.stop_btn = QPushButton("■ 停止相机")
        self.stop_btn.setFixedHeight(36)
        self.stop_btn.setEnabled(False)
        self.stop_btn.clicked.connect(self._on_stop)
        self.stop_btn.setStyleSheet(self._btn_style("#b71c1c", "#e53935"))
        cam_lay.addWidget(self.stop_btn)
        right.addWidget(cam_box)
 
        # ── 任务管理 ──
        task_box = QGroupBox("任务 / 场景")
        task_lay = QVBoxLayout(task_box)
 
        task_row = QHBoxLayout()
        self.task_combo = QComboBox()
        self.task_combo.setEditable(True)
        self.task_combo.setPlaceholderText("选择或输入任务名称...")
        self.task_combo.currentTextChanged.connect(self._on_task_change)
        task_row.addWidget(self.task_combo)
        self.new_task_btn = QPushButton("＋ 新建")
        self.new_task_btn.setFixedWidth(72)
        self.new_task_btn.clicked.connect(self._on_new_task)
        task_row.addWidget(self.new_task_btn)
        task_lay.addLayout(task_row)
 
        self.task_info = QLabel("尚未选择任务")
        self.task_info.setStyleSheet("color:#aaa; font-size:11px;")
        task_lay.addWidget(self.task_info)
        right.addWidget(task_box)
 
        # ── 采集控制 ──
        cap_box = QGroupBox("采集")
        cap_lay = QVBoxLayout(cap_box)
 
        # 单张采集
        self.capture_btn = QPushButton("📷  拍摄（空格键）")
        self.capture_btn.setFixedHeight(48)
        self.capture_btn.setEnabled(False)
        self.capture_btn.clicked.connect(self._on_capture)
        self.capture_btn.setStyleSheet(self._btn_style("#1565c0", "#1e88e5"))
        cap_lay.addWidget(self.capture_btn)
 
        # 连拍
        burst_row = QHBoxLayout()
        self.burst_btn = QPushButton("🔄 连拍")
        self.burst_btn.setCheckable(True)
        self.burst_btn.setEnabled(False)
        self.burst_btn.clicked.connect(self._on_burst)
        self.burst_btn.setStyleSheet(self._btn_style("#4a148c", "#7b1fa2"))
        burst_row.addWidget(self.burst_btn)
 
        burst_row.addWidget(QLabel("间隔(s):"))
        self.burst_interval = QSpinBox()
        self.burst_interval.setRange(1, 60)
        self.burst_interval.setValue(2)
        self.burst_interval.setFixedWidth(56)
        burst_row.addWidget(self.burst_interval)
        cap_lay.addLayout(burst_row)
 
        self._burst_timer = QTimer()
        self._burst_timer.timeout.connect(self._on_capture)
        right.addWidget(cap_box)
 
        # ── 深度图保存 ──
        opt_box = QGroupBox("保存选项")
        opt_lay = QVBoxLayout(opt_box)
        self.save_depth_cb = QCheckBox("同时保存深度图")
        self.save_depth_cb.setChecked(True)
        opt_lay.addWidget(self.save_depth_cb)
        right.addWidget(opt_box)
 
        right.addStretch()
        root.addLayout(right, 1)
        self.setFocusPolicy(Qt.StrongFocus)   # 确保采集页能捕获空格键
 
    # ─────────────────────────────────────────────────────────────
    # 槽函数
    # ─────────────────────────────────────────────────────────────
    def _on_start(self):
        self.camera = create_camera()
        if not self.camera.start():
            QMessageBox.critical(self, "错误", "相机启动失败，请检查设备连接。")
            return
        interval = settings.get("timer_interval_ms", int(1000 / cfg.CAMERA_FPS))
        self._timer.start(interval)
        self._fps_timer.start(1000)
        self._first_frame = True
        self.start_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self.capture_btn.setEnabled(bool(self._task_path))
        self.burst_btn.setEnabled(bool(self._task_path))
        logger.info("相机已启动")
 
    def _on_stop(self):
        self._timer.stop()
        self._fps_timer.stop()
        self._burst_timer.stop()
        if self.camera:
            self.camera.stop()
            self.camera = None
        self.preview.setText("相机已停止")
        self.start_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.capture_btn.setEnabled(False)
        self.burst_btn.setEnabled(False)
 
    def _update_frame(self):
        if not self.camera:
            return
        color, depth = self.camera.get_frame()
 
        # Bug Fix：get_frame() 返回 None 时保留上一帧数据，
        # 不直接 return，否则 _frame_color 永远是 None，
        # 导致 _on_capture 第一行 guard 直接退出，图片永远存不进去。
        if color is not None:
            self._frame_color = color
            self._frame_depth = depth
            if self._first_frame:
                self._first_frame = False
                logger.info(f"收到首帧：shape={color.shape}, dtype={color.dtype}")
        else:
            logger.warning("相机返回空帧，保留上一帧数据")
 
        # 没有任何帧时不更新预览（首次启动前）
        if self._frame_color is None:
            return
 
        self._fps_counter += 1
 
        if self._show_depth and self._frame_depth is not None:
            # 深度图归一化后转伪彩色
            norm = cv2.normalize(self._frame_depth, None, 0, 255, cv2.NORM_MINMAX, cv2.CV_8U)
            disp = cv2.applyColorMap(norm, cv2.COLORMAP_JET)
        else:
            disp = self._frame_color
 
        self.preview.set_frame(disp)
 
    def _calc_fps(self):
        self._fps_val     = self._fps_counter
        self._fps_counter = 0
        imgs = len(self.dm.get_images(self._task_path)) if self._task_path else 0
        self.status_lbl.setText(
            f"帧率: {self._fps_val} fps  |  已采集: {imgs} 张"
        )
 
    def _on_depth_toggle(self, checked: bool):
        self._show_depth = checked
 
    def _on_task_change(self, name: str):
        self._current_task = name.strip()
        # 若已存在此任务，同步更新任务路径和信息
        if name.strip() in self.dm.get_tasks():
            self._task_path = self.dm.get_task_path(name.strip())
            self._update_task_info()
            if self.camera and self.camera.is_running:
                self.capture_btn.setEnabled(True)
                self.burst_btn.setEnabled(True)
 
    def _on_new_task(self):
        name = self.task_combo.currentText().strip()
        if not name:
            QMessageBox.warning(self, "提示", "请先在输入框中填写任务名称")
            return
        path = self.dm.create_task(name)
        self._task_path = path
        self._refresh_tasks()
        idx = self.task_combo.findText(name)
        if idx >= 0:
            self.task_combo.setCurrentIndex(idx)
        self._update_task_info()
        settings.set_and_save("last_task", name)
        if self.camera and self.camera.is_running:
            self.capture_btn.setEnabled(True)
            self.burst_btn.setEnabled(True)
 
    def _refresh_tasks(self):
        current = self.task_combo.currentText() or settings.get("last_task", "")
        self.task_combo.blockSignals(True)
        self.task_combo.clear()
        for t in self.dm.get_tasks():
            self.task_combo.addItem(t)
        idx = self.task_combo.findText(current)
        if idx >= 0:
            self.task_combo.setCurrentIndex(idx)
        self.task_combo.blockSignals(False)
 
        name = self.task_combo.currentText()
        if name:
            self._task_path = self.dm.get_task_path(name)
            self._update_task_info()
 
    def _update_task_info(self):
        if not self._task_path:
            self.task_info.setText("尚未选择任务")
            return
        task_name = os.path.basename(self._task_path) if self._task_path else ""
        stats = self.dm.get_stats(task_name) if task_name in self.dm.get_tasks() else {}
        total = stats.get("total", 0)
        ann   = stats.get("annotated", 0)
        self.task_info.setText(
            f"路径：{self._task_path}\n"
            f"共 {total} 张图  |  已标注 {ann} 张"
        )
 
    def _on_capture(self):
        if self._frame_color is None:
            QMessageBox.warning(self, "提示", 
                "相机尚未准备好图像，请等待预览画面出现后再拍摄。")
            return
        if not self._task_path:
            QMessageBox.warning(self, "提示", "请先选择或创建任务")
            return
        try:
            depth = self._frame_depth if self.save_depth_cb.isChecked() else None
            rec   = self.dm.save_image(self._task_path, self._frame_color.copy(), depth)
            self._update_task_info()
            self.image_saved.emit(self._task_path, rec)
            logger.info(f"图像保存成功：{rec['filename']} → {rec['color_path']}")
        except Exception as e:
            logger.error(f"保存图像失败：{e}", exc_info=True)
            QMessageBox.critical(self, "保存失败", 
                f"图像保存失败：{e}\n\n请检查：\n1. 磁盘空间是否充足\n"
                f"2. 程序对 {self._task_path} 是否有写入权限\n"
                f"3. 数据集目录是否被其他程序占用")
 
    def _on_burst(self, checked: bool):
        if checked:
            if not self._task_path:
                QMessageBox.warning(self, "提示", "请先选择或创建任务")
                self.burst_btn.setChecked(False)
                return
            interval = self.burst_interval.value() * 1000
            self._burst_timer.start(interval)
            self.burst_btn.setText("⏹ 停止连拍")
        else:
            self._burst_timer.stop()
            self.burst_btn.setText("🔄 连拍")
 
    # ─────────────────────────────────────────────────────────────
    # 快捷键：空格拍照
    # ─────────────────────────────────────────────────────────────
    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Space:
            self._on_capture()
        super().keyPressEvent(event)
 
    # ─────────────────────────────────────────────────────────────
    # 工具
    # ─────────────────────────────────────────────────────────────
    @staticmethod
    def _btn_style(dark: str, hover: str) -> str:
        return (
            f"QPushButton{{background:{dark};color:#fff;border-radius:4px;"
            f"font-size:13px;}} "
            f"QPushButton:hover{{background:{hover};}} "
            f"QPushButton:disabled{{background:#444;color:#888;}}"
        )
 
    def closeEvent(self, event):
        self._on_stop()
        super().closeEvent(event)