# SeedGerm-VIG → Huawei Atlas 310B1：代码恢复包

**性质：**依据既有聊天记录、终端日志以及原来保存的 GUI 包重建。不是作者完整 GitHub 源码；不是从你的机器提取的逐字原件。模型权重/数据集/生成的 OM 不随包提供。`atlas/app_seedgerm_gui.py` 来自此前生成的 GUI 提交包，其他脚本依据过去的实现重写或恢复逻辑。该包完成了 Python 语法检查和脱离硬件的模拟烟雾测试，不等于已在你的 Atlas 或 SJTU 服务器重新实测。

## 一、历史部署事实

- 原始项目：**SeedGerm-VIG**（论文/仓库请见原项目 README），本次使用 **rice（水稻）** 模型。
- SJTU 服务器目录：`/lustre/home/acct-stu/stu702/SeedGerm-VIG`。
- 最初检测环境：`seedgerm_detect`，Python 3.8.20、torch 1.13.1+cu117、Ultralytics 8.0.145；当时 `torch.cuda.is_available() == False`，故在 CPU 上跑通。**这不是一个已证实的 CUDA GPU 运行记录**。
- U-Net 环境：`seedgerm_segment_tf22`，TF 2.2 / Python 3.7；旧版 SavedModel 加载，避免资源变量过早销毁；导出需要先 freeze 再 tf2onnx。
- Atlas：Ascend **310B1**；ATC `--soc_version=Ascend310B1`；OM 推理使用 `ais_bench`。
- 最终可用方案：3 个 rice U-Net OM + OpenCV 后处理 + 不依赖 Flask 的网页 GUI。**YOLO OM 未通过精度验证，不属于最终流程。**

## 二、包内容

```text
SeedGerm_VIG_Recovery/
├── server/
│   ├── seedgerm_common.py                通用路径、图像预处理
│   ├── test_yolo_rice_cpu.py              单图 YOLO，--device cpu/0
│   ├── batch_yolo_rice_cpu.py             批量 YOLO，batch_boxes.csv
│   ├── crop_yolo_rois.py                 按 YOLO 框裁 ROI
│   ├── inspect_savedmodels.py            查看 3 个 U-Net 签名
│   ├── test_unet_rice_one.py             单张 TF U-Net
│   ├── batch_unet_rice_full.py           16 张 TF U-Net + mask/overlay/summary.csv
│   ├── extract_traits_from_masks.py      从 YOLO ROI + U-Net mask 提取表型
│   ├── plot_traits_baseline.py           表型趋势图
│   ├── advanced_seedgerm_postprocess.py  固定种子 ID、根尖近似、探索性评分
│   ├── export_yolo_rice_to_onnx.py       PyTorch -> ONNX（segment）
│   ├── test_yolo_rect_onnx_segment.py    正确 ONNX task=segment 测试
│   ├── export_unet_rice_to_onnx_freeze.py TF2.2 freeze -> ONNX
│   ├── fix_unet_onnx_for_atc.py          ATC 兼容修复
│   ├── test_unet_onnx_infer.py           ONNX U-Net 推理
│   ├── save_yolo_onnx_raw_server.py      服务器 raw output
│   ├── run_yolo_on_cuda.sh               CUDA 节点的检测入口
│   └── run_server_pipeline.sh           已有 conda 环境顺序执行
├── atlas/
│   ├── run_seedgerm_unet_cv_pipeline.py  **最终板端主程序**
│   ├── run_unet_om_real_image.py         单图真实 OM 测试
│   ├── app_seedgerm_gui.py         GUI（前一版本恢复）
│   ├── save_yolo_om_raw_atlas.py         OM raw output
│   ├── compare_yolo_raw_outputs.py      与 ONNX 数值对照
│   ├── atc_convert_unets.sh             ONNX -> OM
│   ├── atc_convert_yolo_DIAGNOSTIC.sh   YOLO 转换（仅留排错）
│   └── start_gui.sh                     启动网页
├── tests/
│   └── test_atlas_pipeline_offline.py   离线虚拟模型测试
└── docs/
    └── deployment.md
```

## 三、服务器：代码放在哪里

**先备份现有项目**，切勿无提示覆盖你已找回的其他文件。在服务器项目根目录执行：

```bash
cd /lustre/home/acct-stu/stu702/SeedGerm-VIG
mkdir -p recovery_backup
# 将压缩包里的 server/*.py、server/*.sh 拷贝到此目录；不要拷贝到模型目录
export SEEDGERM_ROOT="$PWD"
```

模型仍使用以前下载并解压的目录：

```text
_models_rice_barley/models_rice_barley/YOLOv8x-Germ_rice.pt
_models_rice_barley/models_rice_barley/U-net_rice_overall_detection/saved_model.pb
_models_rice_barley/models_rice_barley/U-net_rice_seed_detection/saved_model.pb
_models_rice_barley/models_rice_barley/U-net_rice_seedling_detection/saved_model.pb
example_G7/*.jpg
```

