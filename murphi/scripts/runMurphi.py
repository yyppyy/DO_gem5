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
import subprocess
import sys
import glob
from concurrent.futures import ProcessPoolExecutor, as_completed

# Define paths and flags
MURPHIPATH = "../CMurphi"
INCLUDEPATH = os.path.join(MURPHIPATH, "include")
SRCPATH = os.path.join(MURPHIPATH, "src")
CFLAGS = "-O3"
MURPHI_FLAGS = "-b"
RUN_FLAGS = "-tv -pr -m2000"
SOURCE_DIR = "../src"
BUILD_DIR = "../build"

succ_commands = []
failed_commands = []

def run_command(command):
    try:
        print(f"Running command: {' '.join(command)}")
        subprocess.check_call(command)
        return "Success"
    except subprocess.CalledProcessError as e:
        print(f"Command failed with return code {e.returncode}")
        return f"Failed: {command}"
    except Exception as e:
        print(f"An error occurred: {e}")
        return f"Failed: {command}"

def build_cpp_from_murphi(murphi_file, cpp_file):
    if os.path.exists(murphi_file) and (not os.path.exists(cpp_file) or os.path.getmtime(murphi_file) > os.path.getmtime(cpp_file)):
        raw_file_name = os.path.splitext(murphi_file)[0]
        run_command([os.path.join(SRCPATH, "mu"), MURPHI_FLAGS, murphi_file])
        run_command(["mv", raw_file_name + '.cpp', cpp_file])

def build_executable(cpp_file, executable):
    if os.path.exists(cpp_file) and (not os.path.exists(executable) or os.path.getmtime(cpp_file) > os.path.getmtime(executable)):
        run_command(["g++", CFLAGS, "-o", executable, cpp_file, f"-I{INCLUDEPATH}"])

def run_executable(executable):
    assert(os.path.exists(executable))
    return run_command([executable] + RUN_FLAGS.split())

def end_to_end(murphi_file):
    assert(murphi_file.endswith(".m"))
    cpp_file = os.path.splitext(os.path.join(BUILD_DIR, os.path.basename(murphi_file)))[0] + '.cpp'
    build_cpp_from_murphi(murphi_file, cpp_file)
    executable = os.path.splitext(cpp_file)[0]
    build_executable(cpp_file, executable)
    return run_executable(executable)

def main(source_files):
    if source_files == ["all"]:
        source_files = glob.glob(os.path.join(SOURCE_DIR, "*.*.m"))
    elif not source_files:
        print(f"No .m files found in the directory {SOURCE_DIR}")
        return

    # executables = []
    # for murphi_file in source_files:
    #     if murphi_file.endswith(".m"):
    #         cpp_file = os.path.splitext(os.path.join(BUILD_DIR, os.path.basename(murphi_file)))[0] + '.cpp'
    #         build_cpp_from_murphi(murphi_file, cpp_file)
    #         executable = os.path.splitext(cpp_file)[0]
    #         build_executable(cpp_file, executable)
    #         executables.append(executable)

    with ProcessPoolExecutor(max_workers=12) as executor:
        futures = {executor.submit(end_to_end, murphi_file): murphi_file for murphi_file in source_files}
        for future in as_completed(futures):
            result = future.result()
            if "Success" in result:
                succ_commands.append(futures[future])
            else:
                failed_commands.append(futures[future])
                print(result)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        main(None)
    elif sys.argv[1] == "clean":
        pass
    else:
        source_files = sys.argv[1:]
        main(source_files)
        print("Passed:", succ_commands)
        print("Failed:", failed_commands)
