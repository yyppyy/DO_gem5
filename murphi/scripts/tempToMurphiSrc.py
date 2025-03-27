 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.


import re
import yaml
import sys
import os
import glob

default_config_dir = "../configs"
default_src_dir = "../src"
default_temp_dir = "../src/template"
temp_add_cpu_instr = "AddCPUInstr.m"
temp_add_one_instr = "AddOneInstr.m"
temp_check_outcome = "CheckOneOutcome.m"
temp_check_one_load = "CheckOneLoad.m"
temp_check_one_val = "CheckOneAddrVal.m"
# temp_map_one_cpu_cache = "MapOneCPUCache.m"

def read_file(file_path):
    with open(file_path, 'r') as file:
        return file.read()

def write_file(file_path, content):
    with open(file_path, 'w') as file:
        file.write(content)

# def replace_variables(template_content, config):
#     pattern = r"\{\{(.*?)\}\}"
#     matches = re.findall(pattern, template_content)
#     for match in matches:
#         var_name = match.strip()
#         if var_name in config:
#             template_content = template_content.replace(f"{{var_name}}", str(config[var_name]))
#         else:
#             print(f"Warning: Variable '{var_name}' not found in config file.")
#     return template_content


def build_murphi_src_code_form_config(config,
                                    str_src_code,
                                    str_add_cpu_instr,
                                    str_add_one_instr,
                                    str_check_outcome,
                                    str_check_one_load,
                                    str_check_one_val,
                                    ):
    src_code = str_src_code
    var_pattern = r"\{\{(.*?)\}\}"
    matches = re.findall(var_pattern, src_code)
    for match in matches:
        var = match.strip()
        if var == "FUNC_BODY_ADD_CPU_INSTR":
            func_body_add_instr = []
            litmus_tests = config["LITMUS_TEST"]
            for cpu_instr in litmus_tests:
                cpu_idx = cpu_instr["CPU_IDX"]
                instr_stream = cpu_instr["INSTR_STREAM"]
                all_add_one_istr = []
                for instr in instr_stream:
                    idx = instr["INSTR_IDX"]
                    acc = instr["INSTR_ACC"]
                    cst = instr["INSTR_CST"]
                    addr = instr["INSTR_ADDR"]
                    val = instr["INSTR_VAL"]
                    add_one_instr = str_add_one_instr\
                        .replace("{{INSTR_IDX}}", str(idx))\
                        .replace("{{INSTR_ACC}}", str(acc))\
                        .replace("{{INSTR_CST}}", str(cst))\
                        .replace("{{INSTR_ADDR}}", str(addr))\
                        .replace("{{INSTR_VAL}}", str(val))
                    all_add_one_istr.append(add_one_instr)
                all_add_one_istr = "\n".join(all_add_one_istr)
                add_cpu_instr = str_add_cpu_instr\
                    .replace("{{CPU_IDX}}", "CPU" + str(cpu_idx))\
                    .replace("{{FUNC_BODY_ADD_ONE_INSTR}}", all_add_one_istr)\
                    .replace("{{INSTR_COUNT}}", str(len(instr_stream)))
                func_body_add_instr.append(add_cpu_instr)
            func_body_add_instr = "\n\n\t\t\t".join(func_body_add_instr)
            src_code = src_code\
                .replace("{{FUNC_BODY_ADD_CPU_INSTR}}", func_body_add_instr)
        elif var == "FUNC_BODY_FORBIDDEN":
            if "CHECK_FORBIDDEN" in config and config["CHECK_FORBIDDEN"] == 1:
                func_body_check_forbidden = []
                forbidden_outcome = config["FORBIDDEN_OUTCOME"]
                for outcome in forbidden_outcome:
                    illegal_cond = []
                    if "LOAD_OUTCOME" in outcome:
                        load_outcome = outcome["LOAD_OUTCOME"]
                        for one_load_outcome in load_outcome:
                            cpu_idx = one_load_outcome["CPU_IDX"]
                            instr_idx = one_load_outcome["INSTR_IDX"]
                            exp_val = one_load_outcome["INSTR_VAL"]
                            one_load_illegal_cond = str_check_one_load\
                                .replace("{{CPU_IDX}}", "CPU" + str(cpu_idx))\
                                .replace("{{INSTR_IDX}}", str(instr_idx))\
                                .replace("{{EXP_VAL}}", str(exp_val))
                            illegal_cond.append(one_load_illegal_cond)
                    if "VAL_OUTCOME" in outcome:
                        val_outcome = outcome["VAL_OUTCOME"]
                        for one_val_outcome in val_outcome:
                            addr = one_val_outcome["ADDR"]
                            val = one_val_outcome["VAL"]
                            one_val_illegal_cond = str_check_one_val\
                                .replace("{{ADDR}}", str(addr))\
                                .replace("{{VAL}}", str(val))
                            illegal_cond.append(one_val_illegal_cond)
                    illegal_cond = " & ".join(illegal_cond)
                    illegal_cond = str_check_outcome\
                        .replace("{{LOAD_VAL_COND}}", illegal_cond)\
                        .replace("{{ACTION}}", "return false")
                    func_body_check_forbidden.append(illegal_cond)
                # print(forbidden_outcome)
                func_body_check_forbidden = "\n".join(func_body_check_forbidden)
                src_code = src_code\
                    .replace("{{FUNC_BODY_FORBIDDEN}}", func_body_check_forbidden + "\n\t\treturn true;")
            else:
                src_code = src_code\
                    .replace("{{FUNC_BODY_FORBIDDEN}}", "\n\t\treturn true;")
        elif var == "FUNC_BODY_EXPECTED":
            if "CHECK_EXPECTED" in config and config["CHECK_EXPECTED"] == 1:
                func_body_check_expected = []
                expected_outcome = config["EXPECTED_OUTCOME"]
                for outcome in expected_outcome:
                    load_outcome = outcome["LOAD_OUTCOME"]
                    load_legal_cond = []
                    for one_load_outcome in load_outcome:
                        cpu_idx = one_load_outcome["CPU_IDX"]
                        instr_idx = one_load_outcome["INSTR_IDX"]
                        exp_val = one_load_outcome["INSTR_VAL"]
                        one_load_legal_cond = str_check_one_load\
                            .replace("{{CPU_IDX}}", "CPU" + str(cpu_idx))\
                            .replace("{{INSTR_IDX}}", str(instr_idx))\
                            .replace("{{EXP_VAL}}", str(exp_val))
                        load_legal_cond.append(one_load_legal_cond)
                    load_legal_cond = " & ".join(load_legal_cond)
                    load_legal_cond = str_check_outcome\
                        .replace("{{LOAD_VAL_COND}}", load_legal_cond)\
                        .replace("{{ACTION}}", "put \"expected outcome appeared\"")
                    func_body_check_expected.append(load_legal_cond)
                func_body_check_expected = "\n".join(func_body_check_expected)
                src_code = src_code\
                    .replace("{{FUNC_BODY_EXPECTED}}", func_body_check_expected + "\n\t\treturn true;")
            else:
                src_code = src_code\
                    .replace("{{FUNC_BODY_EXPECTED}}", "\n\t\treturn true;")
        elif var == "ENUM_CPU_LIST":
            cpu_list = []
            for idx in range(int(config["CPU_COUNT"])):
                cpu_list.append("CPU" + str(idx))
            cpu_list = ", ".join(cpu_list)
            src_code = src_code\
                .replace("{{ENUM_CPU_LIST}}", cpu_list)
        elif var == "FUNC_BODY_CPU_NEXT":
            # func_body_cpu_next = "\t\t\tif isundefined(cur) then\n\t\t\t\treturn CPU0;\n\t\t\tendif;\n"
            func_body_cpu_next = ""
            cpu_count = int(config["CPU_COUNT"])
            for idx in range(cpu_count):
                func_body_cpu_next += f"\t\t\tif cur = CPU{idx} then\n\t\t\t\treturn CPU{(idx + 1) % cpu_count};\n\t\t\tendif;\n"
            func_body_cpu_next += "\t\t\treturn CPU0"
            src_code = src_code\
                .replace("{{FUNC_BODY_CPU_NEXT}}", func_body_cpu_next)
        elif var == "ENUM_DIR_LIST":
            dir_list = []
            for idx in range(int(config["DIR_COUNT"])):
                dir_list.append("DIR" + str(idx))
            dir_list = ", ".join(dir_list)
            src_code = src_code\
                .replace("{{ENUM_DIR_LIST}}", dir_list)
        elif var == "FUNC_BODY_ADDR_TO_DIR":
            map_addr_to_dir = ""
            addr_to_dir_map = config["ADDR_TO_DIR"]
            for addr_to_dir in addr_to_dir_map:
                addr = addr_to_dir["ADDR"]
                dir = addr_to_dir["DIR"]
                map_addr_to_dir += f"\t\t\tif adr = {addr} then\n\t\t\t\treturn DIR{dir};\n\t\t\tendif;\n"
            map_addr_to_dir += "\t\t\treturn DIR0"
            src_code = src_code\
                .replace("{{FUNC_BODY_ADDR_TO_DIR}}", map_addr_to_dir)
        elif var in config:
            # print(var)
            src_code = src_code.replace(f"{{{{{var}}}}}", str(config[var]))
        else:
            print(f"Warning: Variable '{var}' not found in config file.")
    return src_code


