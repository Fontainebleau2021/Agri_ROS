# struct_icp (PCD pipeline) 全量审计报告

> 说明：以下结论仅基于源码实现，不做函数名推测。每个关键结论都附路径与行号范围或≤30行代码片段。

## 【1】工程入口与完整数据流

### 1.1 主入口（可执行文件、main、CLI）
- **run_sequence** 是 PCD pipeline 的主入口，CLI 解析与默认参数在 `src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp`（`main` 从第123行开始，默认值与参数写入在 132–260 行，解析在 262 行以后）。(src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:123-360)
- **demo_pcd_icp** 是单对 PCD 对齐 demo（读两个 PCD→预处理→构建 map→单次 Align），入口在 `main`。 (src/agri_icp/cpp/struct_icp/examples/demo_pcd_icp.cpp:47-133)
- **PrintUsage 列表中未包含 `--profile`**（当前版本没有 profile 选项逻辑）。(src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:7-118)

### 1.2 一帧调用链（实际顺序）
- **frame0 初始化**：加载→预处理→`LocalMap::InitWithFrame` 同步建图→写起始轨迹（不跑 ICP）。(src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:180-224)
- **后续帧**：Load → Preprocess → (async map swap/wait) → **选择 query map** → corr build → solve/early-stop → map insert/rebuild → log。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:226-738)
- **对齐点 fallback**：若 split 模式的 icp_points 太少（<min_effective_corr），当前帧改用 map_points 做对齐点集。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:253-260)
- **ICP 跳过门禁**：若 map 体素数或源点不足，则本帧 skip ICP（不直接失败）。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:333-348)
- **corr build 内部**：按开关走 `coarse assoc` / `ring-query` / `edge-first` / `score-filter` 的不同分支，构建 corrs 列表→（可选）预算截断→进入求解。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:199-905)

**Mermaid 调用链（含 coarse/refine/fallback 分支）**
```mermaid
flowchart TD
  A[run_sequence main] --> B[StructICP::RunPcdDirectory]
  B --> C[LoadPCD_XYZI]
  C --> D[Preprocessing::Process/ProcessSplit]
  D --> E{frame0?}
  E -->|yes| F[LocalMap::InitWithFrame -> Rebuild]
  E -->|no| G[MaybeSwapPending / WaitForPendingReady]
  G --> H[Select query map: fine / assoc / coarse]
  H --> I[Registration::AlignPointToVoxelMap]
  I --> J{coarse assoc enabled?}
  J -->|yes| K[coarse QueryNearest*]
  K --> L[refine around fine key (refine_ring)]
  L --> M{refine ok?}
  M -->|yes| N[use refined hit]
  M -->|no| O{coarse_fallback_full?}
  O -->|yes| P[fine full query]
  J -->|no| Q{ring-query enabled?}
  Q -->|yes| R[ring=0 early-exit -> fallback ring]
  Q -->|no| S[legacy query]
  I --> T[Build corrs -> budget/nth_element]
  T --> U[Accumulate H/b -> Solve dx]
  U --> V[Yaw clamp/freeze + update T]
  V --> W[LocalMap::AddFrame -> Rebuild/RequestRebuild]
  W --> X[CsvLogger: icp_diag/perf/voxel stats]
```
证据：pipeline调度与分支细节见 `StructICP::RunPcdDirectory` 与 `Registration::AlignPointToVoxelMap` 的条件分支。(src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:226-738, src/agri_icp/cpp/struct_icp/core/Registration.cpp:199-905)

### 1.3 线程/异步
- **async rebuild** 在 `LocalMap` 内部创建后台线程 `RebuildWorker`，用 `frames_` 深拷贝构建新 map，再通过 shared_ptr swap 到主线程。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:312-390)
- 主线程在 ICP 前调用 `MaybeSwapPending` 或 `WaitForPendingReadyAndSwap`，并通过 `GetActiveMapPtr` 拿到稳定的 shared_ptr。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:112-123,186-230)

---

## 【2】核心参数与默认行为（完整）

> 说明：以下“默认值”以 `run_sequence` 初始化为准；结构体层默认值见各自头文件。 (src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:132-260, src/agri_icp/cpp/struct_icp/core/Preprocessing.hpp:16-39, src/agri_icp/cpp/struct_icp/core/Registration.hpp:17-94, src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:9-20, src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp:17-85)
### 2.1 参数结构与字段（默认值 + 来源 + 使用处）

#### 2.1.1 PreprocessParams
来源：`struct_icp::PreprocessParams` 默认值 + run_sequence 初始化覆盖。 (src/agri_icp/cpp/struct_icp/core/Preprocessing.hpp:16-39, src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:179-194)

