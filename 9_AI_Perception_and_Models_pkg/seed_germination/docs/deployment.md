# 完整复现操作清单（服务器 → Atlas）

## 0. 模型资源 / 原仓库

历史下载目录：`SeedGerm-VIG-1.12.zip`、`DL_models_for_rice_barley.zip`、`Image_series_rice.zip`。
原仓库初次解压只有图片和结果，不含可运行的全部脚本；模型包包含 rice/barley 两套 YOLO PT + 6 套 TF SavedModel。本包保留 rice 模型的调用接口。原始仓库地址请以你已经克隆的仓库 remote 和原 README 为准（本包不假定作者仓库的具体 GitHub URL）。

## 1. 原始服务端的确认

```bash
cd /lustre/home/acct-stu/stu702/SeedGerm-VIG
conda info --env
find _models_rice_barley -name 'saved_model.pb' -o -name '*.pt'
find example_G7 -name '*.jpg' | wc -l
```

你曾测试 `YOLOv8x-Germ_rice.pt` 单张 `G7_20221026_1637.jpg` 检测 25 个框，原始 16 帧累计 443 个检测框。`G7_20221029_2018.jpg` 服务器 ONNX segment 推理检测 26 个框。较晚帧检测数超过 25 不等于种子真正增加。

## 2. YOLO / U-Net 分开使用不同 Python 环境

| 环境 | 用途 | 既有状态 |
|---|---|---|
| seedgerm_detect | Ultralytics YOLO；ONNX 验证 | torch1.13.1+cu117；服务器上 CUDA False |
| seedgerm_segment_tf22 | TensorFlow 2.2 SavedModel、mask 批量输出 | Python 3.7；TF2.2 |

YOLO 运行命令用 `--device cpu`，只有在 GPU 节点和 CUDA 检查都通过以后改成 `--device 0`。

## 3. TF2.2 常见报错已经纳入恢复版

1. `Descriptors cannot be created directly`：protobuf 与旧 TensorFlow 不兼容；优先使用以前已安装好的 `seedgerm_segment_tf22`，不要整体升级环境。
2. `np.typeDict` 不存在：旧 scikit-image 与新 NumPy 不兼容。
3. `Resource ... conv2d/bias ... does not exist`：保留 loaded SavedModel 引用，在整批推理结束前不要释放模型。
4. `TypeError int(NoneType)`：overlay 颜色不能为 None；恢复版固定三类 BGR 颜色。
5. `tf2onnx _DictWrapper`：旧 TF2.2 SavedModel 直接 `tf2onnx --saved-model` 报错；恢复版用 Frozen Graph 路线。

## 4. ONNX 验证与转换

server: `export_yolo_rice_to_onnx.py` 导出 `output0 [1,40,6720]` 和 `output1 [1,32,128,160]` 的 segmentation 模型。不能当成 detect-only 模型使用：Ultralytics 加载 ONNX 必须显式 `task='segment'`。

server: `export_unet_rice_to_onnx_freeze.py` 导出三个 ONNX，每个预期 `[1,512,512,3] -> [1,512,512,2]`，在 Atlas 编译用 `--input_format=ND`（ATC 不接受 ONNX 的 `--input_format=NHWC`）。

Atlas: `bash atc_convert_unets.sh`，SoC `Ascend310B1`。历史上成功生成 3 个 U-Net `.om`。

## 5. YOLO ONNX / OM 排错经过

- YOLO ONNX Runtime 输出 TOP 类别置信度接近 `0.90`。
- Atlas YOLO OM 常规转换输出 TOP 类别置信度仅 `0.38`，`>=0.25` 的候选数 ONNX 为 174、OM 为 2；**输入数组差异为零**。
- `precision_mode_v2=origin` 编译失败：MaxPoolV3 FP32 在当时 310B1 算子实现上不受支持。
- `cube_fp16in_fp32out`、`mixed_float16 + high_precision_for_all`、`fp16 + high_precision_for_all` 均未明显改善 raw output。
- 因为已有 U-Net 单独能够稳定生成 25 个 seed 连通域及后续 mask，最终采用 **U-Net + CV** 路线，弃用 YOLO OM 作为正式阶段检测来源。
- **注意**：数值偏移定位于转换/运行链路；不能在没有逐层对齐实验时断言唯一根因必然是 FP16，可能涉及算子选择、图重写、布局或融合精度行为。

## 6. 最终 GUI 与输出

在 Atlas 根目录启动 `bash start_gui.sh`，浏览器上传 `example_G7` 时序图；GUI 会调用 `run_seedgerm_unet_cv_pipeline.py`，并从 `gui_runs/<run_id>/output_pipeline/` 加载：

```text
masks/*_overall_mask.png
masks/*_seed_mask.png
masks/*_seedling_mask.png
masks/*_seedling_skeleton_512.png
overlays/*_overall_overlay.jpg
overlays/*_seed_overlay.jpg
overlays/*_seedling_overlay.jpg
overlays/*_composite_overlay.jpg
summary/image_trait_summary.csv
summary/image_trait_summary_detail.json
summary/vigor_score.json
summary/growth_curve_skeleton_length.png
```

代码不含真实模型权重；需保留原版 3 个 OM 或重新转换。GUI 是仅适合可信局域网的实验性开发服务器。