def main(murphiTempFile, configFiles):
    if not murphiTempFile.endswith('.m'):
        print("Error: murphiTempFile must end with .m")
        return
    
    # If no config files provided, use all .yaml files in the hardcoded directory
    # if configFiles == ["nt"]:
        # configFiles = glob.glob(os.path.join(default_config_dir, '*.nt.yml'))
    if configFiles == ["all"]:
        configFiles = glob.glob(os.path.join(default_config_dir, '*.yml'))
    # elif configFiles == ["reg"]:
        # configFiles = glob.glob(os.path.join(default_config_dir, '*.reg.yml'))
    elif not configFiles:
        print(f"No .yaml files found in the directory {default_config_dir}")
        return

    # Read in the Murphi template file
    str_src_code = read_file(murphiTempFile)
    str_add_cpu_instr = read_file(os.path.join(default_temp_dir, temp_add_cpu_instr))
    str_add_one_instr = read_file(os.path.join(default_temp_dir, temp_add_one_instr))
    str_check_outcome = read_file(os.path.join(default_temp_dir, temp_check_outcome))
    str_check_one_load = read_file(os.path.join(default_temp_dir, temp_check_one_load))
    str_check_one_val = read_file(os.path.join(default_temp_dir, temp_check_one_val))
    # str_map_one_cpu_cache = read_file(os.path.join(default_src_dir, temp_map_one_cpu_cache))

    for configFile in configFiles:
        if not configFile.endswith('.yaml') and not configFile.endswith('.yml'):
            print(f"Skipping non-YAML file: {configFile}")
            continue
        
        # Read and parse the config file
        config_content = read_file(configFile)
        config = yaml.safe_load(config_content)

        # Replace variables in the template
        # new_content = replace_variables(template_content, config)
        src_code = build_murphi_src_code_form_config(config,
                                                    str_src_code,
                                                    str_add_cpu_instr,
                                                    str_add_one_instr,
                                                    str_check_outcome,
                                                    str_check_one_load,
                                                    str_check_one_val,
                                                    # str_map_one_cpu_cache
                                                    )
        
        # Generate the output file name
        config_base_name = os.path.splitext(os.path.basename(configFile))[0]
        murphi_base_name = os.path.splitext(os.path.basename(murphiTempFile))[0]
        murphi_src_file = os.path.join(default_src_dir, f"{murphi_base_name}.{config_base_name}.m")
        
        write_file(murphi_src_file, src_code)
        print(f"Generated file: {murphi_src_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python tempToMurphiSrc.py <murphiTempFile.m> [ConfigFile1.yaml ConfigFile2.yaml ...]")
    else:
        murphiTempFile = sys.argv[1]
        configFiles = sys.argv[2:]  # Remaining arguments are config files
        main(murphiTempFile, configFiles)