| 字段 | 默认值（run_sequence） | 来源 | 使用处（读取点） |
|---|---|---|---|
| min_range | 0.3 | run_sequence 覆盖 | FilterAndDecimate (min_r) (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:139-157)
| max_range | 80.0 | run_sequence 覆盖 | FilterAndDecimate (max_r) (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:139-157)
| decimate | 1 | run_sequence 覆盖 | FilterAndDecimate (decimate) (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:139-151)
| enable_voxel_downsample | true | run_sequence 覆盖 | Process 单输出路径 (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:171-175)
| voxel_size | 0.15 | run_sequence 覆盖 | VoxelDownsampleMean (legacy) (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:171-173)
| enable_split_output | false | run_sequence 覆盖 | ProcessSplit 开关 (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:182-185)
| icp_enable_voxel | true | run_sequence 覆盖 | ProcessSplit icp downsample (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:200-203)
| icp_voxel_size | 0.15 | run_sequence 覆盖 | ProcessSplit icp downsample (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:200-201)
| max_icp_points | 5000 | run_sequence 覆盖 | LimitToN_Structured (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:206-207)
| map_enable_voxel | true | run_sequence 覆盖 | ProcessSplit map downsample (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:193-196)
| map_voxel_size | 0.15 | run_sequence 覆盖 | ProcessSplit map downsample (src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp:193-195)

#### 2.1.2 RegistrationParams
来源：`RegistrationParams` 默认值 + run_sequence 覆盖。 (src/agri_icp/cpp/struct_icp/core/Registration.hpp:17-94, src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:195-246)

| 字段 | 默认值（run_sequence） | 来源 | 使用处 |
|---|---|---|---|
| max_iters | 20 | run_sequence | ICP 迭代上限 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:164)
| max_corr_dist | 1.0 | run_sequence | max_corr2 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:87-90)
| min_effective_corr | 50 | run_sequence | used<min_effective_corr 返回 false (src/agri_icp/cpp/struct_icp/core/Registration.cpp:901-904)
| eps_dx | 1e-4 | run_sequence | early-stop by dx.norm (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1273)
| max_corr_per_iter | 6000 | run_sequence | corr budget / nth_element (src/agri_icp/cpp/struct_icp/core/Registration.cpp:95,879-889)
| eps_cost_rel | 1e-3 | Registration.hpp 默认 | early-stop by cost (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1828-1832)
| yaw_clamp_deg | 0.0 | run_sequence | yaw clamp (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1132-1135)
| legacy_yaw_freeze_by_cond | false | run_sequence | yaw freeze legacy cond (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1140-1145)
| yaw_info_thresh | 1e-4 | run_sequence | yaw freeze by info (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1147-1151)
| alpha_point_to_plane | 0.0 | run_sequence | p2plane mix (src/agri_icp/cpp/struct_icp/core/Registration.cpp:88-90)
| use_score_filter | false | run_sequence | score filter query path (src/agri_icp/cpp/struct_icp/core/Registration.cpp:372-389)
| score_allow_fallback | true | run_sequence | score filter fallback (src/agri_icp/cpp/struct_icp/core/Registration.cpp:380-388)
| score_tau_wall | 0.6 | run_sequence | wall threshold (src/agri_icp/cpp/struct_icp/core/Registration.cpp:438-440)
| use_edge_first | false | run_sequence | edge-first query path (src/agri_icp/cpp/struct_icp/core/Registration.cpp:361-371)
| tau_edge | 0.5 | run_sequence | edge threshold (src/agri_icp/cpp/struct_icp/core/Registration.cpp:268-270)
| use_ring_query | false | run_sequence | ring-query branch (src/agri_icp/cpp/struct_icp/core/Registration.cpp:738-795)
| ring_edge | 0 | run_sequence | ring query (src/agri_icp/cpp/struct_icp/core/Registration.cpp:97-99)
| ring_wall | 0 | run_sequence | ring query (src/agri_icp/cpp/struct_icp/core/Registration.cpp:97-99)
| ring_fallback | 1 | run_sequence | ring query (src/agri_icp/cpp/struct_icp/core/Registration.cpp:97-99)
| use_score_weight | false | run_sequence | score weight in corr build (src/agri_icp/cpp/struct_icp/core/Registration.cpp:425-435)
| w_base | 1.0 | run_sequence | score weight base (src/agri_icp/cpp/struct_icp/core/Registration.cpp:91-92)
| w_score_gain | 1.0 | run_sequence | score weight gain (src/agri_icp/cpp/struct_icp/core/Registration.cpp:430-431)
| w_min | 0.5 | run_sequence | weight clamp (src/agri_icp/cpp/struct_icp/core/Registration.cpp:431)
| w_max | 2.0 | run_sequence | weight clamp (src/agri_icp/cpp/struct_icp/core/Registration.cpp:431)
| weight_only_highscore | true | run_sequence | score weight gate (src/agri_icp/cpp/struct_icp/core/Registration.cpp:426-428)
| use_corr_budget | false | run_sequence | budget path switch (src/agri_icp/cpp/struct_icp/core/Registration.cpp:199)
| corr_build_early_stop | false | run_sequence | early stop in corr build (src/agri_icp/cpp/struct_icp/core/Registration.cpp:213-215)
| corr_budget_edge_ratio | 0.3 | run_sequence | budget calc (src/agri_icp/cpp/struct_icp/core/Registration.cpp:120-129)
| corr_budget_wall_ratio | 0.5 | run_sequence | budget calc (src/agri_icp/cpp/struct_icp/core/Registration.cpp:120-129)
| corr_budget_fallback_ratio | 0.2 | run_sequence | budget calc (src/agri_icp/cpp/struct_icp/core/Registration.cpp:120-129)
| enable_landmark_first | false | run_sequence | landmark-first branch (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1296)
| landmark_stage_max_corr | 1500 | run_sequence | LM stage truncation (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1541-1547)
| landmark_stage_min_corr | 200 | run_sequence | LM stage gate (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1323-1325)
| landmark_stage_weight | 1.0 | run_sequence | LM stage weight (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1350-1355)
| refine_stage_mode | 1 | run_sequence | refine mode switch (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1325-1329)
| refine_stage_max_corr | 2000 | run_sequence | refine truncation (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1541-1547)
| landmark_stage_use_p2plane | true | run_sequence | stage-A alpha (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1340-1341)
| refine_stage_use_p2plane | true | run_sequence | stage-B alpha (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1551-1552)
| enable_single_solve_weighted | false | run_sequence | weighted single-solve branch (src/agri_icp/cpp/struct_icp/core/Registration.cpp:906)
| w_edge/w_wall/w_fallback | 1.0/1.0/1.0 | run_sequence | category weights (src/agri_icp/cpp/struct_icp/core/Registration.cpp:936-939)
| w_cap / w_floor | 5.0 / 0.1 | run_sequence | weight clamp (src/agri_icp/cpp/struct_icp/core/Registration.cpp:946)
| weighted_plane | true | run_sequence | p2plane weight (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1035-1037)
| weighted_normalize | true | run_sequence | normalize by w_mean (src/agri_icp/cpp/struct_icp/core/Registration.cpp:961-978)
| w_edge_auto/w_wall_auto | false | run_sequence | auto weight by score (src/agri_icp/cpp/struct_icp/core/Registration.cpp:940-944)
| w_edge_gain/w_wall_gain | 1.0/1.0 | run_sequence | auto gain (src/agri_icp/cpp/struct_icp/core/Registration.cpp:941-944)
| enable_easy_stop_guard | false | run_sequence | cost_u early-stop guard (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1275-1281)
| easy_stop_min_iters | 2 | run_sequence | cost_u guard threshold (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1275-1278)

