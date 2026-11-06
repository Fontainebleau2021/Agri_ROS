from __future__ import annotations

import runpy
from pathlib import Path
from typing import Any

import matplotlib
import numpy as np
from matplotlib.figure import Figure

# Chinese fonts will be set individually in each visualization function


def _dataset_dir() -> Path:
    return Path(__file__).resolve().parent.parent / "data" / "标定数据"


def load_igv_parameters() -> dict[str, Any]:
    path = _dataset_dir() / "8-igv_sensor_parameters-环视相机激光雷达内外参.py"
    if not path.exists():
        raise FileNotFoundError(f"Real extrinsic data file not found: {path}")
    return runpy.run_path(str(path))


def build_camera_visualization_figure(data: dict[str, Any]) -> tuple[Figure, str]:
    # Ensure font settings apply to current Figure
    matplotlib.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'DejaVu Sans']
    matplotlib.rcParams['axes.unicode_minus'] = False

    fig = Figure(figsize=(12, 8), tight_layout=True)
    axes = fig.subplots(2, 2)

    camera_matrices = [data[f"camera_matrix{i+1}"] for i in range(8)]
    focal_lengths = [m[0, 0] for m in camera_matrices]
    principal_points = [m[0, 2] for m in camera_matrices]
    dist_coeffs = np.array([data[f"dist_coeffs{i+1}"][:5] for i in range(8)])

    axes[0, 0].imshow(camera_matrices[0], cmap="viridis")
    axes[0, 0].set_title("Camera 1 Intrinsic Matrix Heatmap")
    axes[0, 0].set_xlabel("fx, 0, cx\n0, fy, cy\n0, 0, 1")

    axes[0, 1].bar(range(1, 9), focal_lengths, color="tab:blue")
    axes[0, 1].set_title("Camera Focal Length fx Distribution")
    axes[0, 1].set_xlabel("Camera ID")
    axes[0, 1].set_ylabel("Pixels")
    axes[0, 1].grid(True, linestyle="--", alpha=0.3)

    camera_centers = np.array([data[f"camera2lidar{i+1}"][:3] for i in range(8)])
    axes[1, 0].scatter(camera_centers[:, 0], camera_centers[:, 1], marker="o", color="tab:orange")
    for i, (x, y) in enumerate(camera_centers[:, :2], start=1):
        axes[1, 0].text(x, y, f"Cam{i}", fontsize=9, ha="center", va="bottom")
    axes[1, 0].set_title("Camera Centers XY Projection")
    axes[1, 0].set_xlabel("X (m)")
    axes[1, 0].set_ylabel("Y (m)")
    axes[1, 0].grid(True, linestyle="--", alpha=0.3)

    axes[1, 1].bar(np.arange(1, 9) - 0.2, dist_coeffs[:, 0], width=0.2, label="k1")
    axes[1, 1].bar(np.arange(1, 9), dist_coeffs[:, 1], width=0.2, label="k2")
    axes[1, 1].bar(np.arange(1, 9) + 0.2, dist_coeffs[:, 4], width=0.2, label="k5")
    axes[1, 1].set_title("Camera Distortion Coefficients (k1/k2/k5)")
    axes[1, 1].set_xlabel("Camera ID")
    axes[1, 1].legend(fontsize=8)
    axes[1, 1].grid(True, linestyle="--", alpha=0.3)

    summary = (
        "Using real extrinsic parameters from data/标定数据/8-igv_sensor_parameters-环视相机激光雷达内外参.py.\n"
        "This window shows: Camera intrinsic matrix heatmap, focal length distribution, camera center XY projection, and distortion coefficient comparison.\n"
        "Camera extrinsics are provided by camera2lidar1..camera2lidar8 and can directly replace calibration results for demonstration."
    )
    return fig, summary


