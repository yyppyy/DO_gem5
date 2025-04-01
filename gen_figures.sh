#!/bin/bash

 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.

CONTAINER_NAME="sc3_container"

echo() {
    command echo -e "\e[1;33m$*\e[0m"
}

echo "Copying results from docker container..."
mkdir -p figures
mkdir -p results
mkdir -p results/logs
mkdir -p results/m5out
docker cp $CONTAINER_NAME:/artifact_top/DO_gem5/logs/. results/logs/
docker cp $CONTAINER_NAME:/artifact_top/DO_gem5/m5out/. results/m5out/

echo "Processing results..."
cd scripts
python3 process_macro_benchmark.py
python3 process_sensitivity_analysis.py
python3 process_seq_no.py
python3 process_storage.py
python3 process_latency_cutoff.py
python3 process_macro_benchmark_tso.py

echo "Generating figures..."
cd ../plots
python3 plot_motivation_so_perf_traffic_overhead.py
python3 plot_macro_perf_traffic.py
python3 plot_sensitivity_analysis.py
python3 plot_latency_cutoff.py
python3 plot_seq_no.py
python3 plot_storage_overhead.py
python3 plot_storage_breakdown.py
python3 plot_macro_perf_traffic_tso.py

echo "Generating tables..."
cd ../cacti
python3 run.py

echo "Figures and tables generated under figures/"
echo "Artifact evaluation complete"