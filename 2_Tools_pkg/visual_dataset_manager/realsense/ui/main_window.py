# ui/main_window.py
# 主窗口：三个选项卡（采集 / 标注 / 数据集管理）+ 菜单栏 + 状态栏

from pathlib import Path
import sys
import os
from PyQt5.QtWidgets import (
    QMainWindow, QTabWidget, QStatusBar,
    QAction, QMenuBar, QMessageBox, QLabel,
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QFont, QIcon

import config as cfg
from core import DatasetManager
from .capture_widget import CaptureWidget
from .annotate_widget import AnnotateWidget
from .dataset_widget  import DatasetWidget


class MainWindow(QMainWindow):
    """应用主窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowIcon(QIcon(str(Path(__file__).resolve().parents[1] / "assets" / "app_icon.png")))
        self.setWindowTitle(cfg.APP_TITLE)
        self.resize(cfg.WINDOW_W, cfg.WINDOW_H)

        # 共享数据集管理器（所有 tab 共用同一实例）
        self.dm = DatasetManager(base_dir=cfg.OUTPUT_DIR)

        self._build_menu()
        self._build_tabs()
        self._build_statusbar()
        self._connect_signals()

    # ─────────────────────────────────────────────────────────────
    # 菜单栏
    # ─────────────────────────────────────────────────────────────
    def _build_menu(self):
        mb = self.menuBar()
        mb.setNativeMenuBar(False)

        # 文件
        file_menu = mb.addMenu("文件(&F)")
        open_act  = QAction("打开数据集目录", self)
        open_act.triggered.connect(self._open_dataset_dir)
        file_menu.addAction(open_act)
        file_menu.addSeparator()
        quit_act = QAction("退出", self)
        quit_act.setShortcut("Ctrl+Q")
        quit_act.triggered.connect(self.close)
        file_menu.addAction(quit_act)

        # 视图
        view_menu = mb.addMenu("视图(&V)")
        for i, name in enumerate(["采集", "标注", "数据集管理"]):
            act = QAction(f"切换到「{name}」", self)
            act.setShortcut(f"Ctrl+{i+1}")
            # 闭包捕获 i
            act.triggered.connect(lambda _, idx=i: self.tabs.setCurrentIndex(idx))
            view_menu.addAction(act)

        # 帮助
        help_menu = mb.addMenu("帮助(&H)")
        about_act = QAction("关于", self)
        about_act.triggered.connect(self._show_about)
        help_menu.addAction(about_act)

    # ─────────────────────────────────────────────────────────────
    # 选项卡
    # ─────────────────────────────────────────────────────────────
    def _build_tabs(self):
        self.tabs = QTabWidget()
        self.tabs.setFont(QFont("Arial", 11))
        self.tabs.setDocumentMode(True)

        self.capture_tab = CaptureWidget(self.dm)
        self.annotate_tab = AnnotateWidget(self.dm)
        self.dataset_tab  = DatasetWidget(self.dm)

        self.tabs.addTab(self.capture_tab,  "📷  图像采集")
        self.tabs.addTab(self.annotate_tab, "🏷   图像标注")
        self.tabs.addTab(self.dataset_tab,  "🗂   数据集管理")

        self.setCentralWidget(self.tabs)
        self.tabs.currentChanged.connect(self._on_tab_change)

    # ─────────────────────────────────────────────────────────────
    # 状态栏
    # ─────────────────────────────────────────────────────────────
    def _build_statusbar(self):
        sb = self.statusBar()
        mode_text = "模式：模拟相机" if cfg.USE_MOCK else "模式：RealSense 真机"
        mode_lbl  = QLabel(f"  {mode_text}  |  数据集根目录：{os.path.abspath(cfg.OUTPUT_DIR)}  ")
        mode_lbl.setStyleSheet("color:#aaa; font-size:11px;")
        sb.addPermanentWidget(mode_lbl)
        sb.showMessage("就绪", 3000)

    # ─────────────────────────────────────────────────────────────
    # 信号连接：采集 → 标注 / 数据集 同步刷新
    # ─────────────────────────────────────────────────────────────
    def _connect_signals(self):
        self.capture_tab.image_saved.connect(self.annotate_tab.on_image_saved)
        self.capture_tab.image_saved.connect(self.dataset_tab.on_image_saved)
        self.capture_tab.image_saved.connect(self._on_image_saved)

    def _on_image_saved(self, task_path: str, rec: dict):
        self.statusBar().showMessage(
            f"✅ 已保存：{rec['filename']}  →  {task_path}", 4000
        )

    # ─────────────────────────────────────────────────────────────
    # Tab 切换时自动刷新
    # ─────────────────────────────────────────────────────────────
    def _on_tab_change(self, idx: int):
        if idx == 1:   # 标注 tab
            self.annotate_tab.refresh_tasks()
        elif idx == 2: # 数据集 tab
            self.dataset_tab.refresh_classes()
            self.dataset_tab.refresh()

    # ─────────────────────────────────────────────────────────────
    # 菜单动作
    # ─────────────────────────────────────────────────────────────
    def _open_dataset_dir(self):
        import subprocess
        path = os.path.abspath(cfg.OUTPUT_DIR)
        os.makedirs(path, exist_ok=True)
        if sys.platform == "win32":
            os.startfile(path)
        elif sys.platform == "darwin":
            subprocess.run(["open", path])
        else:
            subprocess.run(["xdg-open", path])

    def _show_about(self):
        QMessageBox.about(
            self, "关于",
            f"<b>{cfg.APP_TITLE}</b><br><br>"
            "基于 RealSense 相机的图像数据采集与标注工具<br>"
            "标注格式：YOLO / PASCAL VOC<br><br>"
            "快捷键：<br>"
            "· 采集界面：<b>空格</b> 拍照<br>"
            "· 标注界面：<b>S</b> 保存  |  <b>Del</b> 删除框  "
            "|  <b>←/→</b> 切换图像<br>"
            "· 全局：<b>Ctrl+1/2/3</b> 切换选项卡"
        )

    # ─────────────────────────────────────────────────────────────
    # 退出时释放相机
    # ─────────────────────────────────────────────────────────────
    def closeEvent(self, event):
        if self.capture_tab.camera and self.capture_tab.camera.is_running:
            self.capture_tab._on_stop()
        super().closeEvent(event)