#### 2.1.3 VoxelMapParams
来源：`VoxelMapParams` 默认值 + run_sequence 覆盖。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:9-20, src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:250-260)

| 字段 | 默认值（run_sequence） | 来源 | 使用处 |
|---|---|---|---|
| voxel_size | 0.5 | run_sequence | VoxelKey 与 map 分辨率 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:19-27)
| max_neighbor_ring | 1 | run_sequence | Query 默认 ring (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:152-159)
| eig_update_every_K | 10 | run_sequence | eig 更新周期 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:69-71)
| eig_min_points | 20 | run_sequence | normal/score 有效门槛 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:70-71)
| enable_surfel_stats | true | run_sequence | Welford/eig 开关 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:42-49)
| enable_score | true | run_sequence | score/edge 计算开关 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:93-119)
| score_beta | 0.05 | run_sequence | EMA beta (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:59-62)
| score_sigma_e | 0.2 | run_sequence | temp_score sigma (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:64-66)
| score_tau_wall | 0.6 | run_sequence | wall threshold (Registration uses) (src/agri_icp/cpp/struct_icp/core/Registration.cpp:438-440)
| score_tau_edge | 0.5 | run_sequence | edge threshold (Registration uses) (src/agri_icp/cpp/struct_icp/core/Registration.cpp:268-270)

#### 2.1.4 LocalMapParams
来源：`LocalMapParams` 默认值 + pipeline 参数映射；构造时由 `StructICP` 将 PipelineParams 字段装配为 LocalMapParams。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:19-31, src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:29-51)

| 字段 | 默认值（PipelineParams） | 来源 | 使用处 |
|---|---|---|---|
| window_size | 30 | pipeline | frames_ 保留长度 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:93-95)
| use_radius_crop | false | pipeline | Rebuild 是否裁剪 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:264-297)
| crop_radius | 25.0 | pipeline | radius crop 半径 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:283-291)
| rebuild_every_n | 5 (pipeline) | pipeline | AddFrame 中 rebuild 触发 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:98-109)
| enable_async_rebuild | false | pipeline | async thread enable (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:47,100-105)
| rebuild_min_move | 0.0 | pipeline | RequestRebuild gate (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:319-323)
| async_warmup_frames | 1 | pipeline | async warmup 同步 rebuild (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:100-105)
| enable_assoc_map | false | pipeline | assoc map build (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:39-45)
| assoc_voxel | 1.0 | pipeline | assoc map voxel size (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:39-45)
| enable_coarse_assoc | false | pipeline | coarse assoc map (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:39-45)
| coarse_voxel_mul | 2.0 | pipeline | coarse voxel size multiplier (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:41-43)

#### 2.1.5 PipelineParams
来源：`PipelineParams` 默认值 + run_sequence 覆盖。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp:17-85, src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:132-177)

