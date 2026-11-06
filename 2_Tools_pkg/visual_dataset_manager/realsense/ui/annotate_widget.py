# ui/annotate_widget.py
# 标注界面：
#   - 左侧：图像列表（带标注状态指示）
#   - 中央：标注画布（鼠标拖拽画框，支持选中/删除/键盘微调）
#   - 右侧：类别管理 + 框列表
#
# 标注格式：YOLO .txt（归一化 cx cy w h）

import os
import cv2
import numpy as np
from PyQt5.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLabel, QPushButton,
    QListWidget, QListWidgetItem, QComboBox, QLineEdit,
    QGroupBox, QMessageBox, QSizePolicy, QSplitter,
    QScrollArea, QAbstractItemView, QCheckBox,
)
from PyQt5.QtCore import Qt, QRect, QPoint, pyqtSignal
from PyQt5.QtGui import (
    QImage, QPixmap, QPainter, QPen, QBrush, QColor,
    QFont, QCursor,
)

import config as cfg
from core import DatasetManager
from core import settings
from core.logger import get_logger

logger = get_logger(__name__)


# ─────────────────────────────────────────────────────────────────
# 标注数据结构
# ─────────────────────────────────────────────────────────────────
class BBox:
    """单个标注框（像素坐标）"""
    def __init__(self, x1: int, y1: int, x2: int, y2: int, cls_id: int = 0):
        self.x1     = min(x1, x2)
        self.y1     = min(y1, y2)
        self.x2     = max(x1, x2)
        self.y2     = max(y1, y2)
        self.cls_id = cls_id

    def valid(self) -> bool:
        return (self.x2 - self.x1) > 4 and (self.y2 - self.y1) > 4

    def to_yolo(self, img_w: int, img_h: int) -> str:
        cx = ((self.x1 + self.x2) / 2) / img_w
        cy = ((self.y1 + self.y2) / 2) / img_h
        bw = (self.x2 - self.x1) / img_w
        bh = (self.y2 - self.y1) / img_h
        return f"{self.cls_id} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}"

    @staticmethod
    def from_yolo(line: str, img_w: int, img_h: int) -> "BBox":
        parts  = line.strip().split()
        cid    = int(parts[0])
        cx, cy = float(parts[1]), float(parts[2])
        bw, bh = float(parts[3]), float(parts[4])
        x1     = int((cx - bw / 2) * img_w)
        y1     = int((cy - bh / 2) * img_h)
        x2     = int((cx + bw / 2) * img_w)
        y2     = int((cy + bh / 2) * img_h)
        return BBox(x1, y1, x2, y2, cid)


