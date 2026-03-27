#include <iostream>
#include <string>
#include <algorithm>

#include "StructICP.hpp"

static void PrintUsage(const char* prog) {
  std::cerr
      << "Usage:\n"
      << "  " << prog << " --input <pcd_dir> --output <out_dir> [options]\n\n"
      << "Options:\n"
      << "  --dt <seconds>            Trajectory timestamp step (default: 0.1)\n"
      << "  --start <idx>             Start frame index (default: 0)\n"
      << "  --max <N>                 Max frames to process (default: -1 = all)\n"
      << "  --insert_every_n <N>       Insert map every N frames (default: 1)\n"
      << "  --max_iters <N>            ICP max iters (default: 20)\n"
      << "  --max_corr <meters>        Max correspondence distance (default: 1.0)\n"
      << "  --min_corr <N>             Min effective correspondences (default: 50)\n"
      << "  --max_corr_iter <N>        Max correspondences used per iteration (default: 6000)\n"
      << "  --alpha_p2plane <a>        Mix ratio for point-to-plane (default: 0.0)\n"
      << "  --score_filter             Enable score-filtered association\n"
      << "  --score_tau_wall <t>       Score threshold (default: 0.6)\n"
      << "  --no_score_fallback        Disable fallback to unfiltered nearest\n"
      << "  --use_edge_first <0/1>     Enable edge-first association (default: 0)\n"
      << "  --tau_edge <t>             Edge score threshold (default: 0.5)\n"
      << "  --use_score_weight         Enable score-based weighting\n"
      << "  --w_score_gain <g>         Score weight gain (default: 1.0)\n"
      << "  --w_min <w>                Min weight clamp (default: 0.5)\n"
      << "  --w_max <w>                Max weight clamp (default: 2.0)\n"
      << "  --weight_only_highscore <0/1> Apply weights only to high-score hits (default: 1)\n"
      << "  --ring_edge <n>            Neighbor ring for edge query (default: 0)\n"
      << "  --ring_wall <n>            Neighbor ring for wall query (default: 0)\n"
      << "  --ring_fallback <n>        Neighbor ring for fallback query (default: 1)\n"
      << "  --use_ring_query <0/1>     Enable ring overrides (default: 0)\n"
      << "  --corr_budget_edge_ratio <r>  Budget ratio for edge (default: 0.3)\n"
      << "  --corr_budget_wall_ratio <r>  Budget ratio for wall (default: 0.5)\n"
      << "  --corr_budget_fallback_ratio <r> Budget ratio for fallback (default: 0.2)\n"
      << "  --use_corr_budget <0/1>    Enable corr budget buckets (default: 0)\n"
      << "  --corr_build_early_stop <0/1> Enable corr early-stop (default: 0)\n"
      << "  --enable_landmark_first  Enable landmark-first two-stage ICP\n"
      << "  --landmark_stage_max_corr <n> Stage-A max corr (default: 1500)\n"
      << "  --landmark_stage_min_corr <n> Stage-A min corr (default: 200)\n"
      << "  --landmark_stage_weight <w>  Stage-A weight (default: 1.0)\n"
      << "  --refine_stage_mode <m>   0:none,1:fallback,2:all (default: 1)\n"
      << "  --refine_stage_max_corr <n> Stage-B max corr (default: 2000)\n"
      << "  --landmark_stage_use_p2plane <0/1> Stage-A p2plane (default: 1)\n"
      << "  --refine_stage_use_p2plane <0/1> Stage-B p2plane (default: 1)\n"
      << "  --enable_single_solve_weighted  Enable single-solve weighted ICP\n"
      << "  --w_edge <w>                   EDGE corr weight (default: 1.0)\n"
      << "  --w_wall <w>                   WALL corr weight (default: 1.0)\n"
      << "  --w_fallback <w>               FALLBACK corr weight (default: 1.0)\n"
      << "  --w_cap <w>                    Weight cap (default: 5.0)\n"
      << "  --w_floor <w>                  Weight floor (default: 0.1)\n"
      << "  --weighted_plane <0/1>         Weight p2plane term (default: 1)\n"
      << "  --weighted_normalize <0/1>     Normalize weights by mean (default: 1)\n"
      << "  --w_edge_auto <0/1>            Edge auto weight (default: 0)\n"
      << "  --w_wall_auto <0/1>            Wall auto weight (default: 0)\n"
      << "  --w_edge_gain <g>              Edge auto gain (default: 1.0)\n"
      << "  --w_wall_gain <g>              Wall auto gain (default: 1.0)\n"
      << "  --enable_easy_stop_guard       Guard cost_u early-stop (default: 0)\n"
      << "  --easy_stop_min_iters <n>      Min iters before cost_u stop (default: 2)\n"
      << "  --legacy_yaw_freeze_by_cond Enable legacy yaw freeze by cond(JTJ)\n"
      << "  --yaw_info_thresh <t>      Yaw info threshold (default: 1e-4)\n"
      << "  --enable_perf             Enable perf_stats.csv output\n"
      << "  --yaw_clamp_deg <deg>     clamp per-iter yaw update (0=off)\n"
      << "  --async_warmup_frames <n> Number of sync rebuild frames at start (default: 1)\n"
      << "  --async_wait_first_map <0/1> Wait for first async map (default: 1)\n"
      << "  --async_wait_timeout_ms <ms> Wait timeout for first map (default: 200)\n"
      << "  --min_voxels_for_icp <n> Minimum voxels to run ICP (default: 50)\n"
      << "  --debug_map_ready         Print map readiness diagnostics\n"
      << "  --enable_adaptive_ring    Enable adaptive ring gating\n"
      << "  --tau_fallback_hi <t>     Fallback ratio high threshold (default: 0.6)\n"
      << "  --tau_fallback_lo <t>     Fallback ratio low threshold (default: 0.3)\n"
      << "  --fallback_hi_frames <n>  High ratio frames to degrade (default: 3)\n"
      << "  --fallback_lo_frames <n>  Low ratio frames to recover (default: 5)\n"
      << "  --degrade_hold_frames <n> Min hold frames in degrade (default: 10)\n"
      << "  --adaptive_ring_warmup <n> Warmup frames before gating (default: 5)\n"
      << "  --enable_assoc_map        Enable coarse association map\n"
      << "  --assoc_voxel <meters>    Assoc voxel size (default: 1.0)\n"
      << "  --assoc_ring_fallback <n> Assoc fallback ring (default: 1)\n"
      << "  --assoc_refine_with_fine <0/1> Refine with fine map (default: 0)\n"
      << "  --assoc_refine_ring <n>   Fine refine ring (default: 1)\n"
      << "  --assoc_min_voxels_for_icp <n> Assoc map voxel gate (default: 50)\n"
      << "  --enable_coarse_assoc     Enable coarse assoc map (2x voxel)\n"
      << "  --coarse_voxel_mul <m>    Coarse voxel multiplier (default: 2.0)\n"
      << "  --coarse_ring <n>         Coarse query ring (default: 0)\n"
      << "  --refine_ring <n>         Fine refine ring around coarse (default: 0)\n"
      << "  --coarse_fallback_full <0/1> Fallback to full fine query (default: 1)\n"
      << "  --min_coarse_voxels_for_icp <n> Coarse map voxel gate (default: 50)\n"
      << "  --enable_adaptive_refine_ring Enable adaptive refine ring\n"
      << "  --refine_ring_min <n>   Min refine ring (default: 0)\n"
      << "  --refine_ring_max <n>   Max refine ring (default: 1)\n"
      << "  --fallback_ratio_hi <t> Raise ring when fallback avg > t (default: 0.25)\n"
      << "  --fallback_ratio_lo <t> Lower ring when fallback avg < t (default: 0.15)\n"
      << "  --coarse_hit_ratio_lo <t> Raise ring when coarse hit avg < t (default: 0.60)\n"
      << "  --adapt_window <n>      Window size for avg (default: 5)\n"
      << "  --adapt_warmup_frames <n> Warmup frames (default: 3)\n"
      << "  --voxel_map <meters>       VoxelMap voxel size (default: 0.5)\n"
      << "  --eig_update_k <K>         Update eigen every K adds (default: 10)\n"
      << "  --eig_min_points <N>       Min points to update eigen (default: 20)\n"
      << "  --no_surfel_stats          Disable surfel stats updates\n"
      << "  --no_score                 Disable landmark score stats\n"
      << "  --score_beta <b>           EMA beta for temp stability (default: 0.05)\n"
      << "  --score_sigma_e <s>        Sigma for temp score (default: 0.2)\n"
      << "  --score_tau_wall <t>       High score threshold (default: 0.6)\n"
      << "  --down_voxel <meters>      Legacy preprocess downsample voxel size (default: 0.15)\n"
      << "\n"
      << "  --split                    Enable split preprocessing output (icp_points/map_points)\n"
      << "  --icp_voxel <meters>       Voxel size for ICP points (default: 0.15)\n"
      << "  --map_voxel <meters>       Voxel size for MapInsert points (default: 0.15)\n"
      << "  --icp_n <N>                Max ICP source points (default: 5000)\n"
      << "\n"
      << "  --enable_async_rebuild     Enable async map rebuild\n"
      << "  --rebuild_min_move <meters> Min move to trigger rebuild (default: 0.0)\n"
      << "\n"
      << "  --no_csv                   Disable CSV logging\n"
      << "  --no_traj                  Disable trajectory output\n"
      << "  --quiet                    Disable verbose output\n";
}