| 字段 | 默认值（run_sequence） | 使用处 |
|---|---|---|
| input_dir/output_dir | CLI 必填 | RunPcdDirectory IO (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:127-141)
| pcd_ext | .pcd | ListFilesSorted (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:137)
| start_frame/max_frames | 0 / -1 | range gate (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:143-148)
| frame_dt | 0.1 | CSV/tum time (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:392)
| T_wb0 | Identity | 初始 pose (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:178)
| insert_every_frame/insert_every_n | true / 1 | map insert gate (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:406-415)
| log_csv / write_traj_tum | true / true | logger/trajectory (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:154-174)
| verbose | true | timing/logging (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:229-231,742-754)
| enable_perf | false | perf_stats 统计 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:235-237,549-738)
| debug_map_ready | false | map ready debug (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:294-300)
| local_window_size | 30 | LocalMap window (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:93-95)
| local_use_radius_crop | false | Rebuild crop (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:264-297)
| local_crop_radius | 25.0 | crop radius (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:283-291)
| local_rebuild_every_n | 5 | rebuild periodicity (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:98-109)
| enable_async_rebuild | false | async thread (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:47)
| rebuild_min_move | 0.0 | RequestRebuild gate (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:319-323)
| async_warmup_frames | 1 | async warmup sync rebuild (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:100-105)
| async_wait_first_map | true | wait for first map (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:273-281)
| async_wait_timeout_ms | 200 | wait timeout (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:279-280)
| min_voxels_for_icp | 50 | map size gate (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:333-338)
| enable_adaptive_ring | false | ring gating (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:302-305,449-474)
| tau_fallback_hi/lo | 0.6 / 0.3 | adaptive ring thresholds (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:453-469)
| fallback_hi_frames/lo_frames | 3 / 5 | adaptive ring counters (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:453-473)
| degrade_hold_frames | 10 | adaptive ring hold (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:458-466)
| adaptive_ring_warmup | 5 | warmup (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:450-483)
| enable_assoc_map | false | assoc map query path (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:290-313)
| assoc_voxel | 1.0 | assoc map voxel size (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:39-45)
| assoc_ring_fallback | 1 | ring override (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:308-310)
| assoc_refine_with_fine | false | refine on fine map (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:309-312)
| assoc_refine_ring | 1 | refine ring (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:311)
| assoc_min_voxels_for_icp | 50 | map size gate (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:334-336)
| enable_coarse_assoc | false | coarse assoc path (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:290-321)
| coarse_voxel_mul | 2.0 | coarse voxel size (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:41-43)
| coarse_ring | 0 | coarse ring (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:315-318)
| coarse_refine_ring | 0 | coarse refine ring (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:317-318)
| coarse_fallback_full | true | coarse fallback (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:319)
| min_coarse_voxels_for_icp | 50 | coarse map size gate (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:333-336)
| enable_adaptive_refine_ring | false | adaptive refine ring (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:486-512)
| refine_ring_min/max | 0 / 1 | adaptive refine ring bounds (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:489-500)
| fallback_ratio_hi/lo | 0.25 / 0.15 | adaptive refine thresholds (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:495-501)
| coarse_hit_ratio_lo | 0.60 | adaptive refine threshold (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:495-501)
| adapt_window | 5 | window size (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:433-439)
| adapt_warmup_frames | 3 | warmup (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:491-492)

### 2.2 OFF 时的回归路径清单（实证）
- **use_corr_budget=OFF** → 走 legacy `nth_element` 截断，不走分桶预算。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:879-889)
- **use_ring_query=OFF** → 走 legacy QueryNearestSurfel/Filtered/Hierarchical 分支，而非 ring0→fallback。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:804-819)
- **enable_coarse_assoc=OFF** → corr build 直接使用 fine map 的 QueryNearest*，不走 coarse→refine→fallback。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:633-737)
- **enable_assoc_map=OFF** → query map 直接使用 fine map，不覆盖 ring_fallback 或 refine_map。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:290-313)
- **enable_single_solve_weighted=OFF** → 跳过加权单次求解，进入 landmark-first 或传统单次累加。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:906,1296,1836)
- **enable_landmark_first=OFF** → 不进入两阶段求解分支。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1296)
- **use_score_filter=OFF / use_edge_first=OFF** → corr build 使用 `QueryNearestSurfel` 或 `QueryNearest`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:804-819)
- **enable_async_rebuild=OFF** → LocalMap::AddFrame 走同步 Rebuild。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:100-109)
- **enable_adaptive_ring=OFF** → `use_ring_query_effective` 直接等于 `use_ring_query`，不降级 ring0。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:302-305)
- **enable_adaptive_refine_ring=OFF** → refine_ring_eff 固定为 coarse_refine_ring。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:317-318,486-512)

---

## 【3】Map / Surfel / Score / Edge

### 3.1 VoxelHashMap 数据结构与增量更新
- **每 voxel 统计字段**：`N, mu, M2, normal, lambdas, line_dir, planarity, linearity, verticality, temp_var/temp_score, score, edge_score` 等全部定义在 `SurfelVoxel`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:158-178)
- **Welford 在线均值/二阶矩**：`mu += delta/N`，`M2 += delta * delta2^T`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:51-55)
- **cov 计算**：`cov = M2 / max(N-1,1)`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:73-74)
- **eig 更新频率**：仅当 `N>=eig_min_points` 且 `N % eig_update_every_K == 0` 才进行。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:69-71)
- **normal_valid 判定与归一化**：Query 时若 `||normal||^2` 非有限或过小则置 invalid，否则归一化为单位向量。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:222-233)
- **QueryNearest 返回的目标点**：使用 voxel 内的均值 `mu` 作为最近点（centroid）。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:180-185)

