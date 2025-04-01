/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

const size_t CACHELINE_SIZE = 64;

void GetGraphStat(const std::string filename,
              size_t* num_v,
              size_t* num_e);

void BuildCSR(const std::string filename,
              size_t* row_ptr,
              size_t* col_idx);

void* AllocREGMem(size_t count);
void* AllocNTMem(size_t count);
void* AllocRELMem(size_t count);