static bool HasArg(int i, int argc) { return i + 1 < argc; }

int main(int argc, char** argv) {
  if (argc < 5) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string input_dir;
  std::string output_dir;

  // Pipeline params
  struct_icp::pipeline::PipelineParams pip;
  pip.frame_dt = 0.1;
  pip.start_frame = 0;
  pip.max_frames = -1;
  pip.insert_every_n = 1;
  pip.insert_every_frame = true;
  pip.log_csv = true;
  pip.write_traj_tum = true;
  pip.verbose = true;
  pip.topk_K = 50;
  pip.enable_async_rebuild = false;
  pip.rebuild_min_move = 0.0;
  pip.enable_perf = false;
  pip.async_warmup_frames = 1;
  pip.async_wait_first_map = true;
  pip.async_wait_timeout_ms = 200;
  pip.min_voxels_for_icp = 50;
  pip.debug_map_ready = false;
  pip.enable_adaptive_ring = false;
  pip.tau_fallback_hi = 0.6;
  pip.tau_fallback_lo = 0.3;
  pip.fallback_hi_frames = 3;
  pip.fallback_lo_frames = 5;
  pip.degrade_hold_frames = 10;
  pip.adaptive_ring_warmup = 5;
  pip.enable_assoc_map = false;
  pip.assoc_voxel = 1.0;
  pip.assoc_ring_fallback = 1;
  pip.assoc_refine_with_fine = false;
  pip.assoc_refine_ring = 1;
  pip.assoc_min_voxels_for_icp = 50;
  pip.enable_coarse_assoc = false;
  pip.coarse_voxel_mul = 2.0;
  pip.coarse_ring = 0;
  pip.coarse_refine_ring = 0;
  pip.coarse_fallback_full = true;
  pip.min_coarse_voxels_for_icp = 50;
  pip.enable_adaptive_refine_ring = false;
  pip.refine_ring_min = 0;
  pip.refine_ring_max = 1;
  pip.fallback_ratio_hi = 0.25;
  pip.fallback_ratio_lo = 0.15;
  pip.coarse_hit_ratio_lo = 0.60;
  pip.adapt_window = 5;
  pip.adapt_warmup_frames = 3;

  // Core params
  struct_icp::PreprocessParams pp;
  pp.min_range = 0.3;
  pp.max_range = 80.0;
  pp.enable_voxel_downsample = true;
  pp.voxel_size = 0.15;     // legacy (split=OFF)
  pp.decimate = 1;

  // NEW defaults for split mode (only used if --split)
  pp.enable_split_output = false;
  pp.icp_enable_voxel = true;
  pp.icp_voxel_size = 0.15;
  pp.map_enable_voxel = true;
  pp.map_voxel_size = 0.15;     // start with same as legacy; later you can set 0.08
  pp.max_icp_points = 5000;

  struct_icp::RegistrationParams rp;
  rp.max_iters = 20;
  rp.eps_dx = 1e-4;
  rp.max_corr_dist = 1.0;
  rp.min_effective_corr = 50;
  rp.yaw_clamp_deg = 0.0;
  rp.alpha_point_to_plane = 0.0;
  rp.use_score_filter = false;
  rp.score_allow_fallback = true;
  rp.score_tau_wall = 0.6;
  rp.use_edge_first = false;
  rp.tau_edge = 0.5;
  rp.use_score_weight = false;
  rp.w_base = 1.0;
  rp.w_score_gain = 1.0;
  rp.w_min = 0.5;
  rp.w_max = 2.0;
  rp.weight_only_highscore = true;
  rp.use_ring_query = false;
  rp.ring_edge = 0;
  rp.ring_wall = 0;
  rp.ring_fallback = 1;
  rp.use_corr_budget = false;
  rp.corr_build_early_stop = false;
  rp.corr_budget_edge_ratio = 0.3;
  rp.corr_budget_wall_ratio = 0.5;
  rp.corr_budget_fallback_ratio = 0.2;
  rp.enable_landmark_first = false;
  rp.landmark_stage_max_corr = 1500;
  rp.landmark_stage_min_corr = 200;
  rp.landmark_stage_weight = 1.0;
  rp.refine_stage_mode = 1;
  rp.refine_stage_max_corr = 2000;
  rp.landmark_stage_use_p2plane = true;
  rp.refine_stage_use_p2plane = true;
  rp.enable_single_solve_weighted = false;
  rp.w_edge = 1.0;
  rp.w_wall = 1.0;
  rp.w_fallback = 1.0;
  rp.w_cap = 5.0;
  rp.w_floor = 0.1;
  rp.weighted_plane = true;
  rp.weighted_normalize = true;
  rp.w_edge_auto = false;
  rp.w_wall_auto = false;
  rp.w_edge_gain = 1.0;
  rp.w_wall_gain = 1.0;
  rp.enable_easy_stop_guard = false;
  rp.easy_stop_min_iters = 2;
  rp.legacy_yaw_freeze_by_cond = false;
  rp.yaw_info_thresh = 1e-4;

  // NEW: per-iter correspondence budget (this is what your csv shows as N=6000)
  rp.max_corr_per_iter = 6000;

  struct_icp::VoxelMapParams mp;
  mp.voxel_size = 0.5;
  mp.max_neighbor_ring = 1;
  mp.eig_update_every_K = 10;
  mp.eig_min_points = 20;
  mp.enable_surfel_stats = true;
  mp.enable_score = true;
  mp.score_beta = 0.05;
  mp.score_sigma_e = 0.2;
  mp.score_tau_wall = 0.6;
  mp.score_tau_edge = 0.5;

  // ---- parse args ----
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];

    if (a == "--input" && HasArg(i, argc)) {
      input_dir = argv[++i];
    } else if (a == "--output" && HasArg(i, argc)) {
      output_dir = argv[++i];
    } else if (a == "--dt" && HasArg(i, argc)) {
      pip.frame_dt = std::stod(argv[++i]);
    } else if (a == "--start" && HasArg(i, argc)) {
      pip.start_frame = std::stoi(argv[++i]);
    } else if (a == "--max" && HasArg(i, argc)) {
      pip.max_frames = std::stoi(argv[++i]);
    } else if (a == "--insert_every_n" && HasArg(i, argc)) {
      pip.insert_every_n = std::max(1, std::stoi(argv[++i]));
    } else if (a == "--max_iters" && HasArg(i, argc)) {
      rp.max_iters = std::stoi(argv[++i]);
    } else if (a == "--max_corr" && HasArg(i, argc)) {
      rp.max_corr_dist = std::stod(argv[++i]);
    } else if (a == "--min_corr" && HasArg(i, argc)) {
      rp.min_effective_corr = std::stoi(argv[++i]);
    
    } else if (a == "--max_corr_iter" && HasArg(i, argc)) {
      rp.max_corr_per_iter = std::max(0, std::stoi(argv[++i]));
    
    } else if (a == "--alpha_p2plane" && HasArg(i, argc)) {
      rp.alpha_point_to_plane = std::stod(argv[++i]);

    } else if (a == "--score_filter") {
      rp.use_score_filter = true;
    } else if (a == "--score_tau_wall" && HasArg(i, argc)) {
      rp.score_tau_wall = std::stod(argv[++i]);
    } else if (a == "--no_score_fallback") {
      rp.score_allow_fallback = false;
    } else if (a == "--use_edge_first" && HasArg(i, argc)) {
      rp.use_edge_first = (std::stoi(argv[++i]) != 0);
    } else if (a == "--tau_edge" && HasArg(i, argc)) {
      rp.tau_edge = std::stod(argv[++i]);
      mp.score_tau_edge = rp.tau_edge;
    } else if (a == "--use_score_weight") {
      rp.use_score_weight = true;
    } else if (a == "--w_score_gain" && HasArg(i, argc)) {
      rp.w_score_gain = std::stod(argv[++i]);
    } else if (a == "--w_min" && HasArg(i, argc)) {
      rp.w_min = std::stod(argv[++i]);
    } else if (a == "--w_max" && HasArg(i, argc)) {
      rp.w_max = std::stod(argv[++i]);
    } else if (a == "--weight_only_highscore" && HasArg(i, argc)) {
      rp.weight_only_highscore = (std::stoi(argv[++i]) != 0);
    } else if (a == "--ring_edge" && HasArg(i, argc)) {
      rp.ring_edge = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--ring_wall" && HasArg(i, argc)) {
      rp.ring_wall = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--ring_fallback" && HasArg(i, argc)) {
      rp.ring_fallback = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--use_ring_query" && HasArg(i, argc)) {
      rp.use_ring_query = (std::stoi(argv[++i]) != 0);
    } else if (a == "--corr_budget_edge_ratio" && HasArg(i, argc)) {
      rp.corr_budget_edge_ratio = std::stod(argv[++i]);
    } else if (a == "--corr_budget_wall_ratio" && HasArg(i, argc)) {
      rp.corr_budget_wall_ratio = std::stod(argv[++i]);
    } else if (a == "--corr_budget_fallback_ratio" && HasArg(i, argc)) {
      rp.corr_budget_fallback_ratio = std::stod(argv[++i]);
    } else if (a == "--use_corr_budget" && HasArg(i, argc)) {
      rp.use_corr_budget = (std::stoi(argv[++i]) != 0);
    } else if (a == "--corr_build_early_stop" && HasArg(i, argc)) {
      rp.corr_build_early_stop = (std::stoi(argv[++i]) != 0);
    } else if (a == "--enable_landmark_first") {
      rp.enable_landmark_first = true;
    } else if (a == "--landmark_stage_max_corr" && HasArg(i, argc)) {
      rp.landmark_stage_max_corr = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--landmark_stage_min_corr" && HasArg(i, argc)) {
      rp.landmark_stage_min_corr = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--landmark_stage_weight" && HasArg(i, argc)) {
      rp.landmark_stage_weight = std::stod(argv[++i]);
    } else if (a == "--refine_stage_mode" && HasArg(i, argc)) {
      rp.refine_stage_mode = std::stoi(argv[++i]);
    } else if (a == "--refine_stage_max_corr" && HasArg(i, argc)) {
      rp.refine_stage_max_corr = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--landmark_stage_use_p2plane" && HasArg(i, argc)) {
      rp.landmark_stage_use_p2plane = (std::stoi(argv[++i]) != 0);
    } else if (a == "--refine_stage_use_p2plane" && HasArg(i, argc)) {
      rp.refine_stage_use_p2plane = (std::stoi(argv[++i]) != 0);
    } else if (a == "--enable_single_solve_weighted") {
      rp.enable_single_solve_weighted = true;
    } else if (a == "--w_edge" && HasArg(i, argc)) {
      rp.w_edge = std::stod(argv[++i]);
    } else if (a == "--w_wall" && HasArg(i, argc)) {
      rp.w_wall = std::stod(argv[++i]);
    } else if (a == "--w_fallback" && HasArg(i, argc)) {
      rp.w_fallback = std::stod(argv[++i]);
    } else if (a == "--w_cap" && HasArg(i, argc)) {
      rp.w_cap = std::stod(argv[++i]);
    } else if (a == "--w_floor" && HasArg(i, argc)) {
      rp.w_floor = std::stod(argv[++i]);
    } else if (a == "--weighted_plane" && HasArg(i, argc)) {
      rp.weighted_plane = (std::stoi(argv[++i]) != 0);
    } else if (a == "--weighted_normalize" && HasArg(i, argc)) {
      rp.weighted_normalize = (std::stoi(argv[++i]) != 0);
    } else if (a == "--w_edge_auto" && HasArg(i, argc)) {
      rp.w_edge_auto = (std::stoi(argv[++i]) != 0);
    } else if (a == "--w_wall_auto" && HasArg(i, argc)) {
      rp.w_wall_auto = (std::stoi(argv[++i]) != 0);
    } else if (a == "--w_edge_gain" && HasArg(i, argc)) {
      rp.w_edge_gain = std::stod(argv[++i]);
    } else if (a == "--w_wall_gain" && HasArg(i, argc)) {
      rp.w_wall_gain = std::stod(argv[++i]);
    } else if (a == "--enable_easy_stop_guard") {
      rp.enable_easy_stop_guard = true;
    } else if (a == "--easy_stop_min_iters" && HasArg(i, argc)) {
      rp.easy_stop_min_iters = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--legacy_yaw_freeze_by_cond") {
      rp.legacy_yaw_freeze_by_cond = true;
    } else if (a == "--yaw_info_thresh" && HasArg(i, argc)) {
      rp.yaw_info_thresh = std::stod(argv[++i]);
    } else if (a == "--enable_perf") {
      pip.enable_perf = true;

    } else if (a == "--yaw_clamp_deg" && i + 1 < argc) {
      rp.yaw_clamp_deg = std::stod(argv[++i]);

    } else if (a == "--voxel_map" && HasArg(i, argc)) {
      mp.voxel_size = std::stod(argv[++i]);
    } else if (a == "--eig_update_k" && HasArg(i, argc)) {
      mp.eig_update_every_K = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--eig_min_points" && HasArg(i, argc)) {
      mp.eig_min_points = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--no_surfel_stats") {
      mp.enable_surfel_stats = false;
    } else if (a == "--no_score") {
      mp.enable_score = false;
    } else if (a == "--score_beta" && HasArg(i, argc)) {
      mp.score_beta = std::stod(argv[++i]);
    } else if (a == "--score_sigma_e" && HasArg(i, argc)) {
      mp.score_sigma_e = std::stod(argv[++i]);
    } else if (a == "--score_tau_wall" && HasArg(i, argc)) {
      mp.score_tau_wall = std::stod(argv[++i]);
    } else if (a == "--down_voxel" && HasArg(i, argc)) {
      // legacy single-output preprocess
      pp.voxel_size = std::stod(argv[++i]);
      pp.enable_voxel_downsample = true;

    } else if (a == "--split") {
      pp.enable_split_output = true;

    } else if (a == "--icp_voxel" && HasArg(i, argc)) {
      pp.icp_voxel_size = std::stod(argv[++i]);
      pp.icp_enable_voxel = true;

    } else if (a == "--map_voxel" && HasArg(i, argc)) {
      pp.map_voxel_size = std::stod(argv[++i]);
      pp.map_enable_voxel = true;

    } else if (a == "--icp_n" && HasArg(i, argc)) {
      pp.max_icp_points = static_cast<std::size_t>(std::max(0, std::stoi(argv[++i])));

    } else if (a == "--enable_async_rebuild") {
      pip.enable_async_rebuild = true;
    } else if (a == "--rebuild_min_move" && HasArg(i, argc)) {
      pip.rebuild_min_move = std::stod(argv[++i]);
    } else if (a == "--async_warmup_frames" && HasArg(i, argc)) {
      pip.async_warmup_frames = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--async_wait_first_map" && HasArg(i, argc)) {
      pip.async_wait_first_map = (std::stoi(argv[++i]) != 0);
    } else if (a == "--async_wait_timeout_ms" && HasArg(i, argc)) {
      pip.async_wait_timeout_ms = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--min_voxels_for_icp" && HasArg(i, argc)) {
      pip.min_voxels_for_icp = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--debug_map_ready") {
      pip.debug_map_ready = true;
    } else if (a == "--enable_adaptive_ring") {
      pip.enable_adaptive_ring = true;
    } else if (a == "--tau_fallback_hi" && HasArg(i, argc)) {
      pip.tau_fallback_hi = std::stod(argv[++i]);
    } else if (a == "--tau_fallback_lo" && HasArg(i, argc)) {
      pip.tau_fallback_lo = std::stod(argv[++i]);
    } else if (a == "--fallback_hi_frames" && HasArg(i, argc)) {
      pip.fallback_hi_frames = std::max(1, std::stoi(argv[++i]));
    } else if (a == "--fallback_lo_frames" && HasArg(i, argc)) {
      pip.fallback_lo_frames = std::max(1, std::stoi(argv[++i]));
    } else if (a == "--degrade_hold_frames" && HasArg(i, argc)) {
      pip.degrade_hold_frames = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--adaptive_ring_warmup" && HasArg(i, argc)) {
      pip.adaptive_ring_warmup = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--enable_assoc_map") {
      pip.enable_assoc_map = true;
    } else if (a == "--assoc_voxel" && HasArg(i, argc)) {
      pip.assoc_voxel = std::stod(argv[++i]);
    } else if (a == "--assoc_ring_fallback" && HasArg(i, argc)) {
      pip.assoc_ring_fallback = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--assoc_refine_with_fine" && HasArg(i, argc)) {
      pip.assoc_refine_with_fine = (std::stoi(argv[++i]) != 0);
    } else if (a == "--assoc_refine_ring" && HasArg(i, argc)) {
      pip.assoc_refine_ring = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--assoc_min_voxels_for_icp" && HasArg(i, argc)) {
      pip.assoc_min_voxels_for_icp = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--enable_coarse_assoc") {
      pip.enable_coarse_assoc = true;
    } else if (a == "--coarse_voxel_mul" && HasArg(i, argc)) {
      pip.coarse_voxel_mul = std::stod(argv[++i]);
    } else if (a == "--coarse_ring" && HasArg(i, argc)) {
      pip.coarse_ring = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--refine_ring" && HasArg(i, argc)) {
      pip.coarse_refine_ring = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--coarse_fallback_full" && HasArg(i, argc)) {
      pip.coarse_fallback_full = (std::stoi(argv[++i]) != 0);
    } else if (a == "--min_coarse_voxels_for_icp" && HasArg(i, argc)) {
      pip.min_coarse_voxels_for_icp = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--enable_adaptive_refine_ring") {
      pip.enable_adaptive_refine_ring = true;
    } else if (a == "--refine_ring_min" && HasArg(i, argc)) {
      pip.refine_ring_min = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--refine_ring_max" && HasArg(i, argc)) {
      pip.refine_ring_max = std::max(0, std::stoi(argv[++i]));
    } else if (a == "--fallback_ratio_hi" && HasArg(i, argc)) {
      pip.fallback_ratio_hi = std::stod(argv[++i]);
    } else if (a == "--fallback_ratio_lo" && HasArg(i, argc)) {
      pip.fallback_ratio_lo = std::stod(argv[++i]);
    } else if (a == "--coarse_hit_ratio_lo" && HasArg(i, argc)) {
      pip.coarse_hit_ratio_lo = std::stod(argv[++i]);
    } else if (a == "--adapt_window" && HasArg(i, argc)) {
      pip.adapt_window = std::max(1, std::stoi(argv[++i]));
    } else if (a == "--adapt_warmup_frames" && HasArg(i, argc)) {
      pip.adapt_warmup_frames = std::max(0, std::stoi(argv[++i]));

    } else if (a == "--no_csv") {
      pip.log_csv = false;
    } else if (a == "--no_traj") {
      pip.write_traj_tum = false;
    } else if (a == "--quiet") {
      pip.verbose = false;
    } else if (a == "--help" || a == "-h") {
      PrintUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  if (input_dir.empty() || output_dir.empty()) {
    std::cerr << "Error: --input and --output are required\n";
    PrintUsage(argv[0]);
    return 1;
  }
  if (mp.voxel_size <= 0.0) {
    std::cerr << "Error: --voxel_map must be > 0\n";
    return 1;
  }

  pip.input_dir = input_dir;
  pip.output_dir = output_dir;

  if (pip.verbose) {
    std::cout << "[run_sequence] input_dir=" << pip.input_dir << "\n";
    std::cout << "[run_sequence] output_dir=" << pip.output_dir << "\n";
    std::cout << "[run_sequence] dt=" << pip.frame_dt
              << " start=" << pip.start_frame
              << " max=" << pip.max_frames
              << " insert_every_n=" << pip.insert_every_n << "\n";

    std::cout << "[run_sequence] ICP: iters=" << rp.max_iters
              << " max_corr_dist=" << rp.max_corr_dist
              << " min_corr=" << rp.min_effective_corr
              << " max_corr_per_iter=" << rp.max_corr_per_iter
              << " alpha_p2plane=" << rp.alpha_point_to_plane
              << " score_filter=" << (rp.use_score_filter ? "ON" : "OFF")
              << " score_tau_wall=" << rp.score_tau_wall
              << " score_fallback=" << (rp.score_allow_fallback ? "ON" : "OFF")
              << " edge_first=" << (rp.use_edge_first ? "ON" : "OFF")
              << " tau_edge=" << rp.tau_edge
              << " score_weight=" << (rp.use_score_weight ? "ON" : "OFF")
              << " w_score_gain=" << rp.w_score_gain
              << " w_min=" << rp.w_min
              << " w_max=" << rp.w_max
              << " weight_only_highscore=" << (rp.weight_only_highscore ? "ON" : "OFF")
              << " ring_edge=" << rp.ring_edge
              << " ring_wall=" << rp.ring_wall
              << " ring_fallback=" << rp.ring_fallback
              << " use_ring_query=" << (rp.use_ring_query ? "ON" : "OFF")
              << " corr_budget_edge_ratio=" << rp.corr_budget_edge_ratio
              << " corr_budget_wall_ratio=" << rp.corr_budget_wall_ratio
              << " corr_budget_fallback_ratio=" << rp.corr_budget_fallback_ratio
              << " use_corr_budget=" << (rp.use_corr_budget ? "ON" : "OFF")
              << " corr_build_early_stop=" << (rp.corr_build_early_stop ? "ON" : "OFF")
              << " lm_first=" << (rp.enable_landmark_first ? "ON" : "OFF")
              << " lm_stage_max_corr=" << rp.landmark_stage_max_corr
              << " lm_stage_min_corr=" << rp.landmark_stage_min_corr
              << " lm_stage_w=" << rp.landmark_stage_weight
              << " refine_stage_mode=" << rp.refine_stage_mode
              << " refine_stage_max_corr=" << rp.refine_stage_max_corr
              << " lm_use_p2plane=" << (rp.landmark_stage_use_p2plane ? "ON" : "OFF")
              << " refine_use_p2plane=" << (rp.refine_stage_use_p2plane ? "ON" : "OFF")
              << " single_solve_weighted=" << (rp.enable_single_solve_weighted ? "ON" : "OFF")
              << " w_edge=" << rp.w_edge
              << " w_wall=" << rp.w_wall
              << " w_fallback=" << rp.w_fallback
              << " w_cap=" << rp.w_cap
              << " w_floor=" << rp.w_floor
              << " weighted_plane=" << (rp.weighted_plane ? "ON" : "OFF")
              << " weighted_norm=" << (rp.weighted_normalize ? "ON" : "OFF")
              << " easy_stop_guard=" << (rp.enable_easy_stop_guard ? "ON" : "OFF")
              << " easy_stop_min_iters=" << rp.easy_stop_min_iters
              << " legacy_yaw_freeze=" << (rp.legacy_yaw_freeze_by_cond ? "ON" : "OFF")
              << " yaw_info_thresh=" << rp.yaw_info_thresh
              << " yaw_clamp_deg=" << rp.yaw_clamp_deg
              << "\n";

    std::cout << "[run_sequence] Map: voxel=" << mp.voxel_size
              << " neighbor_ring=" << mp.max_neighbor_ring
              << " eig_update_k=" << mp.eig_update_every_K
              << " eig_min_points=" << mp.eig_min_points
              << " surfel_stats=" << (mp.enable_surfel_stats ? "ON" : "OFF")
              << " score=" << (mp.enable_score ? "ON" : "OFF")
              << " score_beta=" << mp.score_beta
              << " score_sigma_e=" << mp.score_sigma_e
              << " score_tau_wall=" << mp.score_tau_wall
              << "\n";

    std::cout << "[run_sequence] Preprocess: split=" << (pp.enable_split_output ? "ON" : "OFF")
              << " legacy_down_voxel=" << pp.voxel_size
              << " icp_voxel=" << pp.icp_voxel_size
              << " map_voxel=" << pp.map_voxel_size
              << " max_icp_points=" << pp.max_icp_points
              << "\n";
    std::cout << "[run_sequence] MapRebuild: async=" << (pip.enable_async_rebuild ? "ON" : "OFF")
              << " rebuild_min_move=" << pip.rebuild_min_move
              << " warmup_frames=" << pip.async_warmup_frames
              << " wait_first_map=" << (pip.async_wait_first_map ? "ON" : "OFF")
              << " wait_timeout_ms=" << pip.async_wait_timeout_ms
              << " min_voxels_for_icp=" << pip.min_voxels_for_icp
              << " debug_map_ready=" << (pip.debug_map_ready ? "ON" : "OFF")
              << " adaptive_ring=" << (pip.enable_adaptive_ring ? "ON" : "OFF")
              << "\n";
    std::cout << "[run_sequence] AssocMap: enable=" << (pip.enable_assoc_map ? "ON" : "OFF")
              << " voxel=" << pip.assoc_voxel
              << " ring_fallback=" << pip.assoc_ring_fallback
              << " refine_with_fine=" << (pip.assoc_refine_with_fine ? "ON" : "OFF")
              << " refine_ring=" << pip.assoc_refine_ring
              << " min_voxels=" << pip.assoc_min_voxels_for_icp
              << "\n";
    std::cout << "[run_sequence] CoarseAssoc: enable=" << (pip.enable_coarse_assoc ? "ON" : "OFF")
              << " voxel_mul=" << pip.coarse_voxel_mul
              << " coarse_ring=" << pip.coarse_ring
              << " refine_ring=" << pip.coarse_refine_ring
              << " fallback_full=" << (pip.coarse_fallback_full ? "ON" : "OFF")
              << " min_voxels=" << pip.min_coarse_voxels_for_icp
              << "\n";
    std::cout << "[run_sequence] AdaptiveRefine: enable="
              << (pip.enable_adaptive_refine_ring ? "ON" : "OFF")
              << " ring_min=" << pip.refine_ring_min
              << " ring_max=" << pip.refine_ring_max
              << " fb_hi=" << pip.fallback_ratio_hi
              << " fb_lo=" << pip.fallback_ratio_lo
              << " coarse_hit_lo=" << pip.coarse_hit_ratio_lo
              << " window=" << pip.adapt_window
              << " warmup=" << pip.adapt_warmup_frames
              << "\n";
    std::cout << "[run_sequence] Perf: enable_perf=" << (pip.enable_perf ? "ON" : "OFF") << "\n";
  }

  // Build and run pipeline
  struct_icp::pipeline::StructICP runner(pp, rp, mp, pip);
  const bool ok = runner.RunPcdDirectory();

  std::cout << "[run_sequence] done. ok=" << ok << "\n";
  return ok ? 0 : 2;
}
