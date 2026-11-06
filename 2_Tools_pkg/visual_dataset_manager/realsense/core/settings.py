# core/settings.py
# 用户设置持久化：
#   - 保存到 settings.json（项目根目录）
#   - 记忆：上次任务、类别列表、导出格式、窗口大小、QTimer 间隔等
#   - 提供 get / set / save / load 接口，支持带默认值的 get

import os
import json
from typing import Any

import config as cfg
from core.logger import get_logger

logger   = get_logger(__name__)
_SETTING_FILE = "settings.json"

# ── 默认值（首次运行或文件损坏时使用）────────────────────────────
_DEFAULTS: dict = {
    "last_task":        "",
    "classes":          list(cfg.DEFAULT_CLASSES),
    "export_format":    cfg.DEFAULT_EXPORT_FORMAT,   # "yolo" | "voc"
    "train_ratio":      0.8,   # train/val 划分比例
    "save_depth":       True,
    "burst_interval":   2,     # 连拍间隔（秒）
    "timer_interval_ms": int(1000 / cfg.CAMERA_FPS),
    "window_w":         cfg.WINDOW_W,
    "window_h":         cfg.WINDOW_H,
    "window_maximized": False,
}

_data: dict = {}


def load():
    """从 settings.json 加载设置；文件不存在或损坏则使用默认值"""
    global _data
    _data = dict(_DEFAULTS)
    if os.path.exists(_SETTING_FILE):
        try:
            with open(_SETTING_FILE, "r", encoding="utf-8") as f:
                saved = json.load(f)
            # 只合并已知键，忽略版本差异引入的旧键
            for k in _DEFAULTS:
                if k in saved:
                    _data[k] = saved[k]
            logger.info(f"设置已加载 ← {os.path.abspath(_SETTING_FILE)}")
        except Exception as e:
            logger.warning(f"settings.json 读取失败，使用默认值：{e}")
    else:
        logger.info("首次运行，使用默认设置")


def save():
    """将当前设置写入 settings.json"""
    try:
        with open(_SETTING_FILE, "w", encoding="utf-8") as f:
            json.dump(_data, f, ensure_ascii=False, indent=2)
        logger.debug(f"设置已保存 → {os.path.abspath(_SETTING_FILE)}")
    except Exception as e:
        logger.error(f"设置保存失败：{e}")


def get(key: str, default: Any = None) -> Any:
    """读取设置值；key 不存在时返回 default"""
    if not _data:
        load()
    return _data.get(key, default if default is not None else _DEFAULTS.get(key))


def set(key: str, value: Any):
    """更新一个设置项（内存中，需调用 save() 持久化）"""
    if not _data:
        load()
    _data[key] = value


def set_and_save(key: str, value: Any):
    """更新并立即持久化（适合单项改动）"""
    set(key, value)
    save()