代码片段（≤30行）
```cpp
// src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:51-75
const Eigen::Vector3d delta = p - mu;
const double inv_n = 1.0 / static_cast<double>(N);
mu += delta * inv_n;
const Eigen::Vector3d delta2 = p - mu;
M2.noalias() += delta * delta2.transpose();
...
if ((N % eig_update_every_K) != 0) return false;
const double denom = std::max(1, N - 1);
const Eigen::Matrix3d cov = M2 / static_cast<double>(denom);
```

### 3.2 Score / Edge 定义
- **normal 与 line_dir**：特征向量最小/最大方向。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:85-90)
- **planarity/linearity/verticality**：
  - `P=(λ2-λ3)/λ1`, `L=(λ1-λ2)/λ1`，`V=1-|n·z|`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:94-101)
- **score**：`score = clamp(max(P,L) * temp_score * V, 0, 1)`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:103-107)
- **edge_score**：`edge_score = clamp(linearity * |line_dir·z| * temp_score, 0, 1)`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:109-112)
- **temporal stability (EMA)**：`m_e,v_e` 在 normal_valid 时更新；`temp_score = exp(-v_e/sigma^2)`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:57-66)
- **有效性**：若特征值/方向非有限或 score 关闭，则 `score_valid/edge_valid` 置 false。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:101-124)

### 3.3 coarse association map（2× fine voxel）
- **map 维护**：LocalMap 可选维护两类“查询用 map”：`enable_assoc_map`（assoc_voxel）与 `enable_coarse_assoc`（fine*coarse_voxel_mul）。(src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:39-45, src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:290-313)
- **Rebuild 同步插入**：重建时同时插入 fine 与 assoc/coarse map。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:269-272,294-297)
- **查询逻辑**：coarse Query → fine refine around key → full fine fallback（可选）。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:227-349)
- **assoc_map refine（非 coarse）**：若 `assoc_refine_with_fine`，先用 assoc map 查询，再用 fine map 做局部 refine。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:307-312, src/agri_icp/cpp/struct_icp/core/Registration.cpp:404-411)
- **refine_ring 自适应**：基于帧级统计（fallback_ratio/coarse_hit_ratio）在 `StructICP` 内调整 `refine_ring_eff`（0/1）。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:486-512)

### 3.4 LocalMap 重建语义（窗口/半径/异步）
- **window_size 是帧数窗口**：`frames_` 超过窗口即 pop_front。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:90-95)
- **radius crop**：`use_radius_crop` 时仅保留 `crop_radius` 内点。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:283-291)
- **异步重建**：后台线程复制 `frames_` 后 rebuild 新 map，再用 shared_ptr swap。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:345-390)

代码片段（≤30行）
```cpp
// src/agri_icp/cpp/struct_icp/core/Registration.cpp:257-320
if (coarse_ok) {
  coarse_hits += 1;
  const auto key = fine_map->KeyFromPoint(hit_c.mu);
  VoxelHashMap::SurfelHit hit_f;
  bool refine_ok = fine_map->QueryNearestSurfelAroundKey(
      key, coarse_refine_ring, pw, hit_f);
  if (refine_ok) {
    coarse_refine_hits += 1;
    hit = hit_f;
    ... // tier/score recheck
  }
}
if (!ok && coarse_fallback_full && fine_map) {
  coarse_fallback_hits += 1;
  ok = fine_map->QueryNearestSurfel(pw, hit); // fallback
}
```

---

## 【4】Correspondence build / Budget / Ring

### 4.1 corr build 分类与 ring 查询
- **EDGE/WALL/FALLBACK 互斥分类**：EDGE 优先，WALL 次之，否则 FALLBACK。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:438-452)
- **ring-query 两阶段**：`ring=0` 先查，未命中再查 `ring_fallback`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:738-795)
- **skip_ring0（自适应降级）**：直接走 fallback ring。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:797-802)
- **adaptive ring gating**：`use_ring_query_effective` 由 pipeline 根据 fallback_ratio 退化决定。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:302-305,449-474)
- **距离门限**：`d2 > max_corr2` 的对应被丢弃。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:419-421)

### 4.2 corr budget（P2-A）
- **预算计算**：`Be/Bw/Bf` 由比例和 `max_corr` 计算，保证 `Bf>=1`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:117-141)
- **动态回收**：`Bf_eff = max_corr - used_edge - used_wall`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:207-212,463-480)
- **early-stop 条件**：`used_total >= max_corr` 时停止 corr build。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:213-215)
- **downgrade**：若 corr 数不足 `min_effective_corr`，清空并回退 legacy query。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:512-559)
- **budget OFF**：使用 legacy `nth_element` 截断。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:879-889)

---

## 【5】ICP 求解与停止准则

