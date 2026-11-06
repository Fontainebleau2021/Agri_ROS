"""解析 ROS2 rosbag2 的 metadata.yaml，用于与 GUI 配置对齐、离线校验。"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml


@dataclass
class Rosbag2Metadata:
    """rosbag2 metadata.yaml 摘要。"""

    bag_dir: Path
    relative_db3: list[str] = field(default_factory=list)
    topics: list[tuple[str, str, int]] = field(default_factory=list)
    duration_ns: int | None = None

    @property
    def topic_names(self) -> list[str]:
        return [t[0] for t in self.topics]

    def topics_by_type(self, msg_type: str) -> list[str]:
        return [name for name, t, _ in self.topics if t == msg_type]

    def db3_paths_resolved(self) -> list[Path]:
        out: list[Path] = []
        for rel in self.relative_db3:
            p = (self.bag_dir / rel).resolve()
            out.append(p)
        return out

    def missing_db3(self) -> list[Path]:
        return [p for p in self.db3_paths_resolved() if not p.is_file()]


def load_rosbag2_metadata(yaml_path: Path | str) -> Rosbag2Metadata:
    path = Path(yaml_path)
    if not path.is_file():
        raise FileNotFoundError(str(path))
    with open(path, encoding="utf-8") as f:
        root: dict[str, Any] = yaml.safe_load(f) or {}
    info = root.get("rosbag2_bagfile_information") or root
    rel = info.get("relative_file_paths") or []
    if isinstance(rel, str):
        rel = [rel]
    duration = info.get("duration") or {}
    dur_ns = duration.get("nanoseconds") if isinstance(duration, dict) else None

    rows: list[tuple[str, str, int]] = []
    twmc = info.get("topics_with_message_count") or []
    for item in twmc:
        meta = (item or {}).get("topic_metadata") or {}
        name = str(meta.get("name", ""))
        typ = str(meta.get("type", ""))
        cnt = int((item or {}).get("message_count", 0))
        if name:
            rows.append((name, typ, cnt))

    bag_dir = path.parent
    return Rosbag2Metadata(
        bag_dir=bag_dir,
        relative_db3=[str(x) for x in rel],
        topics=rows,
        duration_ns=int(dur_ns) if dur_ns is not None else None,
    )


def summarize_for_report(meta: Rosbag2Metadata) -> str:
    lines = [
        f"目录: {meta.bag_dir}",
        f"时长(ns): {meta.duration_ns}",
        f"记录 db3: {meta.relative_db3}",
    ]
    missing = meta.missing_db3()
    if missing:
        lines.append("缺失数据文件（仅 metadata、未附带 bag）:")
        for p in missing:
            lines.append(f"  - {p}")
    pc = meta.topics_by_type("sensor_msgs/msg/PointCloud2")
    ci = meta.topics_by_type("sensor_msgs/msg/CompressedImage")
    img = meta.topics_by_type("sensor_msgs/msg/Image")
    lines.append(f"PointCloud2 话题数: {len(pc)}")
    lines.append(f"CompressedImage 话题数: {len(ci)}")
    lines.append(f"Image 话题数: {len(img)}")
    return "\n".join(lines)
