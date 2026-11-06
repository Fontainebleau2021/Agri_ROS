# tests/test_dataset_manager.py
# 测试 DatasetManager 的任务管理、图像索引、删除、统计等功能
# 全部在临时目录运行，不依赖真实相机
# 运行：python -m pytest tests/ -v

import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import pytest
import tempfile
import numpy as np
from core.dataset_manager import DatasetManager


@pytest.fixture
def dm(tmp_path):
    """每个测试用独立临时目录"""
    return DatasetManager(base_dir=str(tmp_path / "datasets"))


@pytest.fixture
def color_frame():
    """640×480 随机 BGR 图像"""
    return np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)


@pytest.fixture
def depth_frame():
    """640×480 随机深度图"""
    return np.random.randint(500, 4000, (480, 640), dtype=np.uint16)


# ── 任务管理 ──────────────────────────────────────────────────────
class TestTaskManagement:
    def test_create_task_returns_path(self, dm):
        path = dm.create_task("scene_01")
        assert os.path.isdir(path)

    def test_create_task_creates_subdirs(self, dm):
        path = dm.create_task("scene_01")
        for sub in ["images", "depth", "labels"]:
            assert os.path.isdir(os.path.join(path, sub))

    def test_create_task_idempotent(self, dm):
        p1 = dm.create_task("scene_01")
        p2 = dm.create_task("scene_01")
        assert p1 == p2

    def test_get_tasks_empty(self, dm):
        assert dm.get_tasks() == []

    def test_get_tasks_after_create(self, dm):
        dm.create_task("a")
        dm.create_task("b")
        tasks = dm.get_tasks()
        assert "a" in tasks
        assert "b" in tasks

    def test_delete_task(self, dm):
        dm.create_task("to_delete")
        assert dm.delete_task("to_delete") is True
        assert "to_delete" not in dm.get_tasks()

    def test_delete_nonexistent_task(self, dm):
        assert dm.delete_task("ghost") is False

    def test_task_name_sanitize(self, dm):
        """非法字符应被替换，不抛出异常"""
        path = dm.create_task("test/bad\\name:here")
        assert os.path.isdir(path)


# ── 图像保存与索引 ────────────────────────────────────────────────
class TestImageSave:
    def test_save_image_returns_record(self, dm, color_frame):
        path = dm.create_task("t1")
        rec  = dm.save_image(path, color_frame)
        assert "id" in rec
        assert "filename" in rec
        assert rec["annotated"] is False

    def test_save_image_file_exists(self, dm, color_frame):
        path = dm.create_task("t1")
        rec  = dm.save_image(path, color_frame)
        assert os.path.exists(rec["color_path"])

    def test_save_depth_frame(self, dm, color_frame, depth_frame):
        path = dm.create_task("t1")
        rec  = dm.save_image(path, color_frame, depth_frame)
        assert rec["depth_path"] is not None
        assert os.path.exists(rec["depth_path"])

    def test_save_no_depth(self, dm, color_frame):
        path = dm.create_task("t1")
        rec  = dm.save_image(path, color_frame, None)
        assert rec["depth_path"] is None

    def test_get_images_empty(self, dm):
        path = dm.create_task("t1")
        assert dm.get_images(path) == []

    def test_get_images_count(self, dm, color_frame):
        path = dm.create_task("t1")
        dm.save_image(path, color_frame)
        dm.save_image(path, color_frame)
        assert len(dm.get_images(path)) == 2

    def test_image_record_dimensions(self, dm, color_frame):
        path = dm.create_task("t1")
        rec  = dm.save_image(path, color_frame)
        assert rec["width"]  == 640
        assert rec["height"] == 480


# ── 删除图像 ──────────────────────────────────────────────────────
class TestImageDelete:
    def test_delete_image(self, dm, color_frame):
        path = dm.create_task("t1")
        rec  = dm.save_image(path, color_frame)
        img_path = rec["color_path"]
        assert dm.delete_image(path, rec["id"]) is True
        assert not os.path.exists(img_path)
        assert dm.get_images(path) == []

    def test_delete_nonexistent_image(self, dm):
        path = dm.create_task("t1")
        assert dm.delete_image(path, "ghost_id") is False


# ── 标注状态 ──────────────────────────────────────────────────────
class TestAnnotationStatus:
    def test_mark_annotated(self, dm, color_frame, tmp_path):
        path     = dm.create_task("t1")
        rec      = dm.save_image(path, color_frame)
        lbl_path = str(tmp_path / "fake.txt")
        open(lbl_path, "w").close()
        dm.mark_annotated(path, rec["id"], lbl_path)
        updated = dm.get_image_by_id(path, rec["id"])
        assert updated["annotated"] is True
        assert updated["label_path"] == lbl_path

    def test_mark_unannotated(self, dm, color_frame, tmp_path):
        path     = dm.create_task("t1")
        rec      = dm.save_image(path, color_frame)
        lbl_path = str(tmp_path / "fake.txt")
        open(lbl_path, "w").close()
        dm.mark_annotated(path, rec["id"], lbl_path)
        dm.mark_unannotated(path, rec["id"])
        updated = dm.get_image_by_id(path, rec["id"])
        assert updated["annotated"] is False
        assert updated["label_path"] is None


# ── 统计 ──────────────────────────────────────────────────────────
class TestStats:
    def test_stats_empty(self, dm):
        dm.create_task("t1")
        s = dm.get_stats("t1")
        assert s["total"] == 0
        assert s["annotated"] == 0
        assert s["unannotated"] == 0

    def test_stats_after_save(self, dm, color_frame, tmp_path):
        path = dm.create_task("t1")
        r1   = dm.save_image(path, color_frame)
        r2   = dm.save_image(path, color_frame)

        # 标注第一张
        lbl = str(tmp_path / "lbl.txt")
        open(lbl, "w").close()
        dm.mark_annotated(path, r1["id"], lbl)

        s = dm.get_stats("t1")
        assert s["total"]       == 2
        assert s["annotated"]   == 1
        assert s["unannotated"] == 1
