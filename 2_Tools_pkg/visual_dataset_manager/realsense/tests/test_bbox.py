# tests/test_bbox.py
# 测试 BBox 坐标换算（YOLO 归一化 ↔ 像素绝对坐标）
# 运行：python -m pytest tests/ -v

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import pytest
from ui.annotate_widget import BBox


# ── 基本构造 ──────────────────────────────────────────────────────
class TestBBoxConstruct:
    def test_normal_order(self):
        b = BBox(10, 20, 100, 80)
        assert b.x1 == 10 and b.y1 == 20
        assert b.x2 == 100 and b.y2 == 80

    def test_reverse_order_auto_correct(self):
        """传入右下→左上时，应自动交换使 x1<x2, y1<y2"""
        b = BBox(100, 80, 10, 20)
        assert b.x1 == 10 and b.y1 == 20
        assert b.x2 == 100 and b.y2 == 80

    def test_default_cls_id(self):
        b = BBox(0, 0, 10, 10)
        assert b.cls_id == 0

    def test_custom_cls_id(self):
        b = BBox(0, 0, 10, 10, cls_id=3)
        assert b.cls_id == 3


# ── valid 判断 ────────────────────────────────────────────────────
class TestBBoxValid:
    def test_valid_normal(self):
        assert BBox(0, 0, 50, 50).valid() is True

    def test_invalid_too_small_w(self):
        assert BBox(0, 0, 3, 50).valid() is False   # width <= 4

    def test_invalid_too_small_h(self):
        assert BBox(0, 0, 50, 3).valid() is False   # height <= 4

    def test_boundary_exactly_5(self):
        assert BBox(0, 0, 5, 5).valid() is True


# ── to_yolo ───────────────────────────────────────────────────────
class TestBBoxToYolo:
    def test_center_box(self):
        """正中心的框，归一化 cx=cy=0.5"""
        b   = BBox(0, 0, 640, 480)
        out = b.to_yolo(640, 480)
        parts = out.split()
        assert parts[0] == "0"
        assert abs(float(parts[1]) - 0.5) < 1e-4   # cx
        assert abs(float(parts[2]) - 0.5) < 1e-4   # cy
        assert abs(float(parts[3]) - 1.0) < 1e-4   # w
        assert abs(float(parts[4]) - 1.0) < 1e-4   # h

    def test_quarter_box(self):
        """左上角四分之一图像"""
        b   = BBox(0, 0, 320, 240)
        out = b.to_yolo(640, 480)
        parts = out.split()
        assert abs(float(parts[1]) - 0.25) < 1e-4   # cx = 160/640
        assert abs(float(parts[2]) - 0.25) < 1e-4   # cy = 120/480
        assert abs(float(parts[3]) - 0.5)  < 1e-4   # w  = 320/640
        assert abs(float(parts[4]) - 0.5)  < 1e-4   # h  = 240/480

    def test_cls_id_in_output(self):
        b   = BBox(0, 0, 100, 100, cls_id=5)
        out = b.to_yolo(640, 480)
        assert out.startswith("5 ")

    def test_precision(self):
        """输出应包含足够精度（6位小数）"""
        b   = BBox(13, 17, 237, 189)
        out = b.to_yolo(640, 480)
        parts = out.split()
        for v in parts[1:]:
            assert len(v.split(".")[-1]) >= 4


# ── from_yolo（反向解析）─────────────────────────────────────────
class TestBBoxFromYolo:
    def test_roundtrip(self):
        """to_yolo → from_yolo 应能还原原始坐标（允许1px误差）"""
        orig = BBox(50, 30, 400, 300, cls_id=2)
        line = orig.to_yolo(640, 480)
        back = BBox.from_yolo(line, 640, 480)
        assert abs(back.x1 - orig.x1) <= 1
        assert abs(back.y1 - orig.y1) <= 1
        assert abs(back.x2 - orig.x2) <= 1
        assert abs(back.y2 - orig.y2) <= 1
        assert back.cls_id == orig.cls_id

    def test_from_yolo_cls_id(self):
        line = "3 0.500000 0.500000 0.500000 0.500000"
        b    = BBox.from_yolo(line, 640, 480)
        assert b.cls_id == 3

    def test_from_yolo_full_image(self):
        """全图框：cx=cy=0.5, w=h=1.0"""
        line = "0 0.500000 0.500000 1.000000 1.000000"
        b    = BBox.from_yolo(line, 640, 480)
        assert b.x1 == 0 and b.y1 == 0
        assert b.x2 == 640 and b.y2 == 480
