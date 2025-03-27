/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[{{INSTR_IDX}}] do
            instr.access := {{INSTR_ACC}};
            instr.cst := {{INSTR_CST}};
            instr.adr := {{INSTR_ADDR}};
            instr.val := {{INSTR_VAL}};
            instr.done := false;
        endalias;
        endalias;