### 5.1 残差模型与 Jacobian
- **p2p**：`r = pw - q`，`J = [I, -Hat(pw)]`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1015-1021)
- **p2plane**：`r_pl = n^T(pw - q)`，`J_pl = [n^T, -n^T Hat(pw)]`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1030-1034)
- **更新方式**：`T = exp(dx) * T`（左乘）。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1153)

### 5.2 single-solve weighted ICP（P3-A）与 P3-A-fix
- **权重合成**：corr 权重 `w` 与类别权重 `w_edge/w_wall/w_fallback` 相乘，并 `clamp`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:920-947)
- **加权累加**：`H += w_pt*w * J^T J`，`b += w_pt*w * J^T r`；p2plane 可选乘权重。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1022-1039)
- **P3-A-fix early-stop**：仅在 `enable_single_solve_weighted` 时，使用 **未加权 cost_u** 的 `rel_drop_u` 作为 early-stop。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1262-1294)
  - cost_u 当前实现只累计 p2p 部分（未加权），未包含 p2plane。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1022-1027,1054)
- **weighted OFF 的 early-stop**：使用加权 cost_sum（含 p2plane）与 `cost_prev` 的相对下降判据。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1887-1890,1828-1832)

### 5.2b landmark-first two-stage（P2-B）
- **Stage-A**：edge+wall corr 组成 landmark 列表；不足 `landmark_stage_min_corr` 则跳过。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1323-1325)
- **Stage-B**：按 `refine_stage_mode` 选择 fallback/all，且有 max_corr 截断。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1540-1547)
- **每阶段解算**：分别累加 H/b 并解 dx，均经过 yaw policy 后更新 T。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1550-1589)

### 5.3 yaw 冻结 / cond proxy
- **cond proxy**：Gershgorin bounds。 (src/agri_icp/cpp/struct_icp/core/Statistics.hpp:42-63)
- **yaw observability**：Schur complement `S(2,2)`。 (src/agri_icp/cpp/struct_icp/core/Statistics.hpp:65-76)
- **yaw freeze**：默认按 `yaw_info < yaw_info_thresh` 冻结 dx(5)，legacy 模式按 cond(JTJ)。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1140-1151)
- **cond_JTJ_scaled**：当前写入值等于 cond_JTJ（未做额外尺度归一化）。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1176-1179)

---

## 【6】日志与诊断

### 6.1 CSV 输出与字段
- **icp_diag.csv**：每迭代一行，写在 `LogIcpIterStats`。字段列表与顺序见写入代码。 (src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:9-118)
- **perf_stats.csv**：每帧一行（在 do_insert 时写入）。完整 header 在 `WriteHeadersIfNeeded`。 (src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:485-503, src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:525-738)
- **voxel_stats.csv / voxel_score_stats.csv / edge_score_stats.csv**：每帧 map 插入后写入。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:525-549, src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:468-482)

**icp_diag.csv header（顺序与代码一致）**  
(src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:16-118)
```
t_rel,it,N,inlier_ratio,mean_res,median_res,mad_res,cond_JTJ,grad_norm,
dtrans,dyaw,droll,dpitch,yaw_raw,yaw_clamped,yaw_clamp_triggered,yaw_frozen,
eig_min,eig_max,eig_rank,num_corr,corr_p50,corr_p90,n_raw,n_split,
n_icp,n_map,icp_r0,icp_r1,icp_r2,map_r0,map_r1,map_r2,
alpha_p2plane,plane_used_ratio,plane_res_mean,
corr_highscore_ratio,highscore_fallback_ratio,score_tau_wall,score_filter_used,
w_mean,w_p90,w_min,w_max,w_highscore_ratio,score_mean_used,
corr_edge_ratio,corr_wall_ratio,corr_fallback_ratio,tau_edge,edge_first_used,
yaw_info,yaw_freeze_reason,cond_JTJ_scaled,
budget_edge,budget_wall,budget_fallback_eff,used_corr_total,corr_edge_used,
corr_wall_used,corr_fallback_used,corr_build_early_stop,corr_queried_points,
corr_budget_downgrade,ring_query_downgrade,lm_first_enabled,lm_stage_used,
lm_corr_used,refine_stage_mode,refine_corr_used,lm_dx_norm,refine_dx_norm,
lm_cost,refine_cost,lm_plane_used_ratio,refine_plane_used_ratio,
weighted_enabled,w_edge,w_wall,w_fallback,w_cap,w_mean_used,w_min_used,w_max_used,
w_edge_used_mean,w_wall_used_mean,w_fallback_used_mean,cost_u,rel_drop_u,stop_by_cost_u
```

