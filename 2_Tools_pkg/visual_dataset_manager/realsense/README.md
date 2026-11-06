# RealSense 图像采集与标注工具 — 操作手册

> **版本**：v1.1  
> **最后更新**：2025-05-05  
> **运行模式**：支持「模拟模式」（无需相机）与「RealSense 真机模式」自由切换

---

## 目录

1. [工具简介与特性](#1-工具简介与特性)
2. [环境要求](#2-环境要求)
3. [安装步骤](#3-安装步骤)
4. [快速开始（模拟模式，无需相机）](#4-快速开始模拟模式无需相机)
5. [连接 RealSense 相机（真机模式）](#5-连接-realsense-相机真机模式)
6. [界面总览](#6-界面总览)
7. [图像采集](#7-图像采集)
8. [图像标注](#8-图像标注)
9. [数据集管理与导出](#9-数据集管理与导出)
10. [数据集目录结构说明](#10-数据集目录结构说明)
11. [配置文件说明（config.py）](#11-配置文件说明configpy)
12. [常见问题](#12-常见问题)
13. [快捷键速查](#13-快捷键速查)
14. [项目结构说明](#14-项目结构说明)

---

## 1. 工具简介与特性

本工具基于 **Intel RealSense D4xx 系列相机**，提供从数据采集到标注导出的完整工作流，
可直接生成 YOLOv5/v8 或 PASCAL VOC 格式数据集，用于目标检测模型训练。

### 核心功能

| 功能 | 说明 |
|------|------|
| 实时采集 | 实时预览彩色图与深度图（伪彩色），支持单张拍摄和定时连拍 |
| 深度图保存 | 以 16-bit PNG 保存原始深度值（单位 mm），与彩色图一一对应 |
| 任务管理 | 按场景建立独立任务文件夹，自动维护图像索引 |
| 图像标注 | 鼠标拖拽绘制目标框，支持多类别、修改、删除，实时显示 |
| 格式导出 | 支持 YOLO（含 dataset.yaml）和 PASCAL VOC 格式，自动划分 train/val |
| 模拟模式 | 无 RealSense 设备时自动生成动态测试帧，所有功能均可正常演示 |
| 设置持久化 | 类别列表、上次任务、导出格式等自动保存，下次启动自动恢复 |

---

## 2. 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows 10/11、Ubuntu 20.04+、macOS 12+ |
| Python | **3.8 或更高版本**（推荐 3.10+） |
| USB 接口 | USB 3.0（RealSense D4xx 真机必须；蓝色内芯接口） |
| 内存 | 建议 8 GB 以上 |
| Intel RealSense SDK | 2.54+（真机模式必须；模拟模式无需安装） |

---

## 3. 安装步骤

### 3.1 解压项目

```bash
cd realsense_tool
```

### 3.2 安装 Python 依赖

**模拟模式（无相机演示）——三个包即可：**

```bash
pip install -r requirements_mock.txt
```

**真机模式（含 RealSense SDK）：**

```bash
pip install -r requirements.txt
```

> 若 `pyrealsense2` 安装失败，请先安装 Intel RealSense SDK，
> 或在 `requirements.txt` 中注释掉该行，并在 `config.py` 保持 `USE_MOCK = True`。

### 3.3 安装 Intel RealSense SDK（仅真机模式需要）

**Windows：**

1. 前往 [Intel RealSense GitHub Releases](https://github.com/IntelRealSense/librealsense/releases)
   下载最新 `Intel.RealSense.SDK-WIN10-<version>.exe`
2. 双击安装，全程默认选项
3. 安装完成后插入相机，设备管理器中应出现「Intel RealSense D4xx」设备

**Ubuntu：**

```bash
sudo apt-key adv --keyserver keys.gnupg.net \
  --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCD

sudo add-apt-repository \
  "deb https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main"

sudo apt update
sudo apt install librealsense2-dkms librealsense2-utils librealsense2-dev

# 验证安装（应出现相机预览画面）
realsense-viewer
```

### 3.4 验证安装

```bash
python -c "import cv2; print('OpenCV:', cv2.__version__)"
python -c "from PyQt5.QtWidgets import QApplication; print('PyQt5 OK')"
# 仅真机模式需验证：
python -c "import pyrealsense2; print('pyrealsense2 OK')"
```

---

## 4. 快速开始（模拟模式，无需相机）

无需连接任何设备，直接运行：

```bash
python main.py
```

启动后：
- 顶部状态栏显示 **「模式：模拟相机」**
- 采集界面点击 **「▶ 启动相机」** 即可看到动态彩色测试画面
- 所有采集、标注、导出功能与真机完全一致

> **模拟帧说明**：模拟相机自动生成渐变背景 + 4 个彩色矩形色块，
> 并在左上角叠加 `[MOCK]` 标记与时间戳，方便区分。
>
> 也可在 `config.py` 中设置 `MOCK_IMAGE_FOLDER = "你的图片文件夹路径"`，
> 程序将循环播放该文件夹内的图片作为模拟源。

---

## 5. 连接 RealSense 相机（真机模式）

### 5.1 硬件连接

1. 使用相机随附的 **USB 3.0 线缆**（接口内芯为蓝色）连接相机与计算机
2. 等待系统自动识别（Windows 约 10 秒，Ubuntu 约 5 秒）

### 5.2 切换到真机模式

编辑项目根目录的 `config.py`，将第 7 行改为：

```python
USE_MOCK = False   # 改为 False 启用真机
```

保存后重新运行 `python main.py`。

### 5.3 验证连接

- 状态栏显示 **「模式：RealSense 真机」**
- 采集界面点击「启动相机」后，控制台日志输出设备型号、序列号和固件版本
- 预览区实时显示相机画面

---

## 6. 界面总览

程序分为三个功能选项卡，通过顶部 Tab 切换，也可用快捷键 `Ctrl+1/2/3`。

```
┌─────────────────────────────────────────────────────────────────┐
│  文件(F)   视图(V)   帮助(H)                                     │
├─────────────────────────────────────────────────────────────────┤
│  📷 图像采集  │  🏷 图像标注  │  🗂 数据集管理                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│                      各选项卡内容区域                            │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│  模式：模拟相机  |  数据集根目录：/path/to/datasets              │
└─────────────────────────────────────────────────────────────────┘
```

**三个选项卡功能定位：**

| 选项卡 | 功能 |
|--------|------|
| 📷 图像采集 | 启动相机、实时预览、新建任务、拍摄保存 |
| 🏷 图像标注 | 浏览已采集图像、绘制标注框、保存标注 |
| 🗂 数据集管理 | 浏览/删除数据、统计信息、导出数据集 |

---

## 7. 图像采集

### 7.1 新建采集任务

**任务**是数据的组织单元，对应一个采集场景（如「室内物体」、「停车场车辆」）。

1. 在右侧面板「任务 / 场景」区域的输入框中**填写任务名称**
2. 点击 **「＋ 新建」** 按钮

程序自动在 `datasets/<任务名>/` 下创建：

```
datasets/
  └── 室内物体/
        ├── index.json   ← 图像索引（自动维护，勿手动修改）
        ├── images/      ← 彩色图（JPEG）
        ├── depth/       ← 深度图（16-bit PNG）
        └── labels/      ← 标注文件（YOLO .txt）
```

> 任务名称中的特殊字符（`/ \ : * ? " < > |`）会自动替换为下划线。

### 7.2 启动相机

选择任务后，点击 **「▶ 启动相机」** 按钮：
- 预览区开始显示实时画面
- 底部状态栏显示实时帧率
- 勾选 **「显示深度图（伪彩色）」** 可切换深度可视化视图（JET 色图）

### 7.3 采集图像

| 方式 | 操作 |
|------|------|
| **单张拍摄** | 点击「📷 拍摄」按钮，或按 **空格键** |
| **定时连拍** | 设置间隔秒数（1~60s），点击「🔄 连拍」；再次点击停止 |

- 勾选 **「同时保存深度图」**（默认开启）时，每张彩色图同步保存对应深度图
- 每次拍摄后「已采集」计数器自动更新

### 7.4 深度图格式说明

深度图以 **16-bit PNG** 保存，**像素值单位为毫米（mm）**。

| 像素值 | 含义 |
|--------|------|
| 0 | 无效深度（超出量程、玻璃/镜面反射） |
| 500 | 距相机 500 mm（0.5 m） |
| 3000 | 距相机 3000 mm（3 m） |
| 65535 | 量程上限或无效 |

> 读取深度图时需用 `cv2.imread(path, cv2.IMREAD_ANYDEPTH)` 保持 16-bit 精度。

---

## 8. 图像标注

切换到 **「🏷 图像标注」** 选项卡（或按 `Ctrl+2`）。

### 8.1 界面布局

```
┌──────────────┬────────────────────────────┬──────────────┐
│  任务选择    │                            │  类别管理    │
│  图像列表    │     标  注  画  布          │  标注框列表  │
│  ← / → 翻页 │   （在此拖拽绘制目标框）    │            │
└──────────────┴────────────────────────────┴──────────────┘
      [💾 保存标注（S）]       [🗑 清空所有框]
```

### 8.2 选择图像

1. 左上角下拉框选择任务
2. 左侧图像列表单击任意图像，画布立即加载

**图像状态标记：**
- ✅ 绿色字体：已保存标注
- ○ 白色字体：未标注

### 8.3 绘制标注框

1. 右侧「当前类别」下拉框中选择目标类别
2. 在图像上**左键拖拽**画出矩形框
3. 松开鼠标后，框自动添加到右侧列表

> 画布自动等比缩放图像，坐标会精确换算回原始图像分辨率再保存。

### 8.4 选中与修改标注框

| 操作 | 效果 |
|------|------|
| 左键单击已有框 | 选中（框变粗，显示 [✓] 标记） |
| 选中后修改右侧类别下拉框 | 修改该框的类别 |
| 右键点击画布 | 取消选中 |
| **Del 键** | 删除选中框 |
| 右侧框列表点击 | 选中对应框并高亮 |

### 8.5 保存标注

按 **S 键** 或点击 **「💾 保存标注」** 按钮。

保存路径：`datasets/<任务名>/labels/<图像名>.txt`

**YOLO 标注格式（每行一个目标）：**

```
<类别ID> <中心x> <中心y> <宽> <高>
```

所有坐标为归一化值（0~1），相对于图像宽高。示例：`0 0.514 0.372 0.210 0.186`

### 8.6 类别管理

在右侧「当前类别」面板：
- **添加类别**：在输入框填写名称 → 点击「＋」
- **删除类别**：选中类别 → 点击「删除当前类别」（至少保留一个）

> 类别列表自动持久化到 `settings.json`，下次启动自动恢复。
> ⚠️ 类别 ID 与列表顺序绑定，**不要随意删除或调换顺序**，否则已有标注 ID 会错位。

### 8.7 图像导航

| 操作 | 方式 |
|------|------|
| 上一张 | 点击「◀ 上一张」或按 **← 键** |
| 下一张 | 点击「下一张 ▶」或按 **→ 键** |
| 跳转 | 直接点击左侧图像列表 |

### 8.8 键盘微调标注框

选中任意标注框后，可使用键盘方向键进行像素级微调，无需鼠标反复拖拽：

| 快捷键 | 功能 |
|--------|------|
| ↑ ↓ ← → | 1 像素微调框位置 |
| Shift + ↑ ↓ ← → | 10 像素快速移动 |

> 微调时自动进行图像边界保护，框不会移出画布。此功能在需要精确定位目标边缘时非常有用。

### 8.9 自动保存（可选）

勾选界面中「✓ 自动保存标注」后，每次增删改标注框都会**自动写入** `.txt` 文件，无需手动按 S 保存。适合快速连续标注场景。

> 若需撤销误操作，可在自动保存前关闭该开关，或直接使用「🗑 清空所有框」。

---

## 9. 数据集管理与导出

切换到 **「🗂 数据集管理」** 选项卡（或按 `Ctrl+3`）。

### 9.1 浏览数据集

左侧树形列表展示所有任务及其图像：
- 点击任务节点 → 右侧显示统计（总数/已标注/未标注）
- 点击图像节点 → 中央预览图像（带标注框叠加显示）

### 9.2 删除数据

1. 在树形列表中**选中**图像或任务节点
2. 点击 **「🗑 删除」** 按钮 → 在确认对话框点击「是」

> ⚠️ 删除不可撤销，会同时删除图像文件、深度图和标注文件。

### 9.3 导出数据集

在右侧「导出数据集」面板：

**第一步：选择格式**

| 格式 | 适用场景 | 输出文件 |
|------|----------|----------|
| YOLO（YOLOv5/v8） | Ultralytics YOLO 系列训练 | `.txt` 标注 + `dataset.yaml` + `classes.txt` |
| PASCAL VOC | MMDetection 等传统框架 | `.xml` 标注 + ImageSets 划分文件 |

**第二步：设置训练集比例**（默认 0.80，即 80% 训练 / 20% 验证）

**第三步：选择任务** → 点击 **「📦 选择路径并导出」** → 选择输出目录

---

## 10. 数据集目录结构说明

### 10.1 原始存储结构（采集后）

```
datasets/
  └── <任务名>/
        ├── index.json
        ├── images/
        │     └── img_20250101_120000_123456.jpg
        ├── depth/
        │     └── dep_20250101_120000_123456.png  ← 16-bit PNG
        └── labels/
              └── img_20250101_120000_123456.txt  ← YOLO 格式
```

### 10.2 YOLO 格式导出结构

```
<输出目录>/<任务名>_export/
  ├── dataset.yaml        ← train/val 路径、类别数、类别名称
  ├── classes.txt
  ├── images/
  │     ├── train/
  │     └── val/
  └── labels/
        ├── train/
        └── val/
```

`dataset.yaml` 示例：
```yaml
path: /absolute/path/to/export
train: images/train
val:   images/val
nc: 5
names: ['object', 'person', 'car', 'bottle', 'chair']
```

### 10.3 PASCAL VOC 格式导出结构

```
<输出目录>/<任务名>_export/
  ├── JPEGImages/
  ├── Annotations/
  └── ImageSets/Main/
        ├── train.txt
        ├── val.txt
        └── trainval.txt
```

---

## 11. 配置文件说明（config.py）

```python
# 关键开关：切换真机/模拟只需改这一行
USE_MOCK = True          # True=模拟相机  False=RealSense 真机

# 模拟相机数据源
# 留空 → 自动生成动态测试帧（渐变背景+彩色色块）
# 填路径 → 循环播放该文件夹中的图片（jpg/png/bmp/webp）
MOCK_IMAGE_FOLDER = ""

# 相机参数（真机需与设备支持规格一致）
CAMERA_WIDTH  = 640
CAMERA_HEIGHT = 480
CAMERA_FPS    = 30
# D435i 常用分辨率：640×480、848×480、1280×720（@30fps）

# 数据集存储根目录（相对路径）
OUTPUT_DIR = "datasets"

# 初始类别（运行中修改会自动保存到 settings.json）
DEFAULT_CLASSES = ["object", "person", "car", "bottle", "chair"]
```

---

## 12. 常见问题

**Q：点击「启动相机」弹出「相机启动失败」**
- 确认 USB 接口为 USB 3.0（内芯蓝色）
- 更换 USB 端口，避免使用 USB Hub 或延长线
- Windows：设备管理器确认出现「Intel RealSense D4xx」设备
- Ubuntu：运行 `rs-enumerate-devices` 应输出设备信息
- 确认 `CAMERA_WIDTH/HEIGHT/FPS` 与设备支持规格一致

**Q：启动报 `ImportError: No module named 'pyrealsense2'`**
- 若 `USE_MOCK = True`：不需要此库，执行 `pip install -r requirements_mock.txt`
- 若 `USE_MOCK = False`：先安装 Intel RealSense SDK，再执行 `pip install pyrealsense2`

**Q：预览画面卡顿/帧率极低**
- 确认使用 USB 3.0 接口
- 将分辨率降为 640×480
- 关闭其他占用 USB 带宽的设备

**Q：标注文件保存在哪里？**
- 路径：`datasets/<任务名>/labels/<图像名>.txt`

**Q：类别顺序如何保证一致？**
- 类别 ID 与列表位置绑定（第 0 个 = ID 0）
- 导出时会生成 `classes.txt`，与 `.txt` 标注中的 ID 严格对应
- 不要随意删除或调换已有类别的顺序

**Q：导出的 YOLO 数据集如何用 YOLOv8 训练？**
```bash
pip install ultralytics
yolo detect train data=/path/to/dataset.yaml model=yolov8n.pt epochs=100
```

**Q：深度图如何读取？**
```python
import cv2
depth = cv2.imread("dep_xxx.png", cv2.IMREAD_ANYDEPTH)  # uint16，单位 mm
distance_m = depth[y, x] / 1000.0
```

**Q：程序崩溃如何查看日志？**
- 日志文件路径：`logs/app.log`

**Q：点击图像列表提示「图像文件不存在」**
- 请使用英文路径。检查程序对 `datasets/` 目录是否有**写入权限**（Windows：右键→属性→安全；Linux：`chmod 755`）
- 检查磁盘空间是否充足
- 查看 `logs/app.log` 中是否有 `cv2.imwrite failed` 错误，确认 OpenCV 图像编码是否正常
- 若使用模拟模式，确认 `USE_MOCK = True` 且相机预览已正常显示后再按空格拍摄

---

## 13. 快捷键速查

### 采集界面
| 快捷键 | 功能 |
|--------|------|
| **空格** | 拍摄一张 |

### 标注界面
| 快捷键 | 功能 |
|--------|------|
| 左键拖拽 | 绘制新标注框 |
| 左键单击已有框 | 选中该框 |
| 右键 | 取消选中 |
| **Del** | 删除选中框 |
| **S** | 保存当前图像标注 |
| **←** | 上一张图像 |
| **→** | 下一张图像 |
| **↑ ↓ ← →** | 微调选中框位置（1px） |
| **Shift + 方向键** | 快速移动选中框（10px） |

### 全局
| 快捷键 | 功能 |
|--------|------|
| **Ctrl+1** | 切换到采集界面 |
| **Ctrl+2** | 切换到标注界面 |
| **Ctrl+3** | 切换到数据集管理 |
| **Ctrl+Q** | 退出程序 |

---

## 14. 项目结构说明

```
realsense_tool/
├── main.py                  ← 程序入口，Dark 主题样式表
├── config.py                ← 全局配置（切换真机/模拟只改这里）
├── requirements.txt         ← 完整依赖（含 pyrealsense2）
├── requirements_mock.txt    ← 模拟模式专用（3个包，无需相机）
│
├── camera/
│   ├── __init__.py          ← 工厂函数 create_camera()
│   ├── base_camera.py       ← 抽象基类，定义统一接口
│   ├── mock_camera.py       ← 模拟相机（动态帧/图片循环）
│   └── realsense_camera.py  ← RealSense D4xx 真机实现
│
├── core/
│   ├── dataset_manager.py   ← 任务/图像增删查，维护 index.json
│   ├── exporter.py          ← YOLO / VOC 格式导出
│   ├── settings.py          ← 设置持久化（load/save/get/set）
│   └── logger.py            ← 统一日志（控制台+文件）
│
├── ui/
│   ├── main_window.py       ← 主窗口（菜单+三Tab+状态栏）
│   ├── capture_widget.py    ← 采集界面
│   ├── annotate_widget.py   ← 标注界面（含 AnnotationCanvas 画布）
│   └── dataset_widget.py    ← 数据集管理界面
│
└── tests/
    ├── test_mock_camera.py
    ├── test_exporter.py
    ├── test_dataset_manager.py
    └── test_bbox.py
```

**核心设计亮点：**
- `camera/` 层以抽象基类隔离真机与模拟，上层代码零修改即可切换
- `core/settings.py` 持久化用户偏好，跨会话记忆上次状态
- 采集→标注→管理三模块通过 Qt 信号解耦，数据自动同步

---

## 15. 系统架构与设计模式

本工具采用分层架构，核心设计模式如下：

### 15.1 工厂模式（Factory Pattern）

`camera/__init__.py` 中的 `create_camera()` 根据 `config.USE_MOCK` 自动决定实例化 `MockCamera`（模拟）或 `RealSenseCamera`（真机）。上层业务代码完全解耦，切换模式仅需修改一行配置，无需改动任何业务逻辑。

### 15.2 观察者模式（Observer Pattern）

基于 Qt 信号-槽机制实现模块间零耦合通信：
- `CaptureWidget.image_saved` → `AnnotateWidget.on_image_saved`（采集后标注列表自动刷新）
- `CaptureWidget.image_saved` → `DatasetWidget.on_image_saved`（数据集统计实时更新）

### 15.3 MVC 分离

| 层级 | 职责 | 对应文件 |
|------|------|----------|
| Model | 数据持久化、文件 IO、格式转换 | `core/dataset_manager.py`, `core/exporter.py` |
| View | 界面渲染、用户交互 | `ui/*.py` |
| Controller | 模块协调、全局状态管理 | `ui/main_window.py` |

---

## 16. 无相机演示专项指南（课程答辩版）

### 16.1 为什么无相机也能完整演示？

工具内置 **MockCamera** 模块，通过抽象基类 `BaseCamera` 实现接口隔离。MockCamera 能够：

1. **实时生成动态测试帧**：渐变背景 + 4 个移动色块 + 时间戳水印 `[MOCK]`
2. **生成对应深度图**：色块区域模拟不同距离的深度值（800 mm ~ 2000 mm），支持伪彩色预览
3. **循环播放本地图片**：可指定文件夹模拟真实场景

### 16.2 演示前检查清单

| 检查项 | 操作 |
|--------|------|
| 切换模拟模式 | 确认 `config.py` 第 7 行 `USE_MOCK = True` |
| 安装依赖 | `pip install -r requirements_mock.txt` |
| 清理旧数据 | 删除 `datasets/` 目录（可选，确保演示环境干净） |
| 测试快捷键 | 空格拍照、S 保存、Del 删除、方向键微调框 |
| 验证导出 | 提前运行一次导出，确认 `dataset.yaml` 生成正常 |

### 16.3 向老师展示「真机接口保留」的方法

在答辩中现场演示 **一行代码切换真机**：

1. 打开 `config.py`
2. 将 `USE_MOCK = True` 改为 `False`
3. 展示 `camera/realsense_camera.py` 中的真机实现类
4. 说明：「当前环境无 USB 相机，故使用 Mock 模式；接入真机后无需修改任何业务代码」

---

## 17. 单元测试运行指南

项目包含 4 组测试，答辩前运行可展示代码质量：

```bash
# 安装 pytest
pip install pytest

# 运行全部测试
python -m pytest tests/ -v
---

*如有问题，请查看 `logs/app.log` 获取详细错误信息。*
