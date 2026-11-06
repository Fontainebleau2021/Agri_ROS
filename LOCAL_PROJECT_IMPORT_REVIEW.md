# 五项目导入审查

命名统一为小写snake_case项目目录；既有仓库分类不改。保留中文产品名用于界面。Python模块、第三方目录/API名不机械重命名。人名/学号外层目录不导入，不抹除源码内作者声明或第三方许可。

|项目|目录|现状|
|---|---|---|
|多传感器标定|3_Calibration_pkg/sensor_calibration|Qt图标接入，ROS1/2包装器，需目标环境验证|
|可视化平台|2_Tools_pkg/visual_data_platform|恢复Electron/Vue源码，窗口/About/网页图标已接；完整构建阻塞|
|视觉数据集管理|2_Tools_pkg/visual_dataset_manager|保留RealSense与Azure Kinect两个不同实现，不当重复删掉；Qt图标接入|
|种子萌发|9_AI_Perception_and_Models_pkg/seed_germination|使用此前解压恢复包；其README明确部分源码为恢复重建，不能冒充逐字原始项目；网页favicon接入|
|番茄识别|9_AI_Perception_and_Models_pkg/tomato_ripeness|只有品牌占位，缺源码，EXE内图标尚未替换|

已排除node_modules/运行解释器、构建输出、缓存、视频、模型权重、大数据及原始压缩包；这些仍在原盘，未破坏原资料。26个“(1)”副本逐字SHA256相同才去重。可视化目录中混入的标定项目整体不重复导入。

重要缺口：番茄EXE源码缺失；可视化package.json未含完整devDependencies/构建scripts，内嵌Python运行环境及label工具需另行可复现依赖设计，不能声称源码提取后即可运行；部分第三方发布库需核对许可证。SeedGerm需Ascend/权重及原始源码真实性确认。所有GUI在当前机器尚未运行验收。种子logo1作默认，logo2保留备选。现有源码内授权/激活逻辑未删除或绕过。

本候选是独立桌面/工具软件目录导入，不承诺已集成ROS话题或ROS Noetic可构建；保留原仓库现有功能。
