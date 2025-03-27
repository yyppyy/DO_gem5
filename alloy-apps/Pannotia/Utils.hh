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