#!/usr/bin/env python3
# main.py — 程序入口
# 用法：python main.py
#
# 切换真机 / 模拟：编辑 config.py 中的 USE_MOCK

import sys
import os

os.chdir(os.path.dirname(os.path.abspath(__file__)))

from PyQt5.QtWidgets import QApplication
from PyQt5.QtGui import QFont
from PyQt5.QtCore import Qt

from core import settings
from core.logger import get_logger
from ui import MainWindow
import config as cfg

logger = get_logger(__name__)


def main():
    # 加载用户设置
    settings.load()

    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps,   True)

    app = QApplication(sys.argv)
    app.setApplicationName(cfg.APP_TITLE)

    font = QFont("Segoe UI" if sys.platform == "win32" else "Arial", 10)
    app.setFont(font)
    app.setStyleSheet(_dark_stylesheet())

    window = MainWindow()

    # 恢复窗口大小
    w = settings.get("window_w", cfg.WINDOW_W)
    h = settings.get("window_h", cfg.WINDOW_H)
    window.resize(w, h)
    if settings.get("window_maximized", False):
        window.showMaximized()
    else:
        window.show()

    logger.info(f"应用启动  模式={'模拟' if cfg.USE_MOCK else '真机'}")
    code = app.exec_()

    # 保存窗口大小
    settings.set("window_maximized", window.isMaximized())
    if not window.isMaximized():
        settings.set("window_w", window.width())
        settings.set("window_h", window.height())
    settings.save()
    logger.info("应用退出")
    sys.exit(code)


def _dark_stylesheet() -> str:
    return """
    QMainWindow, QWidget {
        background-color: #2b2b2b;
        color: #ddd;
    }
    QTabWidget::pane {
        border: 1px solid #444;
        border-radius: 4px;
    }
    QTabBar::tab {
        background: #3c3c3c;
        color: #aaa;
        padding: 8px 20px;
        border-top-left-radius: 4px;
        border-top-right-radius: 4px;
        font-size: 12px;
    }
    QTabBar::tab:selected {
        background: #1e88e5;
        color: #fff;
        font-weight: bold;
    }
    QTabBar::tab:hover:!selected {
        background: #4a4a4a;
        color: #fff;
    }
    QGroupBox {
        border: 1px solid #555;
        border-radius: 5px;
        margin-top: 8px;
        padding-top: 6px;
        font-weight: bold;
        color: #bbb;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 10px;
        padding: 0 4px;
    }
    QPushButton {
        background: #3c3c3c;
        color: #ddd;
        border: 1px solid #555;
        border-radius: 4px;
        padding: 5px 12px;
        font-size: 12px;
    }
    QPushButton:hover  { background: #4a4a4a; color: #fff; }
    QPushButton:pressed { background: #222; }
    QPushButton:disabled { background: #333; color: #666; border-color: #444; }
    QComboBox, QLineEdit, QSpinBox {
        background: #3a3a3a;
        color: #ddd;
        border: 1px solid #555;
        border-radius: 4px;
        padding: 4px 8px;
    }
    QComboBox::drop-down { border: none; }
    QComboBox QAbstractItemView {
        background: #3a3a3a;
        color: #ddd;
        selection-background-color: #1e88e5;
    }
    QListWidget, QTreeWidget {
        background: #252525;
        color: #ddd;
        border: 1px solid #444;
        border-radius: 4px;
        alternate-background-color: #2e2e2e;
    }
    QListWidget::item:selected, QTreeWidget::item:selected {
        background: #1565c0;
        color: #fff;
    }
    QListWidget::item:hover, QTreeWidget::item:hover {
        background: #333;
    }
    QHeaderView::section {
        background: #3c3c3c;
        color: #bbb;
        border: 1px solid #444;
        padding: 4px;
    }
    QScrollBar:vertical {
        background: #2b2b2b;
        width: 10px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical {
        background: #555;
        border-radius: 5px;
        min-height: 30px;
    }
    QScrollBar::handle:vertical:hover { background: #777; }
    QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
    QStatusBar { background: #222; color: #888; font-size: 11px; }
    QMenuBar { background: #333; color: #ccc; }
    QMenuBar::item:selected { background: #1e88e5; color: #fff; }
    QMenu { background: #333; color: #ccc; border: 1px solid #555; }
    QMenu::item:selected { background: #1e88e5; color: #fff; }
    QSplitter::handle { background: #444; }
    QCheckBox { color: #ccc; spacing: 6px; }
    QCheckBox::indicator {
        width: 16px; height: 16px;
        border: 1px solid #666;
        border-radius: 3px;
        background: #3a3a3a;
    }
    QCheckBox::indicator:checked { background: #1e88e5; border-color: #1e88e5; }
    QMessageBox { background: #2b2b2b; color: #ddd; }
    """


if __name__ == "__main__":
    main()
