from __future__ import annotations

import os
import shlex
import sys
from pathlib import Path
from typing import Any

import yaml
from PyQt6.QtCore import QProcess
from PyQt6.QtGui import QAction, QIcon
from PyQt6.QtWidgets import (
    QComboBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLineEdit,
    QFileDialog,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QStatusBar,
    QTabWidget,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

from calibration_tool.config_manager import (
    deep_merge,
    load_config,
    save_user_config,
    user_config_path,
)
from calibration_tool.process_runner import RosProcessRunner
from calibration_tool.tabs.camera_tab import CameraCalibrationTab
from calibration_tool.tabs.fusion_tab import FusionCalibrationTab
from calibration_tool.tabs.lidar_tab import LidarCalibrationTab
from calibration_tool.widgets.log_panel import LogPanel


def _project_dir() -> Path:
    return Path(__file__).resolve().parent.parent


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowIcon(QIcon(str(_project_dir() / "assets" / "app_icon.png")))
        self.setWindowTitle("Multi-Sensor Auto-Calibration Tool (ROS Wrapper)")

        self._cfg: dict[str, Any] = load_config()

        self._ros_ver = QComboBox()
        self._ros_ver.addItems(["ros2", "ros1"])
        self._setup_edit = QPlainTextEdit()
        self._setup_edit.setPlaceholderText(
            "One setup script per line, e.g.:\n/opt/ros/humble/setup.bash"
        )
        self._setup_edit.setFixedHeight(80)
        self._shell_prefix = QLineEdit()
        self._shell_prefix.setPlaceholderText(
            "Optional: Fill when calling ROS via WSL on Windows, e.g. wsl -d Ubuntu -- bash -lc"
        )
        self._out_dir = QLineEdit()
        out_browse = QPushButton("Browse…")
        out_browse.clicked.connect(self._pick_out_dir)

        self._apply_cfg_to_header()

        head_box = QGroupBox("ROS Environment")
        form = QFormLayout()
        form.addRow("Interface Type", self._ros_ver)
        form.addRow("Source List (bash)", self._setup_edit)
        form.addRow("Shell Prefix (Optional)", self._shell_prefix)
        row = QHBoxLayout()
        row.addWidget(self._out_dir)
        row.addWidget(out_browse)
        form.addRow("Calibration Results Save Directory", row)
        head_box.setLayout(form)

        self._camera_tab = CameraCalibrationTab(self._cfg)
        self._lidar_tab = LidarCalibrationTab(self._cfg)
        self._fusion_tab = FusionCalibrationTab(self._cfg)

        tabs = QTabWidget()
        tabs.addTab(self._camera_tab, "Camera Intrinsics")
        tabs.addTab(self._lidar_tab, "Multi-Lidar / Merge")
        tabs.addTab(self._fusion_tab, "Camera-Lidar Fusion")

        self._log = LogPanel(self)

        self._runner = RosProcessRunner(self)
        self._runner.stdout_line.connect(lambda s: self._log.append_line(s, stream="out"))
        self._runner.stderr_line.connect(lambda s: self._log.append_line(s, stream="err"))
        self._runner.finished.connect(self._on_proc_finished)

        self._recorder = QProcess(self)
        self._recorder.readyReadStandardError.connect(self._on_record_stderr)
        self._recorder.finished.connect(self._on_record_finished)

        run_cam = QPushButton("Start Camera Calibration")
        run_cam.clicked.connect(self._run_camera)
        run_merge = QPushButton("Start Merge Node")
        run_merge.clicked.connect(self._run_merge)
        run_lidar_calib = QPushButton("Start Multi-Lidar Calibration")
        run_lidar_calib.clicked.connect(self._run_lidar_calib)
        run_fusion = QPushButton("Start Fusion Calibration")
        run_fusion.clicked.connect(self._run_fusion)
        self._record_btn = QPushButton("Start Recording")
        self._record_btn.clicked.connect(self._toggle_recording)
        stop_btn = QPushButton("Stop Current Process")
        stop_btn.clicked.connect(self._stop_proc)

        btn_row = QHBoxLayout()
        btn_row.addWidget(run_cam)
        btn_row.addWidget(run_merge)
        btn_row.addWidget(run_lidar_calib)
        btn_row.addWidget(run_fusion)
        btn_row.addWidget(self._record_btn)
        btn_row.addWidget(stop_btn)
        btn_row.addStretch(1)

        central = QWidget()
        v = QVBoxLayout(central)
        v.addWidget(head_box)
        v.addWidget(tabs)
        v.addLayout(btn_row)
        v.addWidget(self._log, stretch=1)

        self.setCentralWidget(central)
        self.resize(980, 720)

        self._build_toolbar()
        self.setStatusBar(QStatusBar())

    def _build_toolbar(self) -> None:
        tb = QToolBar()
        tb.setMovable(False)
        act_reload = QAction("Reload Configuration", self)
        act_reload.triggered.connect(self._reload_cfg)
        act_save = QAction("Save Configuration", self)
        act_save.triggered.connect(self._save_all)
        act_preset = QAction("Load Dataset Preset…", self)
        act_preset.triggered.connect(self._load_preset_yaml)
        tb.addAction(act_reload)
        tb.addAction(act_save)
        tb.addAction(act_preset)
        self.addToolBar(tb)

    def _apply_cfg_to_header(self) -> None:
        ros = self._cfg.get("ros", {})
        ver = str(ros.get("version", "ros2")).lower()
        self._ros_ver.setCurrentIndex(1 if ver == "ros1" else 0)
        scripts = ros.get("setup_scripts") or []
        self._setup_edit.setPlainText("\n".join(str(s) for s in scripts))
        self._shell_prefix.setText(str(ros.get("shell_prefix", "")))
        self._out_dir.setText(str(self._cfg.get("output", {}).get("calibration_dir", "./calibration_results")))

    def _pick_out_dir(self) -> None:
        from PyQt6.QtWidgets import QFileDialog

        d = QFileDialog.getExistingDirectory(self, "Select Output Directory")
        if d:
            self._out_dir.setText(d)

    def _reload_cfg(self) -> None:
        self._cfg = load_config()
        self._apply_cfg_to_header()
        self._camera_tab._load_from_cfg()
        self._lidar_tab._load_from_cfg()
        self._fusion_tab._load_from_cfg()
        self.statusBar().showMessage("Configuration reloaded", 3000)

    def _load_preset_yaml(self) -> None:
        presets = _project_dir() / "config" / "presets"
        presets.mkdir(parents=True, exist_ok=True)
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Dataset Preset YAML (will merge with current disk configuration)",
            str(presets),
            "YAML (*.yaml *.yml);;All (*.*)",
        )
        if not path:
            return
        try:
            with open(path, encoding="utf-8") as f:
                preset = yaml.safe_load(f) or {}
            self._cfg = deep_merge(load_config(), preset)
            self._apply_cfg_to_header()
            self._camera_tab._load_from_cfg()
            self._lidar_tab._load_from_cfg()
            self._fusion_tab._load_from_cfg()
        except OSError as e:
            QMessageBox.critical(self, "Load Failed", str(e))
            return
        self.statusBar().showMessage(f"Preset loaded (not auto-saved): {path}", 6000)

    def _sync_header_to_cfg(self) -> None:
        self._cfg.setdefault("ros", {})
        self._cfg["ros"]["version"] = self._ros_ver.currentText().strip()
        lines = [
            ln.strip()
            for ln in self._setup_edit.toPlainText().splitlines()
            if ln.strip()
        ]
        self._cfg["ros"]["setup_scripts"] = lines
        self._cfg["ros"]["shell_prefix"] = self._shell_prefix.text().strip()
        self._cfg.setdefault("output", {})
        self._cfg["output"]["calibration_dir"] = self._out_dir.text().strip()

    def _save_all(self) -> None:
        self._sync_header_to_cfg()
        self._camera_tab.save_to_cfg()
        self._lidar_tab.save_to_cfg()
        self._fusion_tab.save_to_cfg()
        save_user_config(self._cfg)
        self.statusBar().showMessage(f"Saved to {user_config_path()}", 5000)

    def _ros_setup(self) -> tuple[list[str], str]:
        self._sync_header_to_cfg()
        scripts = list(self._cfg.get("ros", {}).get("setup_scripts") or [])
        prefix = str(self._cfg.get("ros", {}).get("shell_prefix") or "")
        return scripts, prefix

    def _run_camera(self) -> None:
        self._camera_tab.save_to_cfg()
        ver = self._ros_ver.currentText().strip()
        cmd = self._camera_tab.build_command(ver)
        if not cmd:
            return
        self._camera_tab.show_real_extrinsics_visualization()
        # Connect output to result display
        self._runner.stdout_line.connect(self._camera_tab.update_result_display)
        self._runner.stderr_line.connect(self._camera_tab.update_result_display)
        self._start_cmd(cmd)

    def _run_merge(self) -> None:
        if self._ros_ver.currentText().strip() != "ros2":
            QMessageBox.information(
                self,
                "Notice",
                "Point cloud merge node is based on ROS2 (multi_lidar_merge). Please switch interface type to ros2.",
            )
            return
        self._lidar_tab.save_to_cfg()
        cmd = self._lidar_tab.build_merge_command_ros2()
        if not cmd:
            return
        self._lidar_tab.show_real_extrinsics_visualization()
        self._start_cmd(cmd)

    def _run_lidar_calib(self) -> None:
        if self._ros_ver.currentText().strip() != "ros2":
            QMessageBox.information(
                self,
                "Notice",
                "Current example command is ros2 launch. If using ROS1, please run corresponding commands manually in terminal.",
            )
        self._lidar_tab.save_to_cfg()
        cmd = self._lidar_tab.build_lidar_calib_command_ros2()
        if not cmd:
            return
        self._lidar_tab.show_real_extrinsics_visualization()
        # Connect output to result display
        self._runner.stdout_line.connect(self._lidar_tab.update_result_display)
        self._runner.stderr_line.connect(self._lidar_tab.update_result_display)
        self._start_cmd(cmd)

    def _run_fusion(self) -> None:
        if self._ros_ver.currentText().strip() != "ros2":
            QMessageBox.warning(self, "Notice", "Fusion calibration startup template defaults to ROS2.")
        self._fusion_tab.save_to_cfg()
        cmd = self._fusion_tab.build_fusion_command_ros2()
        if not cmd:
            return
        self._fusion_tab.show_real_extrinsics_visualization()
        # Connect output to result display
        self._runner.stdout_line.connect(self._fusion_tab.update_result_display)
        self._runner.stderr_line.connect(self._fusion_tab.update_result_display)
        self._start_cmd(cmd)

    def _start_cmd(self, cmd: str) -> None:
        if self._runner.is_running():
            QMessageBox.warning(self, "Notice", "A process is already running, please stop it first.")
            return
        scripts, prefix = self._ros_setup()
        out = self._out_dir.text().strip()
        if out:
            os.makedirs(out, exist_ok=True)
        wd = str(_project_dir())
        self._log.append_line(f"$ {cmd}", stream="out")
        try:
            self._runner.run_shell_command(
                cmd, setup_scripts=scripts, shell_prefix=prefix, workdir=wd
            )
        except RuntimeError as e:
            QMessageBox.critical(self, "Start Failed", str(e))
            return
        self.statusBar().showMessage("Process started", 3000)

    def _stop_proc(self) -> None:
        self._runner.stop()
        self.statusBar().showMessage("Stop requested", 3000)

    def _on_proc_finished(self, code: int, status) -> None:
        self._log.append_line(f"[退出码 {code}]", stream="err")
        # Disconnect result display connections
        try:
            self._runner.stdout_line.disconnect(self._camera_tab.update_result_display)
            self._runner.stderr_line.disconnect(self._camera_tab.update_result_display)
            self._runner.stdout_line.disconnect(self._lidar_tab.update_result_display)
            self._runner.stderr_line.disconnect(self._lidar_tab.update_result_display)
            self._runner.stdout_line.disconnect(self._fusion_tab.update_result_display)
            self._runner.stderr_line.disconnect(self._fusion_tab.update_result_display)
        except TypeError:
            pass  # Not connected

    def _toggle_recording(self) -> None:
        if self._recorder.state() == QProcess.ProcessState.Running:
            self._stop_recording()
            return

        path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Operation Video",
            self._out_dir.text() or str(_project_dir() / "recordings"),
            "MP4 Video (*.mp4)",
        )
        if not path:
            return
        if not path.lower().endswith(".mp4"):
            path += ".mp4"

        cmd = self._build_record_command(path)
        if not cmd:
            QMessageBox.critical(self, "Cannot Record", "No available recording command found, please install ffmpeg first.")
            return

        self._log.append_line(f"$ {cmd}", stream="out")
        if sys.platform == "win32":
            self._recorder.start("cmd.exe", ["/C", cmd])
        else:
            self._recorder.start("bash", ["-lc", cmd])

        if not self._recorder.waitForStarted(2000):
            QMessageBox.critical(self, "Recording Failed", "Recording process failed to start, please check if ffmpeg is installed.")
            return

        self._record_btn.setText("Stop Recording")
        self.statusBar().showMessage(f"录制中: {path}", 5000)

    def _stop_recording(self) -> None:
        if self._recorder.state() == QProcess.ProcessState.Running:
            self._recorder.terminate()
            self._recorder.waitForFinished(3000)
        self._record_btn.setText("开始录制")
        self.statusBar().showMessage("录制已停止", 3000)

    def _build_record_command(self, output_path: str) -> str | None:
        if sys.platform == "win32":
            return f'ffmpeg -y -f gdigrab -framerate 15 -i desktop "{output_path}"'
        if sys.platform.startswith("linux"):
            display = os.environ.get("DISPLAY", ":0")
            return f'ffmpeg -y -video_size 1280x720 -framerate 15 -f x11grab -i {shlex.quote(display)} "{output_path}"'
        if sys.platform == "darwin":
            return f'ffmpeg -y -f avfoundation -framerate 15 -i 1 "{output_path}"'
        return None

    def _on_record_stderr(self) -> None:
        data = bytes(self._recorder.readAllStandardError()).decode(
            sys.getfilesystemencoding() or "utf-8", errors="replace"
        )
        for line in data.splitlines():
            self._log.append_line(line, stream="err")

    def _on_record_finished(self, code: int, status) -> None:
        self._record_btn.setText("开始录制")
        self._log.append_line(f"[录制结束 退出码 {code}]", stream="out")
        self.statusBar().showMessage("录制结束", 5000)

    def closeEvent(self, event) -> None:  # noqa: N802
        if self._runner.is_running():
            self._runner.stop()
        event.accept()


def run() -> None:
    from PyQt6.QtWidgets import QApplication

    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