def build_lidar_visualization_figure(data: dict[str, Any]) -> tuple[Figure, str]:
    # Ensure font settings apply to current Figure
    matplotlib.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'DejaVu Sans']
    matplotlib.rcParams['axes.unicode_minus'] = False

    fig = Figure(figsize=(12, 8), tight_layout=True)
    axes = fig.subplots(2, 1)

    lidar_names = ["rs1", "rs2", "rs3", "rs4", "iv1", "iv2"]
    lidar_transforms = [data[name] for name in ("transform_rs1", "transform_rs2", "transform_rs3", "transform_rs4", "transform_iv1", "transform_iv2")]
    lidar_positions = np.array([t[:3] for t in lidar_transforms])
    lidar_heights = lidar_positions[:, 2]

    axes[0].scatter(lidar_positions[:, 0], lidar_positions[:, 1], marker="^", s=100, color="tab:red")
    for name, (x, y) in zip(lidar_names, lidar_positions[:, :2]):
        axes[0].text(x, y, name, fontsize=9, ha="center", va="bottom")
    axes[0].set_title("Lidar Sensor Real Positions (XY)")
    axes[0].set_xlabel("X (m)")
    axes[0].set_ylabel("Y (m)")
    axes[0].grid(True, linestyle="--", alpha=0.3)

    axes[1].bar(lidar_names, lidar_heights, color="tab:green")
    axes[1].set_title("Lidar Sensor Heights (Z-axis)")
    axes[1].set_ylabel("Height (m)")
    axes[1].grid(True, linestyle="--", alpha=0.3)

    summary = (
        "Using real lidar extrinsic parameters from data/标定数据/8-igv_sensor_parameters-环视相机激光雷达内外参.py.\n"
        "This window shows: Actual lidar sensor XY layout and height distribution in vehicle coordinate system, for validating extrinsic parameters and sensor mounting positions.\n"
        "Can be used to replace online calibration results for demonstration, and pops up after starting multi-lidar calibration."
    )
    return fig, summary


def build_fusion_visualization_figure(data: dict[str, Any]) -> tuple[Figure, str]:
    # Ensure font settings apply to current Figure
    matplotlib.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'DejaVu Sans']
    matplotlib.rcParams['axes.unicode_minus'] = False

    fig = Figure(figsize=(12, 8), tight_layout=True)
    axes = fig.subplots(2, 1)

    camera_positions = np.array([data[f"camera2lidar{i+1}"][:3] for i in range(8)])
    lidar_positions = np.array([data[name][:3] for name in ("transform_rs1", "transform_rs2", "transform_rs3", "transform_rs4", "transform_iv1", "transform_iv2")])

    axes[0].scatter(camera_positions[:, 0], camera_positions[:, 1], marker="o", s=80, color="tab:blue", label="Camera")
    axes[0].scatter(lidar_positions[:, 0], lidar_positions[:, 1], marker="^", s=100, color="tab:red", label="Lidar")
    for i, (x, y) in enumerate(camera_positions[:, :2], start=1):
        axes[0].text(x, y, f"C{i}", fontsize=8, ha="center", va="bottom")
    for i, (x, y) in enumerate(lidar_positions[:, :2], start=1):
        axes[0].text(x, y, f"L{i}", fontsize=8, ha="center", va="bottom")
    axes[0].set_title("Camera-Lidar Fusion Real Extrinsic Layout")
    axes[0].set_xlabel("X (m)")
    axes[0].set_ylabel("Y (m)")
    axes[0].legend()
    axes[0].grid(True, linestyle="--", alpha=0.3)

    translation_magnitudes = np.linalg.norm(camera_positions, axis=1)
    axes[1].bar(np.arange(1, 9), translation_magnitudes, color="tab:purple")
    axes[1].set_title("Camera-to-Lidar Translation Magnitudes")
    axes[1].set_xlabel("Camera ID")
    axes[1].set_ylabel("Distance (m)")
    axes[1].grid(True, linestyle="--", alpha=0.3)

    summary = (
        "Using real fusion extrinsic parameters from data/标定数据/8-igv_sensor_parameters-环视相机激光雷达内外参.py.\n"
        "This window shows camera and lidar real mounting positions, camera-to-lidar translation magnitudes, and real spatial relationships for fusion calibration.\n"
        "Pops up after starting camera-lidar fusion calibration and can directly replace simulated/calibration results for visualization."
    )
    return fig, summary
