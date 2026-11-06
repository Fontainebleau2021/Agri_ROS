from __future__ import annotations

import os
import shlex
import signal
import sys
from pathlib import Path
from typing import Iterable

from PyQt6.QtCore import QObject, QProcess, pyqtSignal


def build_source_prefix_unix(setup_scripts: Iterable[str]) -> str:
    parts: list[str] = []
    for s in setup_scripts:
        s = s.strip()
        if not s:
            continue
        sp = Path(s).expanduser()
        parts.append(f'source "{sp.as_posix()}"')
    return " && ".join(parts)


class RosProcessRunner(QObject):
    """在子进程中运行 Shell 命令；可选先 source ROS workspace。"""

    stdout_line = pyqtSignal(str)
    stderr_line = pyqtSignal(str)
    finished = pyqtSignal(int, QProcess.ExitStatus)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._proc = QProcess(self)
        self._proc.readyReadStandardOutput.connect(self._on_stdout)
        self._proc.readyReadStandardError.connect(self._on_stderr)
        self._proc.finished.connect(self._on_finished)

    def _on_stdout(self) -> None:
        data = bytes(self._proc.readAllStandardOutput()).decode(
            sys.getfilesystemencoding() or "utf-8", errors="replace"
        )
        for line in data.splitlines():
            self.stdout_line.emit(line)

    def _on_stderr(self) -> None:
        data = bytes(self._proc.readAllStandardError()).decode(
            sys.getfilesystemencoding() or "utf-8", errors="replace"
        )
        for line in data.splitlines():
            self.stderr_line.emit(line)

    def _on_finished(self, code: int, status: QProcess.ExitStatus) -> None:
        self.finished.emit(code, status)

    def stop(self) -> None:
        if self._proc.state() == QProcess.ProcessState.NotRunning:
            return
        self._proc.terminate()
        if self._proc.waitForFinished(3000):
            return

        pid = int(self._proc.processId() or 0)
        if pid:
            try:
                pgid = os.getpgid(pid)
                os.killpg(pgid, signal.SIGTERM)
            except OSError:
                pass
        self._proc.kill()

    def is_running(self) -> bool:
        return self._proc.state() != QProcess.ProcessState.NotRunning

    def run_shell_command(
        self,
        command: str,
        *,
        setup_scripts: list[str] | None = None,
        shell_prefix: str = "",
        workdir: str | None = None,
    ) -> None:
        """
        Linux/macOS/WSL：使用 bash -lc 执行，前面的 source 来自 setup_scripts。
        Windows（无 bash）：直接启动 command（需自行保证 ROS 在 PATH 中，或使用 WSL）。
        """
        if self.is_running():
            raise RuntimeError("已有进程在运行，请先停止。")

        setup_scripts = setup_scripts or []
        wd = workdir or os.getcwd()

        if sys.platform == "win32":
            cmd_line = self._windows_command(command, setup_scripts, shell_prefix)
            self._proc.setWorkingDirectory(wd)
            self._proc.start(cmd_line[0], cmd_line[1:])
            return

        inner = command.strip()
        if setup_scripts:
            inner = build_source_prefix_unix(setup_scripts) + " && " + inner
        bash_lc = ["bash", "-lc", inner]
        self._proc.setWorkingDirectory(wd)
        self._proc.start(bash_lc[0], bash_lc[1:])

    def _windows_command(
        self, command: str, setup_scripts: list[str], shell_prefix: str
    ) -> list[str]:
        prefix = shell_prefix.strip()
        if prefix:
            inner = command
            if setup_scripts:
                parts = " && ".join(
                    f'source "{Path(s).expanduser().as_posix()}"'
                    for s in setup_scripts
                    if s.strip()
                )
                if parts:
                    inner = parts + " && " + inner
            escaped = inner.replace('"', '\\"')
            line = f'{prefix} "{escaped}"'
            return ["cmd.exe", "/C", line]

        if setup_scripts:
            parts = "; ".join(
                f'call "{Path(s).expanduser()}"'
                for s in setup_scripts
                if s.strip()
            )
            if parts:
                full = parts + " && " + command
                return ["cmd.exe", "/C", full]
        return ["cmd.exe", "/C", command]
