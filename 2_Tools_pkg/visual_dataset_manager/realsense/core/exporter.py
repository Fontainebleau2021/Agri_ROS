# core/exporter.py
# 数据集导出器：
#   - YOLO 格式（YOLOv5 / YOLOv8 兼容）
#   - PASCAL VOC 格式
#
# 标注文件内部以 YOLO 格式存储（归一化 cx cy w h），
# 导出 VOC 时在此处完成坐标还原。

import os
import shutil
import random
from datetime import datetime
from typing import List, Dict, Optional, Tuple
import xml.etree.ElementTree as ET
from xml.dom import minidom

from core.logger import get_logger

logger = get_logger(__name__)


class Exporter:
    """数据集格式导出器"""

    # ═══════════════════════════════════════════════════════════════
    # YOLO 格式导出
    # ═══════════════════════════════════════════════════════════════
    def export_yolo(
        self,
        task_path: str,
        images: List[Dict],
        classes: List[str],
        output_dir: str,
        copy_unannotated: bool = True,
        train_ratio: float = 0.8,
        seed: int = 42,
    ) -> Tuple[int, int]:
        """
        导出为 YOLO 格式，并按比例划分 train / val 子集。
        输出目录结构：
            output_dir/
              images/train/    images/val/
              labels/train/    labels/val/
              classes.txt
              dataset.yaml     ← train/val 路径已填好

        Args:
            train_ratio      : 训练集比例（0~1），剩余为验证集
            copy_unannotated : True → 未标注图像也复制（labels 为空文件）
            seed             : 随机划分种子，保证可复现
        Returns:
            (success_count, skipped_count)
        """
        # 收集有效记录
        valid = []
        skipped = 0
        for rec in images:
            src_img = rec.get("color_path", "")
            if not src_img or not os.path.exists(src_img):
                skipped += 1
                continue
            if not rec.get("annotated") and not copy_unannotated:
                skipped += 1
                continue
            valid.append(rec)

        # 随机划分
        rng = random.Random(seed)
        rng.shuffle(valid)
        split_idx  = max(1, int(len(valid) * train_ratio))
        train_recs = valid[:split_idx]
        val_recs   = valid[split_idx:]

        # 建目录
        for split in ("train", "val"):
            os.makedirs(os.path.join(output_dir, "images", split), exist_ok=True)
            os.makedirs(os.path.join(output_dir, "labels", split), exist_ok=True)

        # 写文件
        success = 0
        for split, recs in [("train", train_recs), ("val", val_recs)]:
            for rec in recs:
                src_img  = rec["color_path"]
                dst_img  = os.path.join(output_dir, "images", split, rec["filename"])
                shutil.copy2(src_img, dst_img)

                lbl_name = os.path.splitext(rec["filename"])[0] + ".txt"
                dst_lbl  = os.path.join(output_dir, "labels", split, lbl_name)
                src_lbl  = rec.get("label_path", "")
                if src_lbl and os.path.exists(src_lbl):
                    shutil.copy2(src_lbl, dst_lbl)
                else:
                    open(dst_lbl, "w").close()
                success += 1

        # classes.txt
        with open(os.path.join(output_dir, "classes.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(classes))

        # dataset.yaml（带 train/val 路径）
        self._write_yaml(output_dir, classes, train_ratio)

        logger.info(
            f"YOLO 导出完成 → {output_dir}  "
            f"train:{len(train_recs)}  val:{len(val_recs)}  跳过:{skipped}"
        )
        return success, skipped

    # ═══════════════════════════════════════════════════════════════
    # PASCAL VOC 格式导出
    # ═══════════════════════════════════════════════════════════════
    def export_voc(
        self,
        task_path: str,
        images: List[Dict],
        classes: List[str],
        output_dir: str,
        copy_unannotated: bool = True,
        train_ratio: float = 0.8,
        seed: int = 42,
    ) -> Tuple[int, int]:
        """
        导出为 PASCAL VOC 格式，并按比例生成 train.txt / val.txt。
        输出目录结构：
            output_dir/
              JPEGImages/
              Annotations/
              ImageSets/Main/
                train.txt  val.txt  trainval.txt
        """
        img_dir  = os.path.join(output_dir, "JPEGImages")
        ann_dir  = os.path.join(output_dir, "Annotations")
        sets_dir = os.path.join(output_dir, "ImageSets", "Main")
        for d in [img_dir, ann_dir, sets_dir]:
            os.makedirs(d, exist_ok=True)

        valid   = []
        skipped = 0
        for rec in images:
            src_img = rec.get("color_path", "")
            if not src_img or not os.path.exists(src_img):
                skipped += 1
                continue
            if not rec.get("annotated") and not copy_unannotated:
                skipped += 1
                continue
            valid.append(rec)

        # 随机划分
        rng = random.Random(seed)
        rng.shuffle(valid)
        split_idx  = max(1, int(len(valid) * train_ratio))
        train_recs = valid[:split_idx]
        val_recs   = valid[split_idx:]

        success = 0
        all_names = []
        for rec in valid:
            src_img = rec["color_path"]
            base    = os.path.splitext(rec["filename"])[0]
            all_names.append(base)

            shutil.copy2(src_img, os.path.join(img_dir, rec["filename"]))

            w   = rec.get("width",  640)
            h   = rec.get("height", 480)
            boxes   = self._yolo_to_abs(rec.get("label_path", ""), classes, w, h)
            xml_str = self._build_voc_xml(rec["filename"], w, h, boxes)
            with open(os.path.join(ann_dir, base + ".xml"), "w", encoding="utf-8") as f:
                f.write(xml_str)
            success += 1

        # ImageSets 文件
        def _write_set(fname, recs):
            names = [os.path.splitext(r["filename"])[0] for r in recs]
            with open(os.path.join(sets_dir, fname), "w") as f:
                f.write("\n".join(names))

        _write_set("train.txt",    train_recs)
        _write_set("val.txt",      val_recs)
        _write_set("trainval.txt", valid)

        logger.info(
            f"VOC 导出完成 → {output_dir}  "
            f"train:{len(train_recs)}  val:{len(val_recs)}  跳过:{skipped}"
        )
        return success, skipped

    # ═══════════════════════════════════════════════════════════════
    # 内部工具
    # ═══════════════════════════════════════════════════════════════
    def _write_yaml(self, output_dir: str, classes: List[str], train_ratio: float = 0.8):
        """生成 YOLOv5/v8 的 dataset.yaml（含 train/val 路径）"""
        yaml = (
            f"# 由 RealSense 标注工具自动生成\n"
            f"# {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
            f"# train/val 比例：{train_ratio:.0%} / {1-train_ratio:.0%}\n\n"
            f"path: {os.path.abspath(output_dir)}\n"
            f"train: images/train\n"
            f"val:   images/val\n\n"
            f"nc: {len(classes)}\n"
            f"names: {classes}\n"
        )
        with open(os.path.join(output_dir, "dataset.yaml"), "w", encoding="utf-8") as f:
            f.write(yaml)

    def _yolo_to_abs(
        self,
        label_path: Optional[str],
        classes: List[str],
        img_w: int,
        img_h: int,
    ) -> List[Dict]:
        """将 YOLO 归一化坐标转换为像素绝对坐标"""
        boxes = []
        if not label_path or not os.path.exists(label_path):
            return boxes
        with open(label_path, "r") as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) != 5:
                    continue
                cid, cx, cy, bw, bh = (
                    int(parts[0]),
                    float(parts[1]), float(parts[2]),
                    float(parts[3]), float(parts[4]),
                )
                x1 = max(0, int((cx - bw / 2) * img_w))
                y1 = max(0, int((cy - bh / 2) * img_h))
                x2 = min(img_w, int((cx + bw / 2) * img_w))
                y2 = min(img_h, int((cy + bh / 2) * img_h))
                cls_name = classes[cid] if cid < len(classes) else "unknown"
                boxes.append({"class": cls_name, "x1": x1, "y1": y1, "x2": x2, "y2": y2})
        return boxes

    def _build_voc_xml(
        self,
        filename: str,
        width: int,
        height: int,
        boxes: List[Dict],
    ) -> str:
        """构造格式化的 PASCAL VOC XML 字符串"""
        root = ET.Element("annotation")
        ET.SubElement(root, "filename").text = filename
        sz = ET.SubElement(root, "size")
        ET.SubElement(sz, "width").text  = str(width)
        ET.SubElement(sz, "height").text = str(height)
        ET.SubElement(sz, "depth").text  = "3"
        ET.SubElement(root, "segmented").text = "0"

        for b in boxes:
            obj = ET.SubElement(root, "object")
            ET.SubElement(obj, "name").text      = b["class"]
            ET.SubElement(obj, "pose").text      = "Unspecified"
            ET.SubElement(obj, "truncated").text = "0"
            ET.SubElement(obj, "difficult").text = "0"
            bb = ET.SubElement(obj, "bndbox")
            ET.SubElement(bb, "xmin").text = str(b["x1"])
            ET.SubElement(bb, "ymin").text = str(b["y1"])
            ET.SubElement(bb, "xmax").text = str(b["x2"])
            ET.SubElement(bb, "ymax").text = str(b["y2"])

        rough    = ET.tostring(root, encoding="unicode")
        reparsed = minidom.parseString(rough)
        return reparsed.toprettyxml(indent="  ", encoding=None)
