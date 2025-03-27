#!/bin/bash

CONTAINER_NAME="sc3_container"

echo() {
    command echo -e "\e[1;33m$*\e[0m"
}

echo "Running experiments..."
docker exec $CONTAINER_NAME bash -c "cd /artifact_top/DO_gem5 && mkdir -p logs && mkdir -p m5out" || { echo "Directory creation failed"; exit 1; }

# Run macro benchmarks
echo "Running macro benchmarks..."
docker exec $CONTAINER_NAME bash -c "cd /artifact_top/DO_gem5 && scripts/run_macro_benchmarks.sh" || { echo "Macro benchmarks failed"; exit 1; }
echo "Macro benchmarks complete..."

# Run sensitivity analysis
echo "Running sensitivity analysis..."
docker exec $CONTAINER_NAME bash -c "cd /artifact_top/DO_gem5 && scripts/run_sensitivity_analysis.sh" || { echo "Sensitivity analysis failed"; exit 1; }
echo "Sensitivity analysis complete..."

# Run storage overhead
echo "Running storage overhead..."
docker exec $CONTAINER_NAME bash -c "cd /artifact_top/DO_gem5 && scripts/run_storage.sh" || { echo "Storage overhead failed"; exit 1; }
echo "Storage overhead complete..."

# Run latency cutoff analysis
echo "Running latency cutoff analysis..."
docker exec $CONTAINER_NAME bash -c "cd /artifact_top/DO_gem5 && scripts/run_latency_cutoff.sh" || { echo "Latency cutoff analysis failed"; exit 1; }
echo "Latency cutoff analysis complete..."

# Run macro benchmarks for TSO
echo "Running macro benchmarks for TSO..."
docker exec $CONTAINER_NAME bash -c "cd /artifact_top/DO_gem5 && scripts/run_macro_benchmarks_tso.sh" || { echo "Macro benchmarks for TSO failed"; exit 1; }
echo "Macro benchmarks for TSO complete..."

echo "Running Murphi for model checking..."
cd murphi && ./run.sh && cd ..
echo "Murphi for model checking complete."

echo "All experiments complete."