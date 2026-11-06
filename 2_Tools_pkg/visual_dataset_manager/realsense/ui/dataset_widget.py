# ui/dataset_widget.py
# 数据集管理界面：
#   - 浏览所有任务和图像（树形结构）
#   - 预览选中图像及其标注框
#   - 删除图像 / 整个任务
#   - 一键导出（YOLO / VOC）

import os
import cv2
import numpy as np
from PyQt5.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLabel, QPushButton,
    QTreeWidget, QTreeWidgetItem, QGroupBox, QComboBox,
    QFileDialog, QMessageBox, QSplitter, QSizePolicy,
    QProgressDialog, QApplication, QDoubleSpinBox,
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QImage, QPixmap, QColor, QFont

import config as cfg
from core import DatasetManager, Exporter
from core import settings
from core.logger import get_logger

logger = get_logger(__name__)


class DatasetWidget(QWidget):
    """数据集管理选项卡"""

    def __init__(self, dataset_manager: DatasetManager, parent=None):
        super().__init__(parent)
        self.dm       = dataset_manager
        self.exporter = Exporter()
        # 使用用户在标注界面设置的类别（已持久化），而非硬编码的默认值
        self._classes = list(settings.get("classes", cfg.DEFAULT_CLASSES))

        self._build_ui()
        self.refresh()

    # ─────────────────────────────────────────────────────────────
    # UI 构建
    # ─────────────────────────────────────────────────────────────
    def _build_ui(self):
        root     = QHBoxLayout(self)
        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        # ── 左侧：任务/图像树 ──
        left = QWidget()
        ll   = QVBoxLayout(left)
        ll.setContentsMargins(8, 8, 4, 8)

        lbl = QLabel("数据集")
        lbl.setFont(QFont("Arial", 11, QFont.Bold))
        ll.addWidget(lbl)

        self.tree = QTreeWidget()
        self.tree.setHeaderLabels(["名称", "状态"])
        self.tree.setColumnWidth(0, 200)
        self.tree.itemSelectionChanged.connect(self._on_tree_select)
        ll.addWidget(self.tree)

        btn_row = QHBoxLayout()
        refresh_btn = QPushButton("↻ 刷新")
        refresh_btn.clicked.connect(self.refresh)
        btn_row.addWidget(refresh_btn)

        self.del_btn = QPushButton("🗑 删除")
        self.del_btn.setEnabled(False)
        self.del_btn.clicked.connect(self._on_delete)
        self.del_btn.setStyleSheet(
            "QPushButton{background:#c62828;color:#fff;border-radius:4px;}"
            "QPushButton:hover{background:#ef5350;}"
            "QPushButton:disabled{background:#333;color:#666;}"
        )
        btn_row.addWidget(self.del_btn)
        ll.addLayout(btn_row)

        left.setMinimumWidth(250)
        splitter.addWidget(left)

        # ── 中央：图像预览 ──
        mid = QWidget()
        ml  = QVBoxLayout(mid)
        ml.setContentsMargins(4, 8, 4, 8)

        self.preview = QLabel()
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setMinimumSize(400, 300)
        self.preview.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.preview.setStyleSheet("background:#111; border-radius:6px; color:#666;")
        self.preview.setText("← 点击左侧图像预览")
        ml.addWidget(self.preview)

        self.img_info = QLabel("")
        self.img_info.setAlignment(Qt.AlignCenter)
        self.img_info.setStyleSheet("color:#aaa; font-size:11px;")
        ml.addWidget(self.img_info)
        splitter.addWidget(mid)

        # ── 右侧：统计 + 导出 ──
        right = QWidget()
        rl    = QVBoxLayout(right)
        rl.setContentsMargins(4, 8, 8, 8)
        rl.setSpacing(10)

        # 统计信息
        stat_box = QGroupBox("统计信息")
        stat_lay = QVBoxLayout(stat_box)
        self.stat_lbl = QLabel("请选择任务")
        self.stat_lbl.setWordWrap(True)
        self.stat_lbl.setStyleSheet("color:#ccc; line-height:180%;")
        stat_lay.addWidget(self.stat_lbl)
        rl.addWidget(stat_box)

        # 导出
        exp_box = QGroupBox("导出数据集")
        exp_lay = QVBoxLayout(exp_box)

        exp_lay.addWidget(QLabel("导出格式："))
        self.fmt_combo = QComboBox()
        self.fmt_combo.addItems(["YOLO（YOLOv5/v8）", "PASCAL VOC"])
        # 从设置恢复上次格式
        saved_fmt = settings.get("export_format", "yolo")
        self.fmt_combo.setCurrentIndex(0 if saved_fmt == "yolo" else 1)
        self.fmt_combo.currentIndexChanged.connect(
            lambda i: settings.set_and_save("export_format", "yolo" if i == 0 else "voc")
        )
        exp_lay.addWidget(self.fmt_combo)

        # train/val 比例
        ratio_row = QHBoxLayout()
        ratio_row.addWidget(QLabel("训练集比例："))
        self.ratio_spin = QDoubleSpinBox()
        self.ratio_spin.setRange(0.5, 0.95)
        self.ratio_spin.setSingleStep(0.05)
        self.ratio_spin.setDecimals(2)
        self.ratio_spin.setValue(settings.get("train_ratio", 0.8))
        self.ratio_spin.setSuffix("  (剩余为验证集)")
        self.ratio_spin.valueChanged.connect(
            lambda v: settings.set_and_save("train_ratio", v)
        )
        ratio_row.addWidget(self.ratio_spin)
        exp_lay.addLayout(ratio_row)

        exp_lay.addWidget(QLabel("导出任务："))
        self.exp_task_combo = QComboBox()
        exp_lay.addWidget(self.exp_task_combo)

        exp_btn = QPushButton("📦 选择路径并导出")
        exp_btn.setFixedHeight(40)
        exp_btn.clicked.connect(self._on_export)
        exp_btn.setStyleSheet(
            "QPushButton{background:#1b5e20;color:#fff;border-radius:4px;font-size:13px;}"
            "QPushButton:hover{background:#2e7d32;}"
        )
        exp_lay.addWidget(exp_btn)
        rl.addWidget(exp_box)

        # 快捷操作
        ops_box = QGroupBox("批量操作")
        ops_lay = QVBoxLayout(ops_box)
        open_folder_btn = QPushButton("📂 打开数据集根目录")
        open_folder_btn.clicked.connect(self._open_base_dir)
        ops_lay.addWidget(open_folder_btn)
        rl.addWidget(ops_box)

        rl.addStretch()
        right.setMinimumWidth(220)
        right.setMaximumWidth(300)
        splitter.addWidget(right)

        splitter.setSizes([260, 700, 260])

    # ─────────────────────────────────────────────────────────────
    # 刷新树形列表
    # ─────────────────────────────────────────────────────────────
    def refresh(self):
        self.tree.clear()
        self.exp_task_combo.clear()

        for task_name in self.dm.get_tasks():
            stats     = self.dm.get_stats(task_name)
            task_path = self.dm.get_task_path(task_name)
            records   = self.dm.get_images(task_path)

            task_item = QTreeWidgetItem(self.tree)
            task_item.setText(0, f"📁 {task_name}")
            task_item.setText(1, f"{stats['total']} 图 / {stats['annotated']} 标注")
            task_item.setData(0, Qt.UserRole, ("task", task_name))
            task_item.setForeground(1, QColor("#80cbc4"))

            for rec in records:
                img_item = QTreeWidgetItem(task_item)
                status   = "✅" if rec.get("annotated") else "○"
                img_item.setText(0, f"  {status} {rec['filename']}")
                img_item.setText(1, f"{rec.get('width', '?')}×{rec.get('height', '?')}")
                img_item.setData(0, Qt.UserRole, ("image", task_path, rec))
                if rec.get("annotated"):
                    img_item.setForeground(0, QColor("#a5d6a7"))

            self.exp_task_combo.addItem(task_name)

        self.tree.expandAll()

    # ─────────────────────────────────────────────────────────────
    # 树点击
    # ─────────────────────────────────────────────────────────────
    def _on_tree_select(self):
        items = self.tree.selectedItems()
        if not items:
            return
        data = items[0].data(0, Qt.UserRole)
        if not data:
            return

        if data[0] == "task":
            task_name = data[1]
            stats     = self.dm.get_stats(task_name)
            self.stat_lbl.setText(
                f"任务：{task_name}\n"
                f"图像总数：{stats['total']}\n"
                f"已标注：{stats['annotated']}\n"
                f"未标注：{stats['unannotated']}\n"
                f"路径：{self.dm.get_task_path(task_name)}"
            )
            self.del_btn.setEnabled(True)
            self.preview.setText("← 请选择具体图像")

        elif data[0] == "image":
            _, task_path, rec = data
            self._show_preview(task_path, rec)
            self.del_btn.setEnabled(True)

    def _show_preview(self, task_path: str, rec: dict):
        img_path = rec.get("color_path", "")
        if not img_path or not os.path.exists(img_path):
            self.preview.setText("⚠ 图像文件不存在")
            return

        bgr = cv2.imread(img_path)
        if bgr is None:
            return

        # 画标注框（如有）
        lbl_path = rec.get("label_path", "")
        if lbl_path and os.path.exists(lbl_path):
            bgr = self._draw_labels_on_img(bgr, lbl_path)

        h, w = bgr.shape[:2]
        rgb  = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
        qi   = QImage(rgb.data, w, h, 3 * w, QImage.Format_RGB888)
        px   = QPixmap.fromImage(qi).scaled(
            self.preview.width(), self.preview.height(),
            Qt.KeepAspectRatio, Qt.SmoothTransformation
        )
        self.preview.setPixmap(px)
        ann_str = "已标注" if rec.get("annotated") else "未标注"
        self.img_info.setText(
            f"{rec['filename']}  |  {w}×{h}  |  {ann_str}  |  "
            f"保存于 {rec.get('saved_at', '')[:19]}"
        )
        self.stat_lbl.setText(
            f"文件：{rec['filename']}\n"
            f"尺寸：{w}×{h}\n"
            f"状态：{ann_str}\n"
            f"深度图：{'有' if rec.get('depth_path') else '无'}\n"
            f"标注文件：{os.path.basename(lbl_path) if lbl_path else '无'}"
        )

    def _draw_labels_on_img(self, bgr: np.ndarray, lbl_path: str) -> np.ndarray:
        """在图像上绘制已有标注框（用于预览）"""
        img = bgr.copy()
        h, w = img.shape[:2]
        with open(lbl_path, "r") as f:
            for i, line in enumerate(f):
                parts = line.strip().split()
                if len(parts) != 5:
                    continue
                cid = int(parts[0])
                cx, cy, bw, bh = float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4])
                x1 = int((cx - bw / 2) * w)
                y1 = int((cy - bh / 2) * h)
                x2 = int((cx + bw / 2) * w)
                y2 = int((cy + bh / 2) * h)
                colors = cfg.BOX_COLORS
                color  = colors[cid % len(colors)]
                cv2.rectangle(img, (x1, y1), (x2, y2), color[::-1], 2)   # RGB→BGR
                cls_name = self._classes[cid] if cid < len(self._classes) else f"cls{cid}"
                cv2.putText(img, cls_name, (x1, max(y1 - 4, 12)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, color[::-1], 2)
        return img

    # ─────────────────────────────────────────────────────────────
    # 删除
    # ─────────────────────────────────────────────────────────────
    def _on_delete(self):
        items = self.tree.selectedItems()
        if not items:
            return
        data = items[0].data(0, Qt.UserRole)
        if not data:
            return

        if data[0] == "task":
            task_name = data[1]
            reply = QMessageBox.question(
                self, "确认删除",
                f"确定删除整个任务「{task_name}」及其所有图像和标注？\n此操作不可撤销！",
                QMessageBox.Yes | QMessageBox.No,
            )
            if reply == QMessageBox.Yes:
                self.dm.delete_task(task_name)
                self.refresh()

        elif data[0] == "image":
            _, task_path, rec = data
            reply = QMessageBox.question(
                self, "确认删除",
                f"确定删除图像「{rec['filename']}」及其标注？",
                QMessageBox.Yes | QMessageBox.No,
            )
            if reply == QMessageBox.Yes:
                self.dm.delete_image(task_path, rec["id"])
                self.refresh()
                self.preview.setText("图像已删除")

    # ─────────────────────────────────────────────────────────────
    # 导出
    # ─────────────────────────────────────────────────────────────
    def _on_export(self):
        task_name = self.exp_task_combo.currentText()
        if not task_name:
            QMessageBox.warning(self, "提示", "没有可导出的任务")
            return

        out_dir = QFileDialog.getExistingDirectory(
            self, "选择导出目录", os.path.expanduser("~")
        )
        if not out_dir:
            return

        task_path = self.dm.get_task_path(task_name)
        records   = self.dm.get_images(task_path)
        fmt       = self.fmt_combo.currentIndex()   # 0=YOLO, 1=VOC
        ratio     = self.ratio_spin.value()

        export_to = os.path.join(out_dir, f"{task_name}_export")

        dlg = QProgressDialog("正在导出...", None, 0, 0, self)
        dlg.setWindowTitle("导出中")
        dlg.setWindowModality(Qt.WindowModal)
        dlg.show()
        QApplication.processEvents()

        try:
            if fmt == 0:
                ok, skip = self.exporter.export_yolo(
                    task_path, records, self._classes, export_to,
                    train_ratio=ratio,
                )
            else:
                ok, skip = self.exporter.export_voc(
                    task_path, records, self._classes, export_to,
                    train_ratio=ratio,
                )
        except Exception as e:
            logger.error(f"导出失败：{e}", exc_info=True)
            QMessageBox.critical(self, "导出失败", str(e))
            dlg.close()
            return

        dlg.close()
        fmt_name = "YOLO" if fmt == 0 else "PASCAL VOC"
        train_n  = max(1, int(ok * ratio))
        val_n    = ok - train_n
        QMessageBox.information(
            self, "导出完成",
            f"格式：{fmt_name}\n"
            f"训练集：{train_n} 张  验证集：{val_n} 张\n"
            f"跳过：{skip} 张\n"
            f"输出目录：{export_to}"
        )

    def _open_base_dir(self):
        path = os.path.abspath(self.dm.base_dir)
        os.makedirs(path, exist_ok=True)
        # 跨平台打开文件管理器
        import subprocess, sys
        if sys.platform == "win32":
            os.startfile(path)
        elif sys.platform == "darwin":
            subprocess.run(["open", path])
        else:
            subprocess.run(["xdg-open", path])

    # 外部刷新入口
    def on_image_saved(self, _task_path, _rec):
        self.refresh()

    def refresh_classes(self):
        """从 settings 同步最新类别列表（由 MainWindow 在 tab 切换时调用）"""
        self._classes = list(settings.get("classes", cfg.DEFAULT_CLASSES))
