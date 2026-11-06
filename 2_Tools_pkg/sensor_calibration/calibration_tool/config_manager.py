from __future__ import annotations

import copy
import os
from pathlib import Path
from typing import Any

import yaml


def _project_root() -> Path:
    return Path(__file__).resolve().parent.parent


def default_config_path() -> Path:
    return _project_root() / "config" / "default.yaml"


def user_config_path() -> Path:
    return _project_root() / "config" / "user.yaml"


def load_config() -> dict[str, Any]:
    path = user_config_path()
    if not path.is_file():
        path = default_config_path()
    with open(path, encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    return data


def save_user_config(data: dict[str, Any]) -> None:
    path = user_config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        yaml.safe_dump(data, f, allow_unicode=True, sort_keys=False)


def deep_merge(base: dict[str, Any], override: dict[str, Any]) -> dict[str, Any]:
    out = copy.deepcopy(base)
    for k, v in override.items():
        if k in out and isinstance(out[k], dict) and isinstance(v, dict):
            out[k] = deep_merge(out[k], v)
        else:
            out[k] = copy.deepcopy(v)
    return out


def expand_path(p: str) -> str:
    return os.path.expanduser(os.path.expandvars(p))