**perf_stats.csv header（顺序与代码一致）**  
(src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:485-503)
```
t_rel,dt_load,dt_preprocess,dt_icp_total,dt_corr_build,dt_query,dt_budget,
dt_accum,dt_solve,dt_map_update,dt_rebuild,dt_rebuild_async,dt_logging,used_corr,
corr_edge_ratio,corr_highscore_ratio,fallback_ratio,yaw_frozen,yaw_info,
waited_first_map_ms,skipped_icp_due_to_small_map,
query_edge_hits,query_wall_hits,query_fallback_hits,query_ring_used_mean,
corr_budget_downgrade,ring_query_downgrade,ring0_hit_ratio,ring_fallback_ratio,
adaptive_ring_enabled,adaptive_ring_degraded,adaptive_ring_hold_left,
effective_use_ring0,skipped_ring0_queries,
assoc_map_enabled,assoc_voxel,assoc_refine_with_fine,assoc_ring0_hit_ratio,
assoc_fallback_ratio,refine_used_ratio,
lm_first_enabled,lm_stage_used_ratio,lm_corr_used_mean,refine_corr_used_mean,
lm_dx_norm_mean,refine_dx_norm_mean,
use_corr_budget,corr_build_early_stop,budget_edge,budget_wall,budget_fallback_eff,
corr_edge_used,corr_wall_used,corr_fallback_used,used_corr_total,corr_queried_points,
weighted_enabled,w_mean_frame,w_max_frame,edge_ratio,wall_ratio,fallback_ratio_frame,
coarse_enabled,coarse_hit_ratio,coarse_refine_hit_ratio,coarse_fallback_ratio,
coarse_ring,refine_ring,fb_avg,coarse_hit_avg,map_version,used_map_version
```

**voxel_stats / voxel_score_stats / edge_score_stats header**  
(src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:468-482)
```
voxel_stats: t_rel,voxel_count,mean_N,p90_N,eig_updated_voxels
voxel_score_stats: t_rel,voxel_count,score_valid_ratio,score_mean,score_p90,score_p99,high_score_ratio,planarity_mean,linearity_mean,verticality_mean,temp_var_mean
edge_score_stats: t_rel,voxel_count,edge_valid_ratio,edge_score_mean,edge_score_p90,edge_score_p99,high_edge_ratio
```

### 6.2 诊断字段使用建议（健康检查）
- `corr_budget_downgrade=1` 应极少出现，否则预算过紧/图稀疏。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:512-559)
- `ring0_hit_ratio` 低且 `ring_fallback_ratio` 高 → ring0 early-exit 失效，应考虑 adaptive ring 或 coarse assoc。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1092-1097)
- `coarse_hit_ratio` 高且 `coarse_fallback_ratio` 低 → coarse assoc 有效；若相反可提升 `refine_ring` 或开启自适应。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:653-665)
- `stop_by_cost_u` 主要出现在 easy frame，若频繁出现在 hard frame 可能说明 cost_u 门限过小。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1262-1291)
- `yaw_frozen=1` 且 `yaw_info` 低 → yaw observability 不足；若 `legacy_yaw_freeze_by_cond=1` 则冻结触发原因改为 cond(JTJ)。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1140-1151)

---

## 【7】性能瓶颈点定位（基于代码）
- **Query 扫描**：`QueryNearestInRing` / `QueryNearestSurfel*` 逐 voxel 扫描 `(2r+1)^3`。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:173-187,322-347,441-458)
- **Rebuild O(N)**：每次 rebuild 遍历 frames_ 所有点；半径裁剪仍需扫描全部点。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:260-297)
- **corr 截断**：`nth_element` 在 max_corr 裁剪时触发。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:879-889)
- **LDLT 解算**：每迭代一次 6x6 LDLT。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1070-1076)
- **unordered_map rehash**：InsertPoints 对 map_ reserve 并逐点更新。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:128-145)

已有优化点：ring-query、corr budget、coarse association map、async rebuild；对应开关在 `RegistrationParams` 与 `PipelineParams`。 (src/agri_icp/cpp/struct_icp/core/Registration.hpp:49-67, src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp:58-85)

---

## 【8】封版摘要（10条以内）
- 支持 **单帧 PCD 对齐** 与 **序列 PCD pipeline**（`demo_pcd_icp` / `run_sequence`）。 (src/agri_icp/cpp/struct_icp/examples/demo_pcd_icp.cpp:47-133, src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:127-756)
- ICP 主模型为 **p2p + 可选 p2plane 混合**，由 `alpha_point_to_plane` 控制。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:88-90,1017-1034)
- 具备 **surfel stats + score/edge** 的 voxel map（planarity/linearity/verticality/temporal）。 (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp:51-112)
- **ring-query / corr budget / coarse assoc map / adaptive ring & refine** 均默认 OFF，保证回归路径。 (src/agri_icp/cpp/struct_icp/core/Registration.hpp:49-67, src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp:58-85)
- **P3-A weighted single-solve + P3-A-fix**：加权求解但 early-stop 用 cost_u。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:906-1294)
- **yaw 冻结** 默认使用 yaw_info（Schur）阈值，而非 cond(JTJ)；legacy 可选。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:1140-1151, src/agri_icp/cpp/struct_icp/core/Statistics.hpp:65-76)
- **async rebuild** 通过 shared_ptr swap + frames_ 深拷贝，保证 ICP 读图稳定。 (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:186-199,332-390)
- **CSV 诊断完备**：icp_diag、perf_stats、voxel/score/edge stats。 (src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp:16-118,485-503)

