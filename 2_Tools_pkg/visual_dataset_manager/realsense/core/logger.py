# core/logger.py
# 统一日志系统：
#   - 同时输出到控制台和日志文件（logs/app_YYYYMMDD.log）
#   - 日志文件自动按日期滚动，保留最近 7 天
#   - 全项目通过 get_logger(__name__) 获取 logger，替换原有 print

import logging
import os
from logging.handlers import TimedRotatingFileHandler
from datetime import datetime

_LOG_DIR      = "logs"
_LOG_LEVEL    = logging.DEBUG
_RETAIN_DAYS  = 7
_initialized  = False


def _init():
    global _initialized
    if _initialized:
        return
    _initialized = True

    os.makedirs(_LOG_DIR, exist_ok=True)

    root = logging.getLogger()
    root.setLevel(_LOG_LEVEL)

    fmt = logging.Formatter(
        fmt="%(asctime)s  %(levelname)-8s  [%(name)s]  %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    # ── 控制台 Handler ──────────────────────────────────────────
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    ch.setFormatter(fmt)
    root.addHandler(ch)

    # ── 文件 Handler（每天午夜滚动，保留 7 天）─────────────────
    log_path = os.path.join(_LOG_DIR, "app.log")
    fh = TimedRotatingFileHandler(
        log_path,
        when="midnight",
        backupCount=_RETAIN_DAYS,
        encoding="utf-8",
    )
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(fmt)
    fh.suffix = "%Y%m%d"
    root.addHandler(fh)

    root.info("=" * 60)
    root.info(f"日志系统初始化完成  →  {os.path.abspath(log_path)}")
    root.info("=" * 60)


def get_logger(name: str) -> logging.Logger:
    """
    获取命名 logger，在首次调用时完成初始化。

    用法（替换原有 print）：
        from core.logger import get_logger
        logger = get_logger(__name__)
        logger.info("相机已启动")
        logger.warning("帧读取超时")
        logger.error("保存失败", exc_info=True)
    """
    _init()
    return logging.getLogger(name)
