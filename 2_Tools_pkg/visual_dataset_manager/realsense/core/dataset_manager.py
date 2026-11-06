# core/dataset_manager.py
# 数据集管理器：
#   - 按任务/场景建立文件夹（images / depth / labels）
#   - 保存彩色图与深度图
#   - 维护 index.json 索引（记录每张图的元信息与标注状态）
#   - 提供查询、删除、统计等接口

import os
import re
import json
import shutil
import cv2
import numpy as np
from datetime import datetime
from typing import List, Dict, Optional

from core.logger import get_logger

logger = get_logger(__name__)


class DatasetManager:
    """数据集文件管理器"""

    INDEX_FILE = "index.json"

    # 每个任务的子文件夹名称
    DIR_IMAGES = "images"
    DIR_DEPTH  = "depth"
    DIR_LABELS = "labels"

    def __init__(self, base_dir: str = "datasets"):
        self.base_dir = base_dir
        os.makedirs(base_dir, exist_ok=True)

    # ═══════════════════════════════════════════════════════════════
    # 任务管理
    # ═══════════════════════════════════════════════════════════════
    def create_task(self, task_name: str) -> str:
        """
        创建任务文件夹（幂等：已存在则直接返回路径）。
        Returns:
            任务根路径（绝对路径）
        """
        task_name = self._sanitize(task_name)
        task_path = os.path.join(self.base_dir, task_name)
        for sub in [self.DIR_IMAGES, self.DIR_DEPTH, self.DIR_LABELS]:
            os.makedirs(os.path.join(task_path, sub), exist_ok=True)

        idx_path = os.path.join(task_path, self.INDEX_FILE)
        if not os.path.exists(idx_path):
            self._save_index(task_path, {
                "task":       task_name,
                "created_at": datetime.now().isoformat(),
                "images":     [],
            })
        return os.path.abspath(task_path)

    def get_tasks(self) -> List[str]:
        """返回所有有效任务名称（按字母排序）"""
        tasks = []
        if not os.path.isdir(self.base_dir):
            return tasks
        for name in sorted(os.listdir(self.base_dir)):
            p = os.path.join(self.base_dir, name)
            if os.path.isdir(p) and os.path.exists(os.path.join(p, self.INDEX_FILE)):
                tasks.append(name)
        return tasks

    def delete_task(self, task_name: str) -> bool:
        """删除整个任务文件夹"""
        task_path = os.path.join(self.base_dir, self._sanitize(task_name))
        if os.path.isdir(task_path):
            shutil.rmtree(task_path)
            return True
        return False

    def get_task_path(self, task_name: str) -> str:
        return os.path.abspath(os.path.join(self.base_dir, task_name))

    # ═══════════════════════════════════════════════════════════════
    # 图像保存 & 读取
    # ═══════════════════════════════════════════════════════════════
    def save_image(
        self,
        task_path: str,
        color_frame: np.ndarray,
        depth_frame: Optional[np.ndarray] = None,
    ) -> Dict:
        """
        保存一帧到任务文件夹，更新索引，返回图像记录 dict。
        color_frame: HxWx3 uint8 BGR
        depth_frame: HxW uint16（可选）
        """
        ts       = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        img_name = f"img_{ts}.jpg"
        dep_name = f"dep_{ts}.png"

        # ── 保存彩色图（带写入验证）─────────────────────────────
        img_dir = os.path.join(task_path, self.DIR_IMAGES)
        os.makedirs(img_dir, exist_ok=True)
        img_path = os.path.join(img_dir, img_name)

        ok = cv2.imwrite(img_path, color_frame, [cv2.IMWRITE_JPEG_QUALITY, 95])
        if not ok:
            raise RuntimeError(
                f"彩色图保存失败：{img_path}。请检查目录权限、磁盘空间，"
                f"或路径是否包含非法字符。"
            )

        # ── 保存深度图（16-bit PNG 保留原始 mm 值）───────────────
        dep_path = None
        if depth_frame is not None:
            dep_dir = os.path.join(task_path, self.DIR_DEPTH)
            os.makedirs(dep_dir, exist_ok=True)
            dep_path = os.path.join(dep_dir, dep_name)
            ok = cv2.imwrite(dep_path, depth_frame.astype(np.uint16))
            if not ok:
                # 原子性：深度图失败时回删已保存的彩色图
                if os.path.exists(img_path):
                    os.remove(img_path)
                raise RuntimeError(f"深度图保存失败：{dep_path}")

        # index.json 只存相对路径（相对于 task_path），
        # 避免换目录/换电脑后绝对路径失效。
        record: Dict = {
            "id":          ts,
            "filename":    img_name,
            "color_path":  os.path.join(self.DIR_IMAGES, img_name),
            "depth_path":  os.path.join(self.DIR_DEPTH, dep_name) if dep_path else None,
            "label_path":  None,
            "annotated":   False,
            "saved_at":    datetime.now().isoformat(),
            "width":       int(color_frame.shape[1]),
            "height":      int(color_frame.shape[0]),
        }

        idx = self._load_index(task_path)
        idx["images"].append(record)
        self._save_index(task_path, idx)

        logger.info(f"图像已保存：{img_name} → {img_path}")
        # 返回给上层时补全绝对路径，方便立即使用
        return self._resolve_paths(record, task_path)

    def get_images(self, task_path: str) -> List[Dict]:
        """返回任务中所有图像记录列表（按保存时间正序），路径已解析为绝对路径"""
        raw = self._load_index(task_path).get("images", [])
        return [self._resolve_paths(r, task_path) for r in raw]

    def get_image_by_id(self, task_path: str, image_id: str) -> Optional[Dict]:
        for rec in self.get_images(task_path):
            if rec["id"] == image_id:
                return rec
        return None

    # ═══════════════════════════════════════════════════════════════
    # 删除
    # ═══════════════════════════════════════════════════════════════
    def delete_image(self, task_path: str, image_id: str) -> bool:
        """删除图像文件、对应深度图、标注文件，并从索引中移除"""
        idx    = self._load_index(task_path)
        images = idx.get("images", [])
        target = next((r for r in images if r["id"] == image_id), None)
        if not target:
            return False

        # 解析路径后再删文件
        resolved = self._resolve_paths(target, task_path)
        for key in ("color_path", "depth_path", "label_path"):
            p = resolved.get(key)
            if p and os.path.exists(p):
                os.remove(p)

        idx["images"] = [r for r in images if r["id"] != image_id]
        self._save_index(task_path, idx)
        return True

    # ═══════════════════════════════════════════════════════════════
    # 标注状态更新
    # ═══════════════════════════════════════════════════════════════
    def mark_annotated(self, task_path: str, image_id: str, label_path: str):
        """将指定图像标记为已标注，记录标注文件路径（存相对路径）"""
        idx = self._load_index(task_path)
        # 若传入绝对路径则转换为相对路径再存储
        try:
            rel = os.path.relpath(label_path, task_path)
        except ValueError:
            rel = label_path
        for rec in idx["images"]:
            if rec["id"] == image_id:
                rec["annotated"]  = True
                rec["label_path"] = rel
                break
        self._save_index(task_path, idx)

    def mark_unannotated(self, task_path: str, image_id: str):
        """清除标注状态（删除标注文件并更新索引）"""
        idx = self._load_index(task_path)
        for rec in idx["images"]:
            if rec["id"] == image_id:
                lp = rec.get("label_path")
                if lp:
                    # 修复：确保使用绝对路径删除文件
                    abs_lp = os.path.join(task_path, lp) if not os.path.isabs(lp) else lp
                    abs_lp = os.path.normpath(abs_lp)
                    if os.path.exists(abs_lp):
                        os.remove(abs_lp)
                        logger.info(f"已删除标注文件：{abs_lp}")
                rec["annotated"]  = False
                rec["label_path"] = None
                break
        self._save_index(task_path, idx)

    # ═══════════════════════════════════════════════════════════════
    # 统计
    # ═══════════════════════════════════════════════════════════════
    def get_stats(self, task_name: str) -> Dict:
        """返回任务的图像数量统计"""
        task_path = self.get_task_path(task_name)
        images    = self.get_images(task_path)
        annotated = sum(1 for r in images if r.get("annotated"))
        return {
            "total":       len(images),
            "annotated":   annotated,
            "unannotated": len(images) - annotated,
        }

    # ═══════════════════════════════════════════════════════════════
    # 内部工具
    # ═══════════════════════════════════════════════════════════════
    def _load_index(self, task_path: str) -> Dict:
        idx_path = os.path.join(task_path, self.INDEX_FILE)
        if os.path.exists(idx_path):
            with open(idx_path, "r", encoding="utf-8") as f:
                return json.load(f)
        return {"images": []}

    def _resolve_paths(self, record: Dict, task_path: str) -> Dict:
        """将 record 中的相对路径展开为绝对路径，返回副本（不修改原 record）"""
        rec = dict(record)
        for key in ("color_path", "depth_path", "label_path"):
            val = rec.get(key)
            if val and not os.path.isabs(val):
                abs_path = os.path.normpath(os.path.join(task_path, val))
                rec[key] = abs_path
        return rec

    def _save_index(self, task_path: str, data: Dict):
        idx_path = os.path.join(task_path, self.INDEX_FILE)
        with open(idx_path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)

    @staticmethod
    def _sanitize(name: str) -> str:
        """移除文件名非法字符，防止路径穿越"""
        name = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", name)
        name = name.strip(". ")
        return name or "unnamed_task"
