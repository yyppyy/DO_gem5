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

set -e  # Exit immediately if a command fails
set -o pipefail  # Ensure pipeline errors are caught

echo() {
    command echo -e "\e[1;33m$*\e[0m"
}

# Step 1: Download the Docker image if not already present
DOCKER_IMAGE="wang-hcc-dts-docker-bin-isca2020.tar.gz"
DOCKER_URL="https://zenodo.org/records/3910803/files/$DOCKER_IMAGE?download=1"
GITHUB_URL="https://github.com/yyppyy/DO_gem5.git"

if [ ! -f "$DOCKER_IMAGE" ]; then
    echo "Downloading Docker image..."
    wget -O $DOCKER_IMAGE "$DOCKER_URL" || { echo "Failed to download Docker image"; exit 1; }
else
    echo "Docker image already downloaded."
fi

# Step 2: Check if the Docker image is already loaded
IMAGE_NAME="sc3"
if docker images | grep -q "$IMAGE_NAME"; then
    echo "Docker image $IMAGE_NAME is already loaded. Skipping load step."
else
    echo "Loading Docker image..."
    docker load < $DOCKER_IMAGE || { echo "Failed to load Docker image"; exit 1; }
fi

# Step 3: Start Docker container in detached mode
CONTAINER_NAME="sc3_container"

# Check if the container is already running
if [ "$(docker ps -q -f name=$CONTAINER_NAME)" ]; then
    echo "Container $CONTAINER_NAME is already running."
elif [ "$(docker ps -aq -f name=$CONTAINER_NAME)" ]; then
    echo "Container $CONTAINER_NAME exists but is stopped. Restarting..."
    docker start $CONTAINER_NAME || { echo "Failed to restart container"; exit 1; }
else
    echo "Starting new container..."
    docker run -dit --cap-add SYS_ADMIN --name $CONTAINER_NAME $IMAGE_NAME /bin/bash || { echo "Failed to start container"; exit 1; }
fi

# Step 4: Clone repository and run setup inside the container
echo "Setting up gem5 inside the container..."
docker exec $CONTAINER_NAME bash -c "
    cd /artifact_top &&
    if [ ! -d 'DO_gem5/.git' ]; then
        git clone $GITHUB_URL;
    else
        echo 'Repository already exists, skipping clone.';
    fi &&
    cd DO_gem5 &&
    ./setup_docker.sh
" || { echo 'Setup failed inside container'; exit 1; }

echo "Setting up Cacti"
cd cacti && ./compile.sh && cd ..

echo "Setting up Murphi"
cd murphi && ./setup.sh && cd ..

echo "Setup complete."