# ─────────────────────────────────────────────────────────────────
# 标注画布
# ─────────────────────────────────────────────────────────────────
class AnnotationCanvas(QLabel):
    """
    支持鼠标拖拽绘制标注框的画布。
    图像自动等比缩放至控件大小，坐标自动换算。
    新增：键盘方向键微调选中框（Shift加速）。
    """
    boxes_changed = pyqtSignal()

    def __init__(self, classes: list, parent=None):
        super().__init__(parent)
        self.classes        = classes
        self.current_cls_id = 0
        self.boxes: list[BBox] = []
        self.selected_idx   = -1

        self._src_img: np.ndarray = None   # 原始 BGR 图像
        self._img_w   = 1
        self._img_h   = 1
        self._scale   = 1.0
        self._offset_x = 0
        self._offset_y = 0

        self._drawing  = False
        self._drag_start: QPoint = None
        self._drag_end:   QPoint = None

        self.setAlignment(Qt.AlignCenter)
        self.setMinimumSize(400, 300)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setStyleSheet("background:#1e1e1e; border-radius:6px;")
        self.setCursor(QCursor(Qt.CrossCursor))
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.StrongFocus)  # 确保画布能捕获键盘事件

    # ── 公共接口 ──────────────────────────────────────────────────
    def load_image(self, bgr: np.ndarray):
        self._src_img = bgr
        self._img_h, self._img_w = bgr.shape[:2]
        self.boxes        = []
        self.selected_idx = -1
        self._recalc_transform()
        self._redraw()

    def load_labels(self, label_path: str):
        self.boxes = []
        if label_path and os.path.exists(label_path):
            with open(label_path, "r") as f:
                for line in f:
                    if line.strip():
                        box = BBox.from_yolo(line, self._img_w, self._img_h)
                        self.boxes.append(box)
        self._redraw()
        self.boxes_changed.emit()

    def save_labels(self, label_path: str):
        os.makedirs(os.path.dirname(label_path), exist_ok=True)
        with open(label_path, "w") as f:
            for b in self.boxes:
                if b.valid():
                    f.write(b.to_yolo(self._img_w, self._img_h) + "\n")

    def delete_selected(self):
        if 0 <= self.selected_idx < len(self.boxes):
            self.boxes.pop(self.selected_idx)
            self.selected_idx = -1
            self._redraw()
            self.boxes_changed.emit()

    def clear_boxes(self):
        self.boxes.clear()
        self.selected_idx = -1
        self._redraw()
        self.boxes_changed.emit()

    def set_class(self, cls_id: int):
        self.current_cls_id = cls_id
        # 改变选中框的类别
        if 0 <= self.selected_idx < len(self.boxes):
            self.boxes[self.selected_idx].cls_id = cls_id
            self._redraw()
            self.boxes_changed.emit()

    # ── 鼠标事件 ─────────────────────────────────────────────────
    def mousePressEvent(self, event):
        if self._src_img is None:
            return
        if event.button() == Qt.LeftButton:
            self._drawing    = True
            self._drag_start = event.pos()
            self._drag_end   = event.pos()
            # 点击已有框 → 选中
            ix, iy = self._widget_to_img(event.pos())
            hit = self._hit_test(ix, iy)
            if hit >= 0:
                self._drawing     = False
                self.selected_idx = hit
                self._redraw()
        elif event.button() == Qt.RightButton:
            self.selected_idx = -1
            self._redraw()

    def mouseMoveEvent(self, event):
        if self._drawing:
            self._drag_end = event.pos()
            self._redraw()

    def mouseReleaseEvent(self, event):
        if self._drawing and event.button() == Qt.LeftButton:
            self._drawing = False
            p1 = self._widget_to_img(self._drag_start)
            p2 = self._widget_to_img(event.pos())
            box = BBox(p1[0], p1[1], p2[0], p2[1], self.current_cls_id)
            if box.valid():
                self.boxes.append(box)
                self.selected_idx = len(self.boxes) - 1
                self.boxes_changed.emit()
            self._drag_start = self._drag_end = None
            self._redraw()

    def resizeEvent(self, event):
        self._recalc_transform()
        self._redraw()
        super().resizeEvent(event)

    # ── 键盘事件：方向键微调选中框 ──────────────────────────────
    def keyPressEvent(self, event):
        """方向键微调选中框（Shift+方向键=10px，单独按=1px）"""
        if 0 <= self.selected_idx < len(self.boxes):
            b = self.boxes[self.selected_idx]
            step = 10 if event.modifiers() & Qt.ShiftModifier else 1
            moved = False
            
            if event.key() == Qt.Key_Up:
                b.y1 -= step; b.y2 -= step; moved = True
            elif event.key() == Qt.Key_Down:
                b.y1 += step; b.y2 += step; moved = True
            elif event.key() == Qt.Key_Left:
                b.x1 -= step; b.x2 -= step; moved = True
            elif event.key() == Qt.Key_Right:
                b.x1 += step; b.x2 += step; moved = True
            
            if moved:
                # 边界保护
                b.x1 = max(0, min(b.x1, self._img_w - 1))
                b.x2 = max(1, min(b.x2, self._img_w))
                b.y1 = max(0, min(b.y1, self._img_h - 1))
                b.y2 = max(1, min(b.y2, self._img_h))
                # 确保 x1<x2, y1<<y2
                if b.x1 >= b.x2: b.x1 = b.x2 - 1
                if b.y1 >= b.y2: b.y1 = b.y2 - 1
                self._redraw()
                self.boxes_changed.emit()
                return
        super().keyPressEvent(event)

    # ── 绘制 ─────────────────────────────────────────────────────
    def _redraw(self):
        if self._src_img is None:
            self.clear()
            self.setText("← 请从左侧选择图像")
            self.setStyleSheet("background:#1e1e1e; color:#666; border-radius:6px;"
                               "font-size:15px;")
            return

        # 将 BGR 转 QPixmap
        rgb  = cv2.cvtColor(self._src_img, cv2.COLOR_BGR2RGB)
        h, w = rgb.shape[:2]
        qi   = QImage(rgb.data, w, h, 3 * w, QImage.Format_RGB888)
        base = QPixmap.fromImage(qi).scaled(
            self.width(), self.height(),
            Qt.KeepAspectRatio, Qt.SmoothTransformation
        )
        self._recalc_transform()

        canvas = QPixmap(self.width(), self.height())
        canvas.fill(QColor("#1e1e1e"))
        painter = QPainter(canvas)
        painter.drawPixmap(self._offset_x, self._offset_y, base)

        # 画已有框
        for idx, box in enumerate(self.boxes):
            self._draw_box(painter, box, idx == self.selected_idx)

        # 画正在绘制的框
        if self._drawing and self._drag_start and self._drag_end:
            r = QRect(self._drag_start, self._drag_end).normalized()
            color = self._cls_color(self.current_cls_id, alpha=120)
            painter.setPen(QPen(QColor(*self._cls_rgb(self.current_cls_id)), 2, Qt.DashLine))
            painter.setBrush(QBrush(color))
            painter.drawRect(r)

        painter.end()
        self.setPixmap(canvas)

    def _draw_box(self, painter: QPainter, box: BBox, selected: bool):
        # 图像坐标 → 控件坐标
        wx1 = int(box.x1 * self._scale) + self._offset_x
        wy1 = int(box.y1 * self._scale) + self._offset_y
        wx2 = int(box.x2 * self._scale) + self._offset_x
        wy2 = int(box.y2 * self._scale) + self._offset_y
        r   = QRect(QPoint(wx1, wy1), QPoint(wx2, wy2))

        rgb   = self._cls_rgb(box.cls_id)
        color = QColor(*rgb)
        if selected:
            painter.setPen(QPen(color, 3, Qt.SolidLine))
            painter.setBrush(QBrush(QColor(*rgb, 50)))
        else:
            painter.setPen(QPen(color, 2, Qt.SolidLine))
            painter.setBrush(QBrush(QColor(*rgb, 25)))
        painter.drawRect(r)

        # 类别标签
        cls_name = self.classes[box.cls_id] if box.cls_id < len(self.classes) else f"cls{box.cls_id}"
        label    = f"{'[✓] ' if selected else ''}{cls_name}"
        fm       = painter.fontMetrics()
        lw       = fm.horizontalAdvance(label) + 8
        lh       = fm.height() + 4
        lx       = wx1
        ly       = max(wy1 - lh, 0)
        painter.fillRect(lx, ly, lw, lh, color)
        painter.setPen(QPen(Qt.white))
        painter.drawText(lx + 4, ly + lh - 4, label)

    # ── 坐标换算 ──────────────────────────────────────────────────
    def _recalc_transform(self):
        if self._src_img is None:
            return
        cw, ch = self.width(), self.height()
        sx = cw / self._img_w
        sy = ch / self._img_h
        self._scale    = min(sx, sy)
        disp_w         = int(self._img_w * self._scale)
        disp_h         = int(self._img_h * self._scale)
        self._offset_x = (cw - disp_w) // 2
        self._offset_y = (ch - disp_h) // 2

    def _widget_to_img(self, pt: QPoint):
        ix = int((pt.x() - self._offset_x) / self._scale)
        iy = int((pt.y() - self._offset_y) / self._scale)
        ix = max(0, min(ix, self._img_w - 1))
        iy = max(0, min(iy, self._img_h - 1))
        return ix, iy

    def _hit_test(self, ix: int, iy: int) -> int:
        for i, b in enumerate(self.boxes):
            if b.x1 <= ix <= b.x2 and b.y1 <= iy <= b.y2:
                return i
        return -1

    # ── 颜色工具 ──────────────────────────────────────────────────
    def _cls_rgb(self, cls_id: int):
        colors = cfg.BOX_COLORS
        return colors[cls_id % len(colors)]

    def _cls_color(self, cls_id: int, alpha: int = 255) -> QColor:
        r, g, b = self._cls_rgb(cls_id)
        return QColor(r, g, b, alpha)


