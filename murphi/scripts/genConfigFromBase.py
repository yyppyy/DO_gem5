 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.


import os
import yaml
from copy import deepcopy
import itertools
from itertools import combinations

st_types = ['nt_store', 'store']
overflow_configs = [{'name': 'of', 'TS_MAX': 1, 'CNT_MAX': 1}, {'name': 'nof', 'TS_MAX': 4, 'CNT_MAX':4}]

def generate_partitions(n, max_groups):
    def recursive_partition(elements, remaining_groups):
        if remaining_groups == 0:
            if not elements:
                return [[]]
            else:
                return []
        if remaining_groups == 1:
            return [[set(elements)]]
        partitions = []
        min_size = max(1, len(elements) // remaining_groups)  # Ensure somewhat balanced partitions
        max_size = len(elements) - remaining_groups + 1  # Maximum size the first group can be
        # Iterate over possible sizes for the first group
        for size in range(min_size, max_size + 1):
            for first_group in combinations(elements, size):
                first_group = set(first_group)
                for rest in recursive_partition(elements - first_group, remaining_groups - 1):
                    # Construct a new partition by adding the first group
                    new_partition = [first_group] + rest
                    # Sort to ensure canonical form to avoid duplicates
                    new_partition_sorted = sorted(new_partition, key=lambda x: (len(x), sorted(x)))
                    if new_partition_sorted not in partitions:
                        partitions.append(new_partition_sorted)
        return partitions
    
    result = []
    # Generate partitions for every number of groups from 1 to max_groups
    elements = set(range(n))
    for i in range(1, max_groups + 1):
        for partition in recursive_partition(elements, i):
            if partition not in result:
                result.append(partition)

    return result

def generate_store_type_configs(config):
    rlx_stores = {}
    num_rlx_stores = 0
    for cpu in config["LITMUS_TEST"]:
        instrs = cpu["INSTR_STREAM"]
        for instr in instrs:
            if instr["INSTR_ACC"] == 'store' and instr["INSTR_CST"] == 'RLX':
                cpu_idx = cpu['CPU_IDX']
                if not cpu_idx in rlx_stores:
                    rlx_stores[cpu_idx] = {}
                rlx_stores[cpu_idx][instr['INSTR_IDX']] = ''
                num_rlx_stores += 1    
    st_type_configs = []
    binaries = list(itertools.product([0, 1], repeat=num_rlx_stores))
    for b in binaries:
        rlx_stores_copy = deepcopy(rlx_stores)
        store_idx = 0
        for cpu in rlx_stores_copy:
            instrs = rlx_stores_copy[cpu]
            for instr_idx in instrs:
                instrs[instr_idx] = st_types[b[store_idx]]
                store_idx += 1
        st_type_configs.append(rlx_stores_copy)
    return st_type_configs, binaries

def gen_config_from_base(base_dir, output_dir):
    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Walk through the directory
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith(".yml"):
                file_path = os.path.join(root, file)
                with open(file_path, 'r') as ymlfile:
                    original_config = yaml.safe_load(ymlfile)
                    num_addrs = original_config["ADR_COUNT"]
                    num_dirs = original_config["DIR_COUNT"]
                    addr_to_dirs = generate_partitions(num_addrs, num_dirs)
                    st_type_configs, st_type_binaries = generate_store_type_configs(original_config)
                    for addr_to_dir in addr_to_dirs: # enum addr to dir map
                        addr_to_dir = sorted(addr_to_dir)
                        for st_type_config, b in zip(st_type_configs, st_type_binaries): # enum st type/ no, need to enumerate all stores
                            for overflow_config in overflow_configs: # enum ts and counter overflow
                                gened_config = deepcopy(original_config)
                                
                                addr_to_dir_config = gened_config["ADDR_TO_DIR"]
                                for dir, addrs in enumerate(addr_to_dir):
                                    for addr in addrs:
                                        addr_to_dir_config[addr]["ADDR"] = addr
                                        addr_to_dir_config[addr]["DIR"] = dir
                                
                                for cpu in gened_config["LITMUS_TEST"]:
                                    cpu_idx = cpu["CPU_IDX"]
                                    if cpu_idx in st_type_config:
                                        instrs = cpu["INSTR_STREAM"]
                                        for instr in instrs:
                                            instr_idx = instr["INSTR_IDX"]
                                            if instr_idx in st_type_config[cpu_idx]:
                                                instr["INSTR_ACC"] = st_type_config[cpu_idx][instr_idx]
                                
                                gened_config["TS_MAX"] = overflow_config["TS_MAX"]
                                gened_config["CNT_MAX"] = overflow_config["CNT_MAX"]

                                post_fix = 'a2d'
                                post_fix += '_'.join(["".join(sorted([str(_) for _ in addrs])) for addrs in addr_to_dir])
                                post_fix += '.st'
                                post_fix += ''.join([str(_) for _ in b])
                                post_fix += '.'
                                post_fix += overflow_config["name"]

                                gened_file_name = f"{os.path.splitext(file)[0]}.{post_fix}.yml"
                                gened_file_name = os.path.join(output_dir, gened_file_name)
                                with open(gened_file_name, 'w') as new_ymlfile:
                                    yaml.safe_dump(gened_config, new_ymlfile)
                                    print(f"Generated config: {gened_file_name}")
            # elif file.endswith(".yml"): # classic Armv8 litmus tests imported from heterogen
            #     file_path = os.path.join(root, file)
            #     dir_of_addr0 = 0
            #     dirs_of_addr1 = [0, 1]
            #     st_types = ['nt_store', 'store']
            #     with open(file_path, 'r') as ymlfile:
            #         original_config = yaml.safe_load(ymlfile)
            #         for dir_of_addr1 in dirs_of_addr1:
            #             for st_type in st_types:
            #                 gened_config = deepcopy(original_config)
            #                 if len(gened_config["ADDR_TO_DIR"]) > 1:
            #                     gened_config["ADDR_TO_DIR"][1]["DIR"] = dir_of_addr1
            #                 for cpu in gened_config["LITMUS_TEST"]:
            #                     for instr in cpu["INSTR_STREAM"]:
            #                         if instr["INSTR_CST"] != "REL" and instr["INSTR_ACC"] == "store":
            #                             instr["INSTR_ACC"] = st_type
            #                 post_fix = ('s' if dir_of_addr0 == dir_of_addr1 else 'd')
            #                 if st_type == 'nt_store':
            #                     post_fix += '.nt'
            #                 elif st_type == 'store':
            #                     post_fix += '.reg'
            #                 gened_file_name = f"{os.path.splitext(file)[0]}.{post_fix}.yml"
            #                 gened_file_name = os.path.join(output_dir, gened_file_name)
            #                 with open(gened_file_name, 'w') as new_ymlfile:
            #                     yaml.safe_dump(gened_config, new_ymlfile)
            #                     print(f"Generated config: {gened_file_name}")

# Specify the base directory, key to change, list of new values, and output directory
base_directory = "../configs/template/"
output_directory = "../configs"

# Run the function
gen_config_from_base(base_directory, output_directory)