**推荐默认配置（稳定 baseline）**
- 全部新特性关闭：`use_corr_budget=0`, `use_ring_query=0`, `enable_single_solve_weighted=0`, `enable_landmark_first=0`, `enable_coarse_assoc=0`, `enable_assoc_map=0`, `enable_adaptive_ring=0`, `enable_adaptive_refine_ring=0`。 (src/agri_icp/cpp/struct_icp/core/Registration.hpp:49-80, src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp:58-85)

**实验配置（论文消融）**
- 依赖开关：`use_edge_first=1`, `use_score_filter=1`, `use_score_weight=1`, `alpha_point_to_plane>0`, `use_corr_budget=1`, `enable_coarse_assoc=1`, `enable_adaptive_refine_ring=1`。 (src/agri_icp/cpp/struct_icp/core/Registration.hpp:39-90, src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp:65-85)

**已知风险点/谨慎组合**
- `enable_async_rebuild=1` 且 map 小时：需要 wait/skip ICP 逻辑，否则可能 early fail。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:270-348)
- `use_corr_budget=1` 且 corr 较少：会触发 downgrade，需监控 `corr_budget_downgrade`。 (src/agri_icp/cpp/struct_icp/core/Registration.cpp:512-559)
- `enable_coarse_assoc=1` 且 coarse map voxel 很少：需 `min_coarse_voxels_for_icp` gate。 (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:333-338)

---

## 关键文件清单
- `src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp`
- `src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp`
- `src/agri_icp/cpp/struct_icp/pipeline/StructICP.hpp`
- `src/agri_icp/cpp/struct_icp/core/Registration.cpp`
- `src/agri_icp/cpp/struct_icp/core/Registration.hpp`
- `src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp`
- `src/agri_icp/cpp/struct_icp/core/VoxelHashMap.cpp`
- `src/agri_icp/cpp/struct_icp/core/LocalMap.hpp`
- `src/agri_icp/cpp/struct_icp/core/Preprocessing.hpp`
- `src/agri_icp/cpp/struct_icp/core/Preprocessing.cpp`
- `src/agri_icp/cpp/struct_icp/core/Statistics.hpp`
- `src/agri_icp/cpp/struct_icp/core/IcpStats.hpp`
- `src/agri_icp/cpp/struct_icp/core/CsvLogger.hpp`
- `src/agri_icp/cpp/struct_icp/core/CsvLogger.cpp`
- `src/agri_icp/cpp/struct_icp/examples/demo_pcd_icp.cpp`

## 关键函数索引（签名 + 路径）
- `int main(int argc, char** argv)` — `run_sequence` (src/agri_icp/cpp/struct_icp/pipeline/apps/run_sequence.cpp:123)
- `bool StructICP::RunPcdDirectory()` (src/agri_icp/cpp/struct_icp/pipeline/StructICP.cpp:127)
- `void Preprocessing::Process(const std::vector<PointXYZI>&, std::vector<PointXYZI>&) const` (src/agri_icp/cpp/struct_icp/core/Preprocessing.hpp:56-58)
- `void Preprocessing::ProcessSplit(const std::vector<PointXYZI>&, std::vector<PointXYZI>&, std::vector<PointXYZI>&) const` (src/agri_icp/cpp/struct_icp/core/Preprocessing.hpp:61-63)
- `bool Registration::AlignPointToVoxelMap(const VoxelHashMap&, const std::vector<PointXYZI>&, Sophus::SE3d&, ...) const` (src/agri_icp/cpp/struct_icp/core/Registration.hpp:148-154)
- `bool VoxelHashMap::QueryNearestSurfel(const Eigen::Vector3d&, SurfelHit&) const` (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:58)
- `bool VoxelHashMap::QueryNearestSurfelAroundKey(const VoxelKey&, int, const Eigen::Vector3d&, SurfelHit&) const` (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:62-65)
- `void VoxelHashMap::InsertPoints(const std::vector<Eigen::Vector3d>&)` (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:34)
- `bool VoxelHashMap::SurfelVoxel::Add(...)` (src/agri_icp/cpp/struct_icp/core/VoxelHashMap.hpp:179-187)
- `void LocalMap::InitWithFrame(const std::vector<Eigen::Vector3d>&, const Sophus::SE3d&)` (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:74-84)
- `void LocalMap::AddFrame(const std::vector<Eigen::Vector3d>&, const Sophus::SE3d&)` (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:86-109)
- `void LocalMap::Rebuild(...)` (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:250-309)
- `void LocalMap::RebuildWorker()` (src/agri_icp/cpp/struct_icp/core/LocalMap.hpp:332-390)
- `double ConditionNumberJTJ(const Eigen::Matrix<double,6,6>&)` (src/agri_icp/cpp/struct_icp/core/Statistics.hpp:42-63)
- `double YawInfoSchur(const Eigen::Matrix<double,6,6>&)` (src/agri_icp/cpp/struct_icp/core/Statistics.hpp:65-76)
- `void CsvLogger::LogIcpIterStats(double, const IcpIterStats&)` (src/agri_icp/cpp/struct_icp/core/CsvLogger.hpp:32)
- `void CsvLogger::LogPerfStats(...)` (src/agri_icp/cpp/struct_icp/core/CsvLogger.hpp:64-135)