# ─────────────────────────────────────────────────────────────────
# 标注界面主 Widget
# ─────────────────────────────────────────────────────────────────
class AnnotateWidget(QWidget):
    """图像标注选项卡"""

    def __init__(self, dataset_manager: DatasetManager, parent=None):
        super().__init__(parent)
        self.dm          = dataset_manager
        # 从持久化设置加载类别，没有则用默认值
        self.classes     = list(settings.get("classes", cfg.DEFAULT_CLASSES))
        self._task_path  = ""
        self._records    = []
        self._cur_rec    = None

        self._build_ui()

    # ─────────────────────────────────────────────────────────────
    # UI 构建
    # ─────────────────────────────────────────────────────────────
    def _build_ui(self):
        root = QHBoxLayout(self)
        root.setSpacing(0)
        root.setContentsMargins(0, 0, 0, 0)
        splitter = QSplitter(Qt.Horizontal)
        root.addWidget(splitter)

        # ── 左侧：任务选择 + 图像列表 ──
        left_widget = QWidget()
        left_lay    = QVBoxLayout(left_widget)
        left_lay.setContentsMargins(8, 8, 4, 8)

        task_row = QHBoxLayout()
        self.task_combo = QComboBox()
        self.task_combo.currentTextChanged.connect(self._on_task_select)
        task_row.addWidget(self.task_combo)
        self.refresh_btn = QPushButton("↻")
        self.refresh_btn.setFixedWidth(32)
        self.refresh_btn.clicked.connect(self.refresh_tasks)
        task_row.addWidget(self.refresh_btn)
        left_lay.addLayout(task_row)

        self.img_list = QListWidget()
        self.img_list.setSelectionMode(QAbstractItemView.SingleSelection)
        self.img_list.currentRowChanged.connect(self._on_img_select)
        left_lay.addWidget(self.img_list)

        # 快速操作按钮
        prev_next = QHBoxLayout()
        self.prev_btn = QPushButton("◀ 上一张")
        self.prev_btn.clicked.connect(lambda: self._nav(-1))
        self.next_btn = QPushButton("下一张 ▶")
        self.next_btn.clicked.connect(lambda: self._nav(1))
        prev_next.addWidget(self.prev_btn)
        prev_next.addWidget(self.next_btn)
        left_lay.addLayout(prev_next)

        left_widget.setMinimumWidth(200)
        left_widget.setMaximumWidth(260)
        splitter.addWidget(left_widget)

        # ── 中央：标注画布 ──
        mid_widget = QWidget()
        mid_lay    = QVBoxLayout(mid_widget)
        mid_lay.setContentsMargins(4, 8, 4, 8)

        # 快捷键提示（更新：增加方向键微调）
        hint = QLabel(
            "左键拖拽：画框  |  点击已有框：选中  |  右键：取消  |  "
            "Del：删除  |  S：保存  |  方向键：微调框  |  Shift+方向键：快速移动"
        )
        hint.setAlignment(Qt.AlignCenter)
        hint.setStyleSheet("color:#888; font-size:11px; padding:2px;")
        mid_lay.addWidget(hint)

        self.canvas = AnnotationCanvas(self.classes)
        self.canvas.boxes_changed.connect(self._on_boxes_changed)
        mid_lay.addWidget(self.canvas)

        # 自动保存
        self.auto_save_cb = QCheckBox("✓ 自动保存标注（修改后自动写入文件）")
        self.auto_save_cb.setStyleSheet("color:#aaa;")
        mid_lay.addWidget(self.auto_save_cb)

        # 保存按钮行
        save_row = QHBoxLayout()
        self.save_btn = QPushButton("💾 保存标注（S）")
        self.save_btn.setFixedHeight(38)
        self.save_btn.setEnabled(False)
        self.save_btn.clicked.connect(self._save_annotation)
        self.save_btn.setStyleSheet(
            "QPushButton{background:#1565c0;color:#fff;border-radius:4px;font-size:13px;}"
            "QPushButton:hover{background:#1e88e5;}"
            "QPushButton:disabled{background:#333;color:#666;}"
        )
        save_row.addWidget(self.save_btn)

        self.clear_btn = QPushButton("🗑 清空所有框")
        self.clear_btn.setFixedHeight(38)
        self.clear_btn.clicked.connect(self._on_clear)
        self.clear_btn.setStyleSheet(
            "QPushButton{background:#444;color:#fff;border-radius:4px;}"
            "QPushButton:hover{background:#666;}"
        )
        save_row.addWidget(self.clear_btn)
        mid_lay.addLayout(save_row)

        self.ann_status = QLabel("")
        self.ann_status.setAlignment(Qt.AlignCenter)
        self.ann_status.setStyleSheet("color:#4fc3f7; font-size:12px;")
        mid_lay.addWidget(self.ann_status)
        splitter.addWidget(mid_widget)

        # ── 右侧：类别 + 框列表 ──
        right_widget = QWidget()
        right_lay    = QVBoxLayout(right_widget)
        right_lay.setContentsMargins(4, 8, 8, 8)

        # 类别管理
        cls_box = QGroupBox("当前类别")
        cls_lay = QVBoxLayout(cls_box)
        self.cls_combo = QComboBox()
        self.cls_combo.currentIndexChanged.connect(self._on_cls_change)
        cls_lay.addWidget(self.cls_combo)

        add_row = QHBoxLayout()
        self.cls_input = QLineEdit()
        self.cls_input.setPlaceholderText("新类别名称...")
        add_row.addWidget(self.cls_input)
        add_cls_btn = QPushButton("＋")
        add_cls_btn.setFixedWidth(32)
        add_cls_btn.clicked.connect(self._add_class)
        add_row.addWidget(add_cls_btn)
        cls_lay.addLayout(add_row)

        del_cls_btn = QPushButton("删除当前类别")
        del_cls_btn.clicked.connect(self._del_class)
        cls_lay.addWidget(del_cls_btn)
        right_lay.addWidget(cls_box)

        # 框列表
        box_box = QGroupBox("标注框列表")
        box_lay = QVBoxLayout(box_box)
        self.box_list = QListWidget()
        self.box_list.currentRowChanged.connect(self._on_boxlist_select)
        box_lay.addWidget(self.box_list)
        del_box_btn = QPushButton("删除选中框（Del）")
        del_box_btn.clicked.connect(self._del_selected_box)
        box_lay.addWidget(del_box_btn)
        right_lay.addWidget(box_box)

        right_lay.addStretch()
        right_widget.setMinimumWidth(180)
        right_widget.setMaximumWidth(240)
        splitter.addWidget(right_widget)

        splitter.setSizes([220, 800, 200])
        self._refresh_cls_combo()

    # ─────────────────────────────────────────────────────────────
    # 任务 / 图像切换
    # ─────────────────────────────────────────────────────────────
    def refresh_tasks(self):
        cur = self.task_combo.currentText()
        self.task_combo.blockSignals(True)
        self.task_combo.clear()
        for t in self.dm.get_tasks():
            self.task_combo.addItem(t)
        idx = self.task_combo.findText(cur)
        if idx >= 0:
            self.task_combo.setCurrentIndex(idx)
        self.task_combo.blockSignals(False)
        self._on_task_select(self.task_combo.currentText())

    def _on_task_select(self, name: str):
        if not name:
            return
        self._task_path = self.dm.get_task_path(name)
        self._load_image_list()

    def _load_image_list(self):
        self.img_list.clear()
        self._records = self.dm.get_images(self._task_path)
        for rec in self._records:
            status = "✅" if rec.get("annotated") else "○"
            item   = QListWidgetItem(f"{status}  {rec['filename']}")
            item.setData(Qt.UserRole, rec["id"])
            if rec.get("annotated"):
                item.setForeground(QColor("#81c784"))
            self.img_list.addItem(item)

    def _on_img_select(self, row: int):
        if row < 0 or row >= len(self._records):
            return
        self._cur_rec = self._records[row]
        img_path = self._cur_rec.get("color_path", "")
        
        # 增强诊断：显示具体路径和目录状态
        if not img_path:
            self.ann_status.setText("⚠ 图像路径为空（内部错误）")
            return
        if not os.path.exists(img_path):
            dir_path = os.path.dirname(img_path)
            hint = ""
            if not os.path.exists(dir_path):
                hint = f"（目录 {dir_path} 也不存在）"
            self.ann_status.setText(f"⚠ 图像文件不存在 {hint}\n路径：{img_path}")
            logger.warning(f"图像文件不存在：{img_path}，目录存在={os.path.exists(dir_path)}")
            return
        
        bgr = cv2.imread(img_path)
        if bgr is None:
            self.ann_status.setText(f"⚠ 图像读取失败（OpenCV无法解码）\n路径：{img_path}")
            return
        
        self.canvas.load_image(bgr)
        self.canvas.load_labels(self._cur_rec.get("label_path", ""))
        self.save_btn.setEnabled(True)
        self._refresh_box_list()
        self._update_ann_status()

    def _nav(self, delta: int):
        cur = self.img_list.currentRow()
        nxt = cur + delta
        if 0 <= nxt < self.img_list.count():
            self.img_list.setCurrentRow(nxt)

    # ─────────────────────────────────────────────────────────────
    # 标注保存
    # ─────────────────────────────────────────────────────────────
    def _save_annotation(self):
        if not self._cur_rec or not self._task_path:
            return
        lbl_dir  = os.path.join(self._task_path, "labels")
        lbl_name = os.path.splitext(self._cur_rec["filename"])[0] + ".txt"
        lbl_path = os.path.join(lbl_dir, lbl_name)
        self.canvas.save_labels(lbl_path)
        self.dm.mark_annotated(self._task_path, self._cur_rec["id"], lbl_path)
        self._cur_rec["label_path"] = lbl_path
        self._cur_rec["annotated"]  = True
        # 更新列表显示
        row  = self.img_list.currentRow()
        item = self.img_list.item(row)
        if item:
            item.setText(f"✅  {self._cur_rec['filename']}")
            item.setForeground(QColor("#81c784"))
        self._update_ann_status(saved=True)

    def _on_clear(self):
        if not self._cur_rec:
            return
        reply = QMessageBox.question(
            self, "确认", "确定清空当前图像的所有标注框？",
            QMessageBox.Yes | QMessageBox.No
        )
        if reply == QMessageBox.Yes:
            self.canvas.clear_boxes()

    # ─────────────────────────────────────────────────────────────
    # 类别管理
    # ─────────────────────────────────────────────────────────────
    def _refresh_cls_combo(self):
        cur_idx = self.cls_combo.currentIndex()
        self.cls_combo.blockSignals(True)
        self.cls_combo.clear()
        for i, c in enumerate(self.classes):
            self.cls_combo.addItem(f"[{i}] {c}")
        self.cls_combo.setCurrentIndex(max(0, cur_idx))
        self.cls_combo.blockSignals(False)
        self.canvas.classes = self.classes

    def _on_cls_change(self, idx: int):
        self.canvas.set_class(idx)
        self._refresh_box_list()

    def _add_class(self):
        name = self.cls_input.text().strip()
        if not name:
            return
        if name in self.classes:
            QMessageBox.warning(self, "提示", "类别已存在")
            return
        self.classes.append(name)
        self.cls_input.clear()
        self._refresh_cls_combo()
        settings.set_and_save("classes", self.classes)

    def _del_class(self):
        idx = self.cls_combo.currentIndex()
        if len(self.classes) <= 1:
            QMessageBox.warning(self, "提示", "至少保留一个类别")
            return
        cls_name = self.classes[idx]
        reply = QMessageBox.question(
            self, "确认",
            f"确定删除类别 [{idx}] {cls_name}？\n（已有该类别的标注框不受影响）",
            QMessageBox.Yes | QMessageBox.No,
        )
        if reply == QMessageBox.Yes:
            self.classes.pop(idx)
            self._refresh_cls_combo()
            settings.set_and_save("classes", self.classes)

    # ─────────────────────────────────────────────────────────────
    # 框列表
    # ─────────────────────────────────────────────────────────────
    def _on_boxes_changed(self):
        self._refresh_box_list()
        # 自动保存逻辑
        if self.auto_save_cb.isChecked() and self._cur_rec and self._task_path:
            self._save_annotation()

    def _refresh_box_list(self):
        self.box_list.blockSignals(True)
        self.box_list.clear()
        for i, b in enumerate(self.canvas.boxes):
            cls_name = self.classes[b.cls_id] if b.cls_id < len(self.classes) else f"cls{b.cls_id}"
            w = b.x2 - b.x1
            h = b.y2 - b.y1
            self.box_list.addItem(f"#{i+1}  {cls_name}  ({w}×{h})")
        sel = self.canvas.selected_idx
        if 0 <= sel < self.box_list.count():
            self.box_list.setCurrentRow(sel)
        self.box_list.blockSignals(False)

    def _on_boxlist_select(self, row: int):
        self.canvas.selected_idx = row
        if 0 <= row < len(self.canvas.boxes):
            self.cls_combo.setCurrentIndex(self.canvas.boxes[row].cls_id)
        self.canvas._redraw()

    def _del_selected_box(self):
        self.canvas.delete_selected()
        self._refresh_box_list()

    # ─────────────────────────────────────────────────────────────
    # 状态提示
    # ─────────────────────────────────────────────────────────────
    def _update_ann_status(self, saved: bool = False):
        n = len(self.canvas.boxes)
        if saved:
            self.ann_status.setText(f"✅ 已保存 {n} 个标注框")
        else:
            self.ann_status.setText(f"共 {n} 个标注框（未保存）")

    # ─────────────────────────────────────────────────────────────
    # 键盘快捷键
    # ─────────────────────────────────────────────────────────────
    def keyPressEvent(self, event):
        # 若画布有选中框，方向键优先微调框位置，不切换图片
        if event.key() in (Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down):
            if 0 <= self.canvas.selected_idx < len(self.canvas.boxes):
                self.canvas.keyPressEvent(event)
                return
        
        if event.key() == Qt.Key_Delete:
            self._del_selected_box()
        elif event.key() == Qt.Key_S:
            self._save_annotation()
        elif event.key() == Qt.Key_Left:
            self._nav(-1)
        elif event.key() == Qt.Key_Right:
            self._nav(1)
        else:
            super().keyPressEvent(event)

    # 允许接收从外部传入新保存图片的通知
    def on_image_saved(self, task_path: str, _rec):
        if task_path == self._task_path:
            self._load_image_list()
        else:
            self.refresh_tasks()