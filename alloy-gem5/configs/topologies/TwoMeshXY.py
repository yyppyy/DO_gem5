 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.


# Copyright (c) 2010 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Brad Beckmann

from m5.params import *
from m5.objects import *

from BaseTopology import SimpleTopology

# Creates a Mesh topology with 4 directories, one at each corner.
# One L1 (and L2, depending on the protocol) are connected to each router.
# XY routing is enforced (using link weights) to guarantee deadlock freedom.

class TwoMeshXY(SimpleTopology):
    description='TwoMeshXY'

    def __init__(self, controllers):
        self.nodes = controllers

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes
        num_cpus = options.num_cpus
        
        # default values for link latency and router latency.
        # Can be over-ridden on a per link/router basis
        link_latency = options.link_latency # used by simple and garnet
        interPU_link_latency = options.CG_link_latency
        router_latency = options.router_latency # only used by garnet


        # First determine which nodes are cache cntrls vs. dirs vs. dma
        cache_nodes = []
        l2cache_nodes = []
        dir_nodes = []
        dma_nodes = []
        for node in nodes:
            if node.type == 'L1Cache_Controller' or \
               node.type == 'SC3_Controller' or \
               node.type == 'DeNovo_Controller':
                cache_nodes.append(node)
            elif node.type == 'L2Cache_Controller':
                l2cache_nodes.append(node)
            elif node.type == 'Directory_Controller':
                dir_nodes.append(node)
            elif node.type == 'DMA_Controller':
                dma_nodes.append(node)

        assert(len(cache_nodes) == num_cpus)
        assert(len(l2cache_nodes) == len(dir_nodes))
        assert(num_cpus % len(l2cache_nodes) == 0)
        num_l2caches = len(l2cache_nodes)
        num_routers = num_cpus + num_l2caches + 1
        num_cpus_per_l2cache = num_cpus // num_l2caches

        # Create the routers in the mesh
        routers = [Router(router_id=i, latency = router_latency) \
            for i in range(num_routers)]
        network.routers = routers

        # link counter to set unique link ids
        link_count = 0

        # Connect each L1 cache controller to the appropriate router
        ext_links = []
        for (i, n) in enumerate(cache_nodes):
            router_id = i
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id],
                                    latency = link_latency))
            link_count += 1

        for (i, n) in enumerate(l2cache_nodes):
            router_id = num_cpus + i
            ext_links.append(
                ExtLink(link_id=link_count, ext_node=n,
                        int_node=routers[router_id],
                        latency = link_latency)
            )
            link_count += 1

        for (i, n) in enumerate(dir_nodes):
            router_id = num_cpus + i
            ext_links.append(
                ExtLink(link_id=link_count, ext_node=n,
                        int_node=routers[router_id],
                        latency = link_latency)
            )
            link_count += 1

        # Connect the dma nodes to router 0.  These should only be DMA nodes.
        for (i, node) in enumerate(dma_nodes):
            assert(node.type == 'DMA_Controller')
            ext_links.append(ExtLink(link_id=link_count, ext_node=node,
                                     int_node=routers[0],
                                     latency = link_latency))
            link_count += 1

        network.ext_links = ext_links

        # Create the mesh links.
        int_links = []
        
        for router_id in range(num_cpus):
            root_router_id = num_cpus + (router_id // num_cpus_per_l2cache)
            int_links.append(IntLink(link_id=link_count,
                                     src_node=routers[router_id],
                                     dst_node=routers[root_router_id],
                                     latency = link_latency))
            link_count += 1
            int_links.append(IntLink(link_id=link_count,
                                     src_node=routers[root_router_id],
                                     dst_node=routers[router_id],
                                     latency = link_latency))
            link_count += 1
        
        for router_id in range(num_cpus, num_cpus + num_l2caches):
            root_router_id = num_cpus + num_l2caches
            int_links.append(IntLink(link_id=link_count,
                                     src_node=routers[router_id],
                                     dst_node=routers[root_router_id],
                                     latency = interPU_link_latency))
            link_count += 1
            int_links.append(IntLink(link_id=link_count,
                                     src_node=routers[root_router_id],
                                     dst_node=routers[router_id],
                                     latency = interPU_link_latency))
            link_count += 1

        network.int_links = int_links