### 在旧 CPU 环境复现（已知能运行的路线）

```bash
conda activate seedgerm_detect
python test_yolo_rice_cpu.py --device cpu
python batch_yolo_rice_cpu.py --device cpu
python crop_yolo_rois.py

conda activate seedgerm_segment_tf22
export CUDA_VISIBLE_DEVICES=""
python inspect_savedmodels.py
python test_unet_rice_one.py
python batch_unet_rice_full.py
python extract_traits_from_masks.py
python plot_traits_baseline.py
python advanced_seedgerm_postprocess.py
```

### 用 NVIDIA CUDA GPU（条件：节点确实有 GPU，且 PyTorch/CUDA 正常）

```bash
conda activate seedgerm_detect
python -c 'import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else "no GPU")'
# True 才运行：
python batch_yolo_rice_cpu.py --device 0
```

`torch 1.13.1+cu117` 只意味着安装的是含 CUDA 支持的 torch 构建；不保证 GPU 可用。TensorFlow 2.2 若需在 GPU 上运行，必须在兼容 CUDA/cuDNN 的 GPU 节点上检查 `tf.config.list_physical_devices('GPU')`，否则应保持原 CPU 流程。**请勿为了跑脚本直接覆盖原 conda 环境或升级旧 TF2.2 的 NumPy/Protobuf。**

### 导出 ONNX（不同环境）

```bash
conda activate seedgerm_detect
python export_yolo_rice_to_onnx.py --height 512 --width 640 --device cpu
python test_yolo_rect_onnx_segment.py

conda activate seedgerm_segment_tf22
export CUDA_VISIBLE_DEVICES=""
python export_unet_rice_to_onnx_freeze.py
# 后面两个可在安装了 onnx / onnxruntime 的 seedgerm_detect 环境运行：
conda activate seedgerm_detect
python fix_unet_onnx_for_atc.py
python test_unet_onnx_infer.py
```

**注意：**旧 ATC 曾因 ONNX 多 opset/domain 声明报 E16005。本包修复脚本仅在图中不含自定义域算子时规范化 opset，避免无条件删掉必需依赖。转换通过不代表数值一致，需要实际推理比较。

## 四、Atlas：代码放哪里

将 `atlas/*.py`、`atlas/*.sh` 放到 **`/root/seedgerm_atlas/` 根目录**；保留之前已经转好的 OM：

```text
/root/seedgerm_atlas/models_om/unet_rice_overall_bs1_512.om
/root/seedgerm_atlas/models_om/unet_rice_seed_bs1_512.om
/root/seedgerm_atlas/models_om/unet_rice_seedling_bs1_512.om
/root/seedgerm_atlas/input_images/G7_*.jpg
```

运行：

```bash
cd /root/seedgerm_atlas
source /usr/local/Ascend/ascend-toolkit/set_env.sh
python3 run_unet_om_real_image.py
python3 run_seedgerm_unet_cv_pipeline.py --input_dir input_images --output_dir output_pipeline
cat output_pipeline/summary/vigor_score.json
bash start_gui.sh
```

电脑浏览器打开：`http://192.168.137.100:7860`（以前使用的板子 IP；IP 如果已变更，请 `hostname -I` 查询）。访问 GUI 的设备必须能连到板子网络。**请只在可信局域网内运行这个开发用 HTTP 服务，不要对互联网公开。**

## 五、历史测试结果作为比对基准

之前在 `example_G7` 的 16 张图上得到：

- 第一帧至最后一帧：0 至 **75.6833 h**。
- `seed_count`：每张均为 **25**（512×512 的 seed mask 连通域）。
- `seedling_area_512`：**0 → 1448**。
- `seedling_skeleton_length_512`：**0 → 285**。
- 第一次 seedling 出现：**45.2167 h**。
- 旧演示公式计算的 `batch_vigor_score`：**62.785**。

恢复版采用同类预处理/阈值/评分公式，但 OpenCV/skimage/Ascend 的版本差异、细化算法差异可能导致数值变化。以复测为准。

**评分限制：**`62.785` 是自行定义的演示基线分数，未经生物学标签标定，也不等于“62.8% 活力”或中等活力。`seedling_component_count=30` 并不意味着 30 粒种子已经萌发；它可以包含同株多个碎片，所以 **completion_score=100 不是经验证的萌发率**。任何品种比较都应先有标注/实验测量来校准公式。

## 六、GPU 训练说明

此次工作主要是**现有权重的部署与后处理**，并未依据聊天记录实际训练原始 YOLO / U-Net 模型。所以这个恢复包提供推理代码、导出和部署，不伪造权重训练脚本或训练日志。若将来希望在标注的 SeedVig-phase / SeedVig-traits 上微调或重新训练，需要另行准备标签定义、数据划分和对应训练配置。

