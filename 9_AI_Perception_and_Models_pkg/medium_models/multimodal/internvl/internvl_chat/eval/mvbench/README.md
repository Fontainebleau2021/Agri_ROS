# README for Evaluation

## 🌟 Overview

This script provides an evaluation pipeline for `MVBench`.

## 🗂️ Data Preparation

Before starting to download the data, please create the `InternVL/internvl_chat/data` folder.

### MVBench

Follow the instructions below to prepare the data:

```shell
# Step 1: Download the dataset
cd data/
huggingface-cli download --repo-type dataset --resume-download OpenGVLab/MVBench --local-dir MVBench --local-dir-use-symlinks False

# Step 2: Unzip videos
cd MVBench/video/
for file in *.zip; do unzip "$file" -d "${file%.*}"; done
cd ../../..
```

After preparation is complete, the directory structure is:

```shell
data/MVBench
├── json
│   ├── action_antonym.json
│   ├── action_count.json
│   ├── action_localization.json
│   ├── action_prediction.json
│   ├── action_sequence.json
│   ├── character_order.json
│   ├── counterfactual_inference.json
│   ├── egocentric_navigation.json
│   ├── episodic_reasoning.json
│   ├── fine_grained_action.json
│   ├── fine_grained_pose.json
│   ├── moving_attribute.json
│   ├── moving_count.json
│   ├── moving_direction.json
│   ├── object_existence.json
│   ├── object_interaction.json
│   ├── object_shuffle.json
│   ├── scene_transition.json
│   ├── state_change.json
│   └── unexpected_action.json
├── README.md
└── video
    ├── clevrer
    ├── FunQA_test
    ├── Moments_in_Time_Raw
    ├── nturgbd
    ├── perception
    ├── scene_qa
    ├── ssv2_video
    ├── sta
    ├── star
    ├── tvqa
    └── vlnqa
```

## 🏃 Evaluation Execution

To run the evaluation, execute the following command on an 8-GPU setup:

```shell
torchrun --nproc_per_node=8 eval/mvbench/evaluate_mvbench.py --checkpoint ${CHECKPOINT} --num_segments 16
```

Alternatively, you can run the following simplified command:

```shell
GPUS=8 sh evaluate.sh ${CHECKPOINT} mvbench
```

### Arguments

The following arguments can be configured for the evaluation script:

| Argument         | Type   | Default     | Description                                                                                                       |
| ---------------- | ------ | ----------- | ----------------------------------------------------------------------------------------------------------------- |
| `--checkpoint`   | `str`  | `''`        | Path to the model checkpoint.                                                                                     |
| `--datasets`     | `str`  | `'mvbench'` | Comma-separated list of datasets to evaluate.                                                                     |
| `--dynamic`      | `flag` | `False`     | Enables dynamic high resolution preprocessing.                                                                    |
| `--max-num`      | `int`  | `1`         | Maximum tile number for dynamic high resolution.                                                                  |
| `--load-in-8bit` | `flag` | `False`     | Loads the model weights in 8-bit precision.                                                                       |
| `--auto`         | `flag` | `False`     | Automatically splits a large model across 8 GPUs when needed, useful for models too large to fit on a single GPU. |
