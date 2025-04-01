# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
# AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

import numpy as np
import argparse

def build_csr_from_file(input_file, out_nbr=True):
    with open(input_file, 'r') as f:
        # Skip comments (assuming they start with non-numeric characters)
        line = f.readline().strip()
        while not line[0].isdigit():
            line = f.readline().strip()

        # Read number of vertices and edges
        num_vertices, _, num_edges = map(int, line.split())

        # Initialize CSR components
        row_ptr = np.zeros(num_vertices + 1, dtype=int)
        col_idx = []
        
        # to dedup
        nbrlist = {}

        # Read edges and build col_idx
        for line in f:
            tokens = line.split()
            src = int(tokens[0]) - 1  # Convert from 1-based to 0-based
            dst = int(tokens[1]) - 1  # Convert from 1-based to 0-based
            if src == dst:
                continue

            if out_nbr:
                u = src
                v = dst
            else:
                u = dst
                v = src
            
            if u not in nbrlist:
                nbrlist[u] = []
            nbrlist[u].append(v)
        
        accum = 0
        for u in range(num_vertices):
            row_ptr[u] = accum
            if u in nbrlist:
                col_idx += sorted(nbrlist[u])
                accum += len(nbrlist[u])
        
        # Convert col_idx to numpy array
        col_idx = np.array(col_idx, dtype=int)

        return row_ptr, col_idx

def save_csr_to_file(output_file, row_ptr, col_idx):
    # Save as human-readable format in the same file
    with open(output_file, 'w') as f:
        f.write("%d\n" % (len(row_ptr)))  # Number of vertices
        f.write("%d\n" % len(col_idx))        # Number of edges
        f.write(" ".join(map(str, row_ptr)) + "\n")
        f.write(" ".join(map(str, col_idx)) + "\n")
    
    print(f"CSR format saved in {output_file}")

def main():
    parser = argparse.ArgumentParser(description="Convert a graph file to CSR format (1-based to 0-based index, with optional weights).")
    parser.add_argument('graph_file', help="Input graph file")
    parser.add_argument('csr_file', help="Output CSR file")
    parser.add_argument('out_nbr', help="CSR represent in/out nbrs")
    args = parser.parse_args()

    # Build CSR from input graph file
    row_ptr, col_idx = build_csr_from_file(args.graph_file, args.out_nbr)

    # Save CSR to human-readable text file (same as graph file)
    save_csr_to_file(args.csr_file, row_ptr, col_idx)

if __name__ == '__main__':
    main()
