# tests/test_exporter.py
# 测试 Exporter 的 YOLO / VOC 导出功能
# 运行：python -m pytest tests/ -v

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import pytest
import numpy as np
import cv2
from core.dataset_manager import DatasetManager
from core.exporter import Exporter

CLASSES = ["cat", "dog", "bird"]


@pytest.fixture
def workspace(tmp_path):
    """准备含 3 张图（2张已标注、1张未标注）的任务"""
    dm   = DatasetManager(base_dir=str(tmp_path / "ds"))
    path = dm.create_task("export_test")

    frames = [
        np.zeros((480, 640, 3), dtype=np.uint8),
        np.ones( (480, 640, 3), dtype=np.uint8) * 128,
        np.ones( (480, 640, 3), dtype=np.uint8) * 200,
    ]
    records = [dm.save_image(path, f) for f in frames]

    # 给前两张写标注（YOLO 格式）
    lbl_dir = os.path.join(path, "labels")
    for i, rec in enumerate(records[:2]):
        lbl = os.path.join(lbl_dir, os.path.splitext(rec["filename"])[0] + ".txt")
        with open(lbl, "w") as f:
            f.write(f"{i} 0.500000 0.500000 0.300000 0.300000\n")
        dm.mark_annotated(path, rec["id"], lbl)

    return dm, path, records


# ── YOLO 导出 ─────────────────────────────────────────────────────
class TestYoloExport:
    def test_output_dirs_created(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        Exporter().export_yolo(task_path, dm.get_images(task_path), CLASSES, out)
        assert os.path.isdir(os.path.join(out, "images", "train"))
        assert os.path.isdir(os.path.join(out, "images", "val"))
        assert os.path.isdir(os.path.join(out, "labels", "train"))
        assert os.path.isdir(os.path.join(out, "labels", "val"))

    def test_classes_txt(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        Exporter().export_yolo(task_path, dm.get_images(task_path), CLASSES, out)
        with open(os.path.join(out, "classes.txt")) as f:
            lines = f.read().strip().split("\n")
        assert lines == CLASSES

    def test_dataset_yaml_exists(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        Exporter().export_yolo(task_path, dm.get_images(task_path), CLASSES, out)
        assert os.path.exists(os.path.join(out, "dataset.yaml"))

    def test_yaml_contains_nc(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        Exporter().export_yolo(task_path, dm.get_images(task_path), CLASSES, out)
        with open(os.path.join(out, "dataset.yaml")) as f:
            content = f.read()
        assert f"nc: {len(CLASSES)}" in content

    def test_annotated_images_exported(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        ok, skip = Exporter().export_yolo(
            task_path, dm.get_images(task_path), CLASSES, out,
            copy_unannotated=False
        )
        assert ok == 2    # 只有 2 张已标注
        assert skip == 1

    def test_unannotated_included_by_default(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        ok, skip = Exporter().export_yolo(
            task_path, dm.get_images(task_path), CLASSES, out,
            copy_unannotated=True
        )
        assert ok == 3    # 全部 3 张

    def test_label_file_content(self, workspace, tmp_path):
        """标注文件内容应与原始 YOLO 格式一致"""
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        Exporter().export_yolo(task_path, dm.get_images(task_path), CLASSES, out)
        # 找 train 或 val 中的标注文件
        for split in ("train", "val"):
            ldir = os.path.join(out, "labels", split)
            for fname in os.listdir(ldir):
                if fname.endswith(".txt"):
                    with open(os.path.join(ldir, fname)) as f:
                        lines = [l.strip() for l in f if l.strip()]
                    for line in lines:
                        parts = line.split()
                        assert len(parts) == 5
                        cls_id = int(parts[0])
                        assert 0 <= cls_id < len(CLASSES)
                        for v in parts[1:]:
                            assert 0.0 <= float(v) <= 1.0

    def test_train_val_split_ratio(self, workspace, tmp_path):
        """train 数量应 >= val 数量（默认 0.8 比例）"""
        dm, task_path, records = workspace
        out = str(tmp_path / "yolo_out")
        Exporter().export_yolo(
            task_path, dm.get_images(task_path), CLASSES, out,
            train_ratio=0.8, copy_unannotated=True
        )
        n_train = len(os.listdir(os.path.join(out, "images", "train")))
        n_val   = len(os.listdir(os.path.join(out, "images", "val")))
        assert n_train >= n_val


# ── VOC 导出 ──────────────────────────────────────────────────────
class TestVocExport:
    def test_output_structure(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "voc_out")
        Exporter().export_voc(task_path, dm.get_images(task_path), CLASSES, out)
        assert os.path.isdir(os.path.join(out, "JPEGImages"))
        assert os.path.isdir(os.path.join(out, "Annotations"))
        assert os.path.isdir(os.path.join(out, "ImageSets", "Main"))

    def test_xml_generated(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "voc_out")
        Exporter().export_voc(
            task_path, dm.get_images(task_path), CLASSES, out,
            copy_unannotated=True
        )
        xmls = os.listdir(os.path.join(out, "Annotations"))
        assert len(xmls) == 3

    def test_xml_has_bndbox(self, workspace, tmp_path):
        """已标注的 XML 应包含 bndbox 标签"""
        dm, task_path, records = workspace
        out = str(tmp_path / "voc_out")
        Exporter().export_voc(task_path, dm.get_images(task_path), CLASSES, out)
        import xml.etree.ElementTree as ET
        ann_dir = os.path.join(out, "Annotations")
        for fname in os.listdir(ann_dir):
            tree = ET.parse(os.path.join(ann_dir, fname))
            root = tree.getroot()
            # 有 object 元素的必须有 bndbox
            for obj in root.findall("object"):
                assert obj.find("bndbox") is not None

    def test_set_files_created(self, workspace, tmp_path):
        dm, task_path, records = workspace
        out = str(tmp_path / "voc_out")
        Exporter().export_voc(task_path, dm.get_images(task_path), CLASSES, out)
        sets_dir = os.path.join(out, "ImageSets", "Main")
        assert os.path.exists(os.path.join(sets_dir, "train.txt"))
        assert os.path.exists(os.path.join(sets_dir, "val.txt"))
        assert os.path.exists(os.path.join(sets_dir, "trainval.txt"))


# ── 坐标还原正确性 ────────────────────────────────────────────────
class TestCoordinateConversion:
    def test_yolo_to_abs_center_box(self):
        """中心全图框 → x1=0, y1=0, x2=W, y2=H"""
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write("0 0.500000 0.500000 1.000000 1.000000\n")
            lbl = f.name
        boxes = Exporter()._yolo_to_abs(lbl, CLASSES, 640, 480)
        os.unlink(lbl)
        assert len(boxes) == 1
        b = boxes[0]
        assert b["x1"] == 0 and b["y1"] == 0
        assert b["x2"] == 640 and b["y2"] == 480
        assert b["class"] == "cat"   # cls_id=0

    def test_yolo_to_abs_empty_file(self):
        boxes = Exporter()._yolo_to_abs(None, CLASSES, 640, 480)
        assert boxes == []
