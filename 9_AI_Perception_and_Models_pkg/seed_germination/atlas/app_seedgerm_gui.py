#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SeedGerm Atlas GUI - final submission version.

This GUI uses only Python standard library modules. It does not require Flask.
It calls the deployed U-Net + CV pipeline:
    /root/seedgerm_atlas/run_seedgerm_unet_cv_pipeline.py

Start:
    cd /root/seedgerm_atlas
    source /usr/local/Ascend/ascend-toolkit/set_env.sh
    python3 app_seedgerm_gui.py

Open in browser:
    http://<Atlas-IP>:7860
"""

from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs, unquote
from datetime import datetime
import cgi
import csv
import html
import json
import mimetypes
import shutil
import subprocess
import shlex


ROOT = Path("/root/seedgerm_atlas")
RUNS_DIR = ROOT / "gui_runs"
PIPELINE_SCRIPT = ROOT / "run_seedgerm_unet_cv_pipeline.py"
ASCEND_ENV = "/usr/local/Ascend/ascend-toolkit/set_env.sh"

HOST = "0.0.0.0"
PORT = 7860

RUNS_DIR.mkdir(exist_ok=True)


def esc(x):
    return html.escape(str(x))


def pretty_html(title, body):
    page = """
<!doctype html>
<html>
<head>
<link rel="icon" type="image/png" href="/app-icon.png">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<style>
:root {
  --bg0: #08111f;
  --bg1: #101c32;
  --card: rgba(255,255,255,0.93);
  --text: #172033;
  --muted: #6b7280;
  --green: #19a974;
  --blue: #2f6df6;
  --cyan: #00bcd4;
  --line: rgba(15,23,42,0.10);
  --shadow: 0 22px 70px rgba(0,0,0,0.22);
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: Arial, "Microsoft YaHei", sans-serif;
  color: var(--text);
  background:
    radial-gradient(circle at 10% 10%, rgba(47,109,246,0.32), transparent 32%),
    radial-gradient(circle at 92% 8%, rgba(25,169,116,0.28), transparent 30%),
    linear-gradient(135deg, var(--bg0), var(--bg1));
  min-height: 100vh;
}
a { color: var(--blue); text-decoration: none; }
.shell { max-width: 1240px; margin: 0 auto; padding: 34px 24px 60px; }
.hero { color: white; padding: 34px 0 26px; }
.hero-title { font-size: 42px; line-height: 1.12; font-weight: 800; margin: 0 0 12px; letter-spacing: -0.6px; }
.hero-subtitle { max-width: 850px; color: rgba(255,255,255,0.76); font-size: 16px; line-height: 1.75; }
.badges { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 20px; }
.badge { padding: 8px 12px; border: 1px solid rgba(255,255,255,0.20); background: rgba(255,255,255,0.10); color: rgba(255,255,255,0.88); border-radius: 999px; font-size: 13px; backdrop-filter: blur(10px); }
.panel, .section, .metric { background: var(--card); border: 1px solid rgba(255,255,255,0.70); box-shadow: var(--shadow); }
.panel { border-radius: 24px; overflow: hidden; }
.panel-inner { padding: 28px; }
.upload-card { display: grid; grid-template-columns: 1.1fr 0.9fr; gap: 24px; align-items: stretch; }
.upload-zone {
  border: 2px dashed rgba(47,109,246,0.35);
  background: linear-gradient(135deg, rgba(47,109,246,0.08), rgba(25,169,116,0.08));
  border-radius: 22px;
  min-height: 280px;
  padding: 28px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  cursor: pointer;
  transition: all .2s ease;
}
.upload-zone:hover { transform: translateY(-2px); border-color: rgba(47,109,246,0.75); box-shadow: 0 18px 45px rgba(47,109,246,0.16); }
.upload-icon { width: 62px; height: 62px; border-radius: 18px; background: linear-gradient(135deg, var(--blue), var(--green)); color: white; display: flex; align-items: center; justify-content: center; font-size: 28px; margin-bottom: 18px; }
.upload-title { font-size: 24px; font-weight: 800; margin-bottom: 10px; }
.upload-note, .step-text { color: var(--muted); line-height: 1.75; font-size: 14px; }
input[type=file] { display: none; }
.side-info { background: rgba(15,23,42,0.04); border: 1px solid var(--line); border-radius: 22px; padding: 24px; }
.side-info h3 { margin: 0 0 14px; font-size: 18px; }
.steps { display: grid; gap: 12px; margin-top: 18px; }
.step { display: grid; grid-template-columns: 34px 1fr; gap: 12px; align-items: start; }
.step-num { width: 34px; height: 34px; border-radius: 12px; background: white; border: 1px solid var(--line); display: flex; align-items: center; justify-content: center; font-weight: 800; color: var(--blue); }
.file-list { margin-top: 16px; color: var(--muted); font-size: 14px; line-height: 1.7; max-height: 120px; overflow: auto; }
.actions { display: flex; gap: 12px; align-items: center; margin-top: 22px; }
.btn { border: none; border-radius: 14px; background: linear-gradient(135deg, var(--blue), var(--green)); color: white; font-size: 16px; font-weight: 700; padding: 13px 22px; cursor: pointer; box-shadow: 0 12px 30px rgba(47,109,246,0.25); }
.btn:disabled { background: #b8c0cc; cursor: not-allowed; box-shadow: none; }
.muted { color: var(--muted); }
.grid-metrics { display: grid; grid-template-columns: repeat(4, minmax(150px, 1fr)); gap: 16px; margin-bottom: 22px; }
.metric { border-radius: 20px; padding: 20px; }
.metric-label { font-size: 13px; color: var(--muted); margin-bottom: 8px; }
.metric-value { font-size: 28px; font-weight: 800; }
.section { border-radius: 22px; padding: 24px; margin-bottom: 22px; }
.section h2 { margin: 0 0 16px; }
.score-card { display: grid; grid-template-columns: 240px 1fr; gap: 26px; align-items: center; }
.score-ring { width: 210px; height: 210px; border-radius: 50%; background: conic-gradient(var(--green) calc(var(--score) * 1%), rgba(15,23,42,0.10) 0); display: flex; align-items: center; justify-content: center; margin: auto; position: relative; }
.score-ring::after { content: ""; position: absolute; width: 155px; height: 155px; border-radius: 50%; background: white; }
.score-ring span { position: relative; z-index: 2; font-size: 36px; font-weight: 900; color: #0f5132; }
.subscores { display: grid; gap: 12px; }
.bar-row { display: grid; grid-template-columns: 160px 1fr 70px; gap: 12px; align-items: center; font-size: 14px; }
.bar { height: 12px; border-radius: 999px; background: rgba(15,23,42,0.10); overflow: hidden; }
.bar-fill { height: 100%; border-radius: 999px; background: linear-gradient(90deg, var(--blue), var(--green)); }
.table-wrap { overflow-x: auto; border: 1px solid var(--line); border-radius: 14px; }
table { border-collapse: collapse; width: 100%; background: white; font-size: 13px; }
th, td { border-bottom: 1px solid rgba(15,23,42,0.08); padding: 10px 12px; text-align: left; white-space: nowrap; }
th { background: #f8fafc; color: #475467; font-weight: 800; }
.image-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(290px, 1fr)); gap: 16px; }
.image-card { background: white; border: 1px solid var(--line); border-radius: 18px; padding: 12px; box-shadow: 0 8px 22px rgba(15,23,42,0.08); }
.image-card img { width: 100%; border-radius: 14px; display: block; }
.image-card p { margin: 10px 4px 2px; color: var(--muted); font-size: 13px; }
.downloads { display: flex; gap: 12px; flex-wrap: wrap; }
.download { padding: 12px 16px; border-radius: 14px; background: #eef4ff; color: #1d4ed8; font-weight: 700; }
.loading { display: none; position: fixed; inset: 0; background: rgba(8,17,31,0.76); z-index: 999; align-items: center; justify-content: center; color: white; text-align: center; backdrop-filter: blur(6px); }
.loader-box { background: rgba(255,255,255,0.12); border: 1px solid rgba(255,255,255,0.22); padding: 36px; border-radius: 22px; width: 360px; }
.spinner { width: 54px; height: 54px; border-radius: 50%; border: 5px solid rgba(255,255,255,0.25); border-top-color: white; margin: 0 auto 18px; animation: spin 1s linear infinite; }
@keyframes spin { to { transform: rotate(360deg); } }
@media (max-width: 900px) { .upload-card, .score-card { grid-template-columns: 1fr; } .grid-metrics { grid-template-columns: repeat(2, 1fr); } .hero-title { font-size: 32px; } }
</style>
</head>
<body>
<div class="shell">
__BODY__
</div>
</body>
</html>
"""
    page = page.replace("__TITLE__", esc(title))
    page = page.replace("__BODY__", body)
    return page


def index_page():
    body = """
<div class="hero">
  <h1 class="hero-title">SeedGerm Atlas<br>种子活力检测系统</h1>
  <div class="hero-subtitle">
    基于 Atlas 310B1 的 U-Net OM 推理与传统 CV 后处理，自动完成种子、幼苗与整体前景分割，并生成种子活力基线评分。
  </div>
  <div class="badges">
    <div class="badge">Atlas 310B1</div>
    <div class="badge">U-Net OM</div>
    <div class="badge">Seed / Seedling / Overall</div>
    <div class="badge">Baseline Vigor Score</div>
  </div>
</div>

<div class="panel">
  <div class="panel-inner">
    <form id="upload-form" method="post" action="/run" enctype="multipart/form-data">
      <div class="upload-card">
        <label class="upload-zone" id="drop-zone">
          <input id="file-input" type="file" name="images" multiple accept=".jpg,.jpeg,.png,.bmp">
          <div class="upload-icon">↑</div>
          <div class="upload-title">上传种子萌发时序图像</div>
          <div class="upload-note">
            支持 JPG / PNG / BMP。建议一次性上传同一组实验的全部时序图片。
            如果文件名类似 G7_20221026_1637.jpg，系统会自动解析时间轴。
          </div>
          <div id="file-list" class="file-list">尚未选择图片</div>
        </label>

        <div class="side-info">
          <h3>分析流程</h3>
          <div class="steps">
            <div class="step"><div class="step-num">1</div><div class="step-text">上传图片后，系统按文件名排序并解析时间。</div></div>
            <div class="step"><div class="step-num">2</div><div class="step-text">Atlas NPU 运行三个 U-Net OM 模型。</div></div>
            <div class="step"><div class="step-num">3</div><div class="step-text">输出 seed、seedling、overall mask 与 overlay。</div></div>
            <div class="step"><div class="step-num">4</div><div class="step-text">计算面积、骨架长度、增长曲线和活力分数。</div></div>
          </div>
          <div class="actions">
            <button id="submit-btn" class="btn" type="submit" disabled>开始分析</button>
            <span class="muted" id="hint">请选择图片</span>
          </div>
        </div>
      </div>
    </form>
  </div>
</div>

<div class="loading" id="loading">
  <div class="loader-box">
    <div class="spinner"></div>
    <h2>正在分析，请稍等</h2>
    <p>Atlas 正在运行 U-Net OM 模型并生成结果。</p>
  </div>
</div>

<script>
const input = document.getElementById("file-input");
const fileList = document.getElementById("file-list");
const submitBtn = document.getElementById("submit-btn");
const hint = document.getElementById("hint");
const form = document.getElementById("upload-form");
const loading = document.getElementById("loading");
function updateFiles() {
  const files = Array.from(input.files || []);
  if (files.length === 0) {
    fileList.textContent = "尚未选择图片";
    submitBtn.disabled = true;
    hint.textContent = "请选择图片";
    return;
  }
  submitBtn.disabled = false;
  hint.textContent = files.length + " 张图片已选择";
  fileList.innerHTML = files.slice(0, 8).map(f => "• " + f.name).join("<br>");
  if (files.length > 8) fileList.innerHTML += "<br>... 还有 " + (files.length - 8) + " 张";
}
input.addEventListener("change", updateFiles);
form.addEventListener("submit", function() { loading.style.display = "flex"; });
</script>
"""
    return pretty_html("SeedGerm Atlas GUI", body)


def read_csv_rows(csv_path):
    if not csv_path.exists():
        return [], []
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        headers = reader.fieldnames or []
        rows = list(reader)
    return headers, rows


def pct(v):
    try:
        x = float(v)
    except Exception:
        return 0.0
    return max(0.0, min(100.0, x))


def result_metric(label, value):
    return f"""
<div class="metric">
  <div class="metric-label">{esc(label)}</div>
  <div class="metric-value">{esc(value)}</div>
</div>
"""


def score_bar(label, value):
    p = pct(value)
    return f"""
<div class="bar-row">
  <div>{esc(label)}</div>
  <div class="bar"><div class="bar-fill" style="width:{p:.1f}%"></div></div>
  <div>{p:.1f}</div>
</div>
"""


def result_page(run_id):
    run_dir = RUNS_DIR / run_id
    output_dir = run_dir / "output_pipeline"
    summary_dir = output_dir / "summary"
    overlays_dir = output_dir / "overlays"
    vigor_path = summary_dir / "vigor_score.json"
    csv_path = summary_dir / "image_trait_summary.csv"
    curve_path = summary_dir / "growth_curve_skeleton_length.png"
    vigor = {}
    if vigor_path.exists():
        vigor = json.loads(vigor_path.read_text(encoding="utf-8"))
    headers, rows = read_csv_rows(csv_path)
    score = vigor.get("batch_vigor_score", "NA")
    score_num = pct(score)
    parts = []
    parts.append(f"""
<div class="hero">
  <h1 class="hero-title">分析完成</h1>
  <div class="hero-subtitle">Run ID: {esc(run_id)}。下方展示 U-Net + CV baseline 活力评分、时序表型统计、增长曲线和分割 overlay。</div>
  <div class="badges"><div class="badge"><a style="color:white" href="/">返回上传页</a></div><div class="badge">No YOLO</div><div class="badge">Deployable Baseline</div></div>
</div>
""")
    parts.append("<div class='grid-metrics'>")
    parts.append(result_metric("Vigor Score", str(score)))
    parts.append(result_metric("Seed Count", vigor.get("seed_count_estimated", "NA")))
    parts.append(result_metric("Emergence h", vigor.get("time_to_seedling_emergence_h", "NA")))
    parts.append(result_metric("Final Skeleton", vigor.get("final_seedling_skeleton_length_512", "NA")))
    parts.append("</div>")
    parts.append(f"""
<div class="section">
  <div class="score-card">
    <div class="score-ring" style="--score:{score_num:.1f}"><span>{score_num:.1f}</span></div>
    <div>
      <h2>Baseline Vigor Score</h2>
      <p class="muted">该分数基于 U-Net 分割和 CV 表型，不使用 YOLO 阶段分类。</p>
      <div class="subscores">
        {score_bar("Completion", vigor.get("completion_score", 0))}
        {score_bar("Speed", vigor.get("speed_score", 0))}
        {score_bar("Growth", vigor.get("growth_score", 0))}
        {score_bar("Uniformity", vigor.get("uniformity_score", 0))}
      </div>
    </div>
  </div>
</div>
""")
    parts.append("<div class='section'><h2>完整评分信息</h2><div class='table-wrap'><table><tr><th>指标</th><th>数值</th></tr>")
    for k, v in vigor.items():
        parts.append(f"<tr><td>{esc(k)}</td><td>{esc(v)}</td></tr>")
    parts.append("</table></div></div>")
    parts.append("<div class='section'><h2>Trait Summary</h2><div class='table-wrap'>")
    if headers and rows:
        parts.append("<table><tr>")
        for h in headers:
            parts.append(f"<th>{esc(h)}</th>")
        parts.append("</tr>")
        for r in rows:
            parts.append("<tr>")
            for h in headers:
                parts.append(f"<td>{esc(r.get(h, ''))}</td>")
            parts.append("</tr>")
        parts.append("</table>")
    else:
        parts.append("<p>未找到 image_trait_summary.csv</p>")
    parts.append("</div></div>")
    parts.append("<div class='section'><h2>Growth Curve</h2>")
    if curve_path.exists():
        rel = curve_path.relative_to(run_dir)
        parts.append(f'<img src="/files/{esc(run_id)}/{esc(str(rel))}" style="width:100%;max-height:560px;object-fit:contain;background:white;">')
    else:
        parts.append("<p>未找到 growth curve。</p>")
    parts.append("</div>")
    parts.append("<div class='section'><h2>Composite Overlays</h2><div class='image-grid'>")
    if overlays_dir.exists():
        imgs = sorted(overlays_dir.glob("*_composite_overlay.jpg"))
        for p in imgs:
            rel = p.relative_to(run_dir)
            parts.append(f"""
<div class="image-card">
  <img src="/files/{esc(run_id)}/{esc(str(rel))}">
  <p>{esc(p.name)}</p>
</div>
""")
    parts.append("</div></div>")
    parts.append("<div class='section'><h2>下载结果</h2><div class='downloads'>")
    if csv_path.exists():
        rel = csv_path.relative_to(run_dir)
        parts.append(f'<a class="download" href="/files/{esc(run_id)}/{esc(str(rel))}">下载 CSV</a>')
    if vigor_path.exists():
        rel = vigor_path.relative_to(run_dir)
        parts.append(f'<a class="download" href="/files/{esc(run_id)}/{esc(str(rel))}">下载 JSON</a>')
    parts.append("</div></div>")
    return pretty_html("SeedGerm Results", "\n".join(parts))


class Handler(BaseHTTPRequestHandler):
    def send_html(self, text, status=200):
        data = text.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/app-icon.png":
            data = (Path(__file__).resolve().parent.parent / "assets" / "app_icon.png").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if parsed.path == "/":
            self.send_html(index_page())
            return
        if parsed.path == "/result":
            qs = parse_qs(parsed.query)
            run_id = qs.get("run_id", [""])[0]
            if not run_id:
                self.send_html("<h1>Missing run_id</h1>", status=400)
                return
            self.send_html(result_page(run_id))
            return
        if parsed.path.startswith("/files/"):
            self.serve_file(parsed.path)
            return
        self.send_html("<h1>404 Not Found</h1>", status=404)

    def serve_file(self, path):
        rest = path[len("/files/"):]
        parts = rest.split("/", 1)
        if len(parts) != 2:
            self.send_html("<h1>Bad file path</h1>", status=400)
            return
        run_id, rel = parts[0], unquote(parts[1])
        run_dir = RUNS_DIR / run_id
        file_path = (run_dir / rel).resolve()
        if not str(file_path).startswith(str(run_dir.resolve())):
            self.send_html("<h1>Forbidden</h1>", status=403)
            return
        if not file_path.exists():
            self.send_html("<h1>File not found</h1>", status=404)
            return
        content_type = mimetypes.guess_type(str(file_path))[0] or "application/octet-stream"
        data = file_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path != "/run":
            self.send_html("<h1>404 Not Found</h1>", status=404)
            return
        content_type = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in content_type:
            self.send_html("<h1>Need multipart/form-data</h1>", status=400)
            return
        form = cgi.FieldStorage(
            fp=self.rfile,
            headers=self.headers,
            environ={
                "REQUEST_METHOD": "POST",
                "CONTENT_TYPE": self.headers.get("Content-Type"),
                "CONTENT_LENGTH": self.headers.get("Content-Length"),
            },
        )
        if "images" not in form:
            self.send_html("<h1>没有上传图片</h1>", status=400)
            return
        items = form["images"]
        if not isinstance(items, list):
            items = [items]
        run_id = datetime.now().strftime("%Y%m%d_%H%M%S")
        run_dir = RUNS_DIR / run_id
        input_dir = run_dir / "input_images"
        output_dir = run_dir / "output_pipeline"
        input_dir.mkdir(parents=True, exist_ok=True)
        output_dir.mkdir(parents=True, exist_ok=True)
        saved = 0
        for item in items:
            if not getattr(item, "filename", ""):
                continue
            filename = Path(item.filename).name
            suffix = Path(filename).suffix.lower()
            if suffix not in [".jpg", ".jpeg", ".png", ".bmp"]:
                continue
            out_path = input_dir / filename
            with open(out_path, "wb") as f:
                shutil.copyfileobj(item.file, f)
            saved += 1
        if saved == 0:
            self.send_html("<h1>没有有效图片</h1>", status=400)
            return
        log_path = run_dir / "run.log"
        cmd = (
            f"source {shlex.quote(ASCEND_ENV)} && "
            f"python3 {shlex.quote(str(PIPELINE_SCRIPT))} "
            f"--input_dir {shlex.quote(str(input_dir))} "
            f"--output_dir {shlex.quote(str(output_dir))}"
        )
        with open(log_path, "w", encoding="utf-8") as log:
            ret = subprocess.run(
                cmd,
                shell=True,
                executable="/bin/bash",
                cwd=str(ROOT),
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        if ret.returncode != 0:
            log_text = log_path.read_text(errors="ignore")
            self.send_html(f"<h1>Pipeline 运行失败</h1><pre>{html.escape(log_text)}</pre>", status=500)
            return
        self.send_response(302)
        self.send_header("Location", f"/result?run_id={run_id}")
        self.end_headers()


def main():
    print(f"SeedGerm GUI running at http://0.0.0.0:{PORT}")
    print("Open it in browser using: http://<Atlas-IP>:7860")
    server = HTTPServer((HOST, PORT), Handler)
    server.serve_forever()


if __name__ == "__main__":
    main()
