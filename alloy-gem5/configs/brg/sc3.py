 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.


#==============================================================================
# sc3.py
#==============================================================================
# configuration script for SC3 project
#
# Authors: Tuan Ta
# Date   : 2019/08/06

import optparse
import sys
import os
import math

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.util import addToPath, fatal, warn

addToPath('../')

from ruby import Ruby

from common import Options
from common import Simulation
from common import CacheConfig
from common import CpuConfig
from common import MemConfig
from common.Caches import *

from common.brg_utils import get_process, copy_cpu_configs

#------------------------------------------------------------------------------
# make active message network topology
#------------------------------------------------------------------------------

def memory_size_to_bytes(size_str):
    # Define size multipliers
    size_multipliers = {
        'B': 1,
        'KB': 1024,
        'MB': 1024 ** 2,
        'GB': 1024 ** 3,
        'TB': 1024 ** 4,
        'PB': 1024 ** 5
    }

    # Split the string into a number and the unit
    size_str = size_str.strip().upper()  # Remove any leading/trailing spaces and convert to uppercase
    num_str = ''.join([ch for ch in size_str if ch.isdigit() or ch == '.'])  # Extract the number part
    unit = ''.join([ch for ch in size_str if not ch.isdigit() and ch != '.']).strip()  # Extract the unit part

    if unit not in size_multipliers:
        return 0
        # raise ValueError(f"Invalid size unit: {unit}")

    # Convert the number to a float (to handle decimal sizes like 1.5GB)
    num = float(num_str)
    
    # Multiply the number by the appropriate size multiplier
    return int(num * size_multipliers[unit])

def makeActiveMsgNetworkTopology(adapters, network, IntLink, ExtLink, Router,
                                 options):
    nodes = adapters

    num_routers = len(nodes)
    num_rows = options.mesh_rows

    if num_rows == 0:
      num_rows = 1

    link_latency = 1
    router_latency = 1 # only used by garnet

    # There must be an evenly divisible number of cntrls to routers
    # Also, obviously the number or rows must be <= the number of routers
    cntrls_per_router, remainder = divmod(len(nodes), num_routers)
    assert(remainder == 0)
    assert(num_rows > 0 and num_rows <= num_routers)
    num_columns = int(num_routers / num_rows)
    assert(num_columns * num_rows == num_routers)

    # Create the routers in the mesh
    routers = [Router(router_id=i, latency = router_latency) \
        for i in range(num_routers)]
    network.routers = routers

    # link counter to set unique link ids
    link_count = 0

    # Add all but the remainder nodes to the list of nodes to be uniformly
    # distributed across the network.
    network_nodes = []
    remainder_nodes = []
    for node_index in xrange(len(nodes)):
        if node_index < (len(nodes) - remainder):
            network_nodes.append(nodes[node_index])
        else:
            remainder_nodes.append(nodes[node_index])

    # Connect each node to the appropriate router
    ext_links = []
    for (i, n) in enumerate(network_nodes):
        cntrl_level, router_id = divmod(i, num_routers)
        assert(cntrl_level < cntrls_per_router)
        ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                int_node=routers[router_id],
                                latency = link_latency))
        link_count += 1

    network.ext_links = ext_links

    # Create the mesh links.
    int_links = []

    # East output to West input links (weight = 1)
    for row in xrange(num_rows):
        for col in xrange(num_columns):
            if (col + 1 < num_columns):
                east_out = col + (row * num_columns)
                west_in = (col + 1) + (row * num_columns)
                int_links.append(IntLink(link_id=link_count,
                                         src_node=routers[east_out],
                                         dst_node=routers[west_in],
                                         src_outport="East",
                                         dst_inport="West",
                                         latency = link_latency,
                                         weight=1))
                link_count += 1

    # West output to East input links (weight = 1)
    for row in xrange(num_rows):
        for col in xrange(num_columns):
            if (col + 1 < num_columns):
                east_in = col + (row * num_columns)
                west_out = (col + 1) + (row * num_columns)
                int_links.append(IntLink(link_id=link_count,
                                         src_node=routers[west_out],
                                         dst_node=routers[east_in],
                                         src_outport="West",
                                         dst_inport="East",
                                         latency = link_latency,
                                         weight=1))
                link_count += 1

    # North output to South input links (weight = 2)
    for col in xrange(num_columns):
        for row in xrange(num_rows):
            if (row + 1 < num_rows):
                north_out = col + (row * num_columns)
                south_in = col + ((row + 1) * num_columns)
                int_links.append(IntLink(link_id=link_count,
                                         src_node=routers[north_out],
                                         dst_node=routers[south_in],
                                         src_outport="North",
                                         dst_inport="South",
                                         latency = link_latency,
                                         weight=2))
                link_count += 1

    # South output to North input links (weight = 2)
    for col in xrange(num_columns):
        for row in xrange(num_rows):
            if (row + 1 < num_rows):
                north_in = col + (row * num_columns)
                south_out = col + ((row + 1) * num_columns)
                int_links.append(IntLink(link_id=link_count,
                                         src_node=routers[south_out],
                                         dst_node=routers[north_in],
                                         src_outport="South",
                                         dst_inport="North",
                                         latency = link_latency,
                                         weight=2))
                link_count += 1


    network.int_links = int_links

#------------------------------------------------------------------------------
# Adding options
#------------------------------------------------------------------------------

parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
Options.addBRGOptions(parser)
Options.addLaneOptions(parser)
Options.addBRGSC3Options(parser)
Options.addDOOptions(parser)

if '--ruby' in sys.argv:
    Ruby.define_options(parser)

(options, args) = parser.parse_args()

if args:
    print("Error: script doesn't take any positional arguments")
    sys.exit(1)

if options.cmd:
    process = get_process(options)
else:
    print("No workload specified. Exiting!\n")
    sys.exit(1)

#------------------------------------------------------------------------------
# Create CPU instances
#------------------------------------------------------------------------------
#
# Warm-up CPUs are used before ROI
# Main CPUs are used inside ROI
# Cool-down CPUs are used after ROI
#

WarmupCPUClass   = NonCachingSimpleCPU
MainCPUClass, _  = Simulation.getCPUClass(options.cpu_type)
CooldownCPUClass = NonCachingSimpleCPU

WarmupCPUClass.numThreads   = options.nthreads_per_cpu
MainCPUClass.numThreads     = options.nthreads_per_cpu
CooldownCPUClass.numThreads = options.nthreads_per_cpu

np = options.num_cpus
num_big_cpus  = np
num_tiny_cpus = 0

# Number of CPUs
if options.big_tiny:
  num_tiny_cpus = options.num_tiny_cpus
  num_big_cpus  = np - num_tiny_cpus

assert(np == num_tiny_cpus + num_big_cpus)

warmup_cpu_list = None
main_cpu_list = None
cooldown_cpu_list = None

main_switched_out = False

if (options.brg_fast_forward):
  warmup_cpu_list   = [ WarmupCPUClass(cpu_id = i) \
                                        for i in xrange(np) ]
  cooldown_cpu_list = [ CooldownCPUClass(switched_out = True, cpu_id = i) \
                                        for i in xrange(np) ]
  main_switched_out = True

if options.big_tiny:
  TinyCPUClass, _ = Simulation.getCPUClass(options.tiny_cpu_type)
  main_cpu_list = []
  for i in xrange(num_big_cpus):
    main_cpu_list.append(MainCPUClass(switched_out = main_switched_out, \
                                      cpu_id = i))
  for i in xrange(num_big_cpus, np):
    main_cpu_list.append(TinyCPUClass(switched_out = main_switched_out, \
                                      cpu_id = i))
else:
  main_cpu_list = [ MainCPUClass(switched_out = main_switched_out, \
                                 cpu_id = i) for i in xrange(np) ]

assert(len(main_cpu_list) == np)

#------------------------------------------------------------------------------
# Create the top-level system
#------------------------------------------------------------------------------

# If fast_forward is enabled, the system will start with WarmupCPUClass.
# Otherwise, it will start with MainCPUClass

if (options.brg_fast_forward):
  system = System(cpu             = warmup_cpu_list,
                  mem_mode        = WarmupCPUClass.memory_mode(),
                  mem_ranges      = [AddrRange(options.mem_size)],
                  cache_line_size = options.cacheline_size)
else:
  system = System(cpu             = main_cpu_list,
                  mem_mode        = MainCPUClass.memory_mode(),
                  mem_ranges      = [AddrRange(options.mem_size)],
                  cache_line_size = options.cacheline_size)

## True if using SMT CPU
#system.multi_thread = True

# Create a top-level voltage domain
system.voltage_domain = VoltageDomain(voltage = options.sys_voltage)

# Create a source clock for the system and set the clock period
system.clk_domain = SrcClockDomain(clock =  options.sys_clock,
                                   voltage_domain = system.voltage_domain)

# Create a CPU voltage domain
system.cpu_voltage_domain = VoltageDomain()

# Create a separate clock domain for the CPUs
system.cpu_clk_domain = SrcClockDomain(clock = options.cpu_clock,
                                       voltage_domain =
                                       system.cpu_voltage_domain)

# Configure CPUs
for cpu in system.cpu:
  # All cpus belong to a common cpu_clk_domain, therefore running at a common
  # frequency.
  cpu.clk_domain = system.cpu_clk_domain
  # Assume single-workload simulation. All CPUs are mapped to the same process
  cpu.workload   = process
  cpu.createThreads()
  # Set activity trace flags for all CPUs in the system
  cpu.activity_trace = options.activity_trace

# Set brg fast-forward for the system
system.brg_fast_forward = options.brg_fast_forward
system.num_nodes = options.num_l2caches
system.num_cores = options.num_cpus

#------------------------------------------------------------------------------
# Create memory system
#------------------------------------------------------------------------------

if options.ruby:
  Ruby.create_system(options, False, system)
  
  system.ruby.l2_bits = int(math.log(options.num_l2caches, 2))
  system.ruby.real_phys_mem_bits = int(math.log(memory_size_to_bytes(options.mem_size), 2))
  
  assert(options.num_cpus == len(system.ruby._cpu_ports))

  system.ruby.clk_domain = SrcClockDomain(clock = options.ruby_clock,
                                          voltage_domain = \
                                                        system.voltage_domain)
  for i in xrange(np):
    ruby_port = system.ruby._cpu_ports[i]

    # Create the interrupt controller and connect its ports to Ruby
    # Note that the interrupt controller is always present but only
    # in x86 does it have message ports that need to be connected
    system.cpu[i].createInterruptController()

    # Connect the cpu's cache ports to Ruby
    system.cpu[i].icache_port = ruby_port.slave
    system.cpu[i].dcache_port = ruby_port.slave
    if buildEnv['TARGET_ISA'] == 'x86':
      system.cpu[i].interrupts[0].pio = ruby_port.master
      system.cpu[i].interrupts[0].int_master = ruby_port.slave
      system.cpu[i].interrupts[0].int_slave = ruby_port.master
      system.cpu[i].itb.walker.port = ruby_port.slave
      system.cpu[i].dtb.walker.port = ruby_port.slave
else:
  MemClass = Simulation.setMemClass(options)
  system.membus = SystemXBar()
  system.system_port = system.membus.slave
  CacheConfig.config_cache(options, system)
  MemConfig.config_mem(options, system)

#------------------------------------------------------------------------------
# Create active message network
#------------------------------------------------------------------------------

if options.active_message_network and np >= 2:
  ruby = RubySystem()
  ruby.num_of_sequencers = 0
  ruby.number_of_virtual_networks = 2

  system.ruby_am = ruby

  am_network = SimpleNetwork( ruby_system = ruby,
                              routers = [],
                              ext_links = [],
                              int_links = [],
                              netifs = [],
                              number_of_virtual_networks = 2,
                              is_active_msg_network = True)

  # construct adapters
  adapters = []
  for i in xrange(np):
    adapter = NetworkAdapter(version = i, ruby_system = ruby)

    # connect adapter and the network
    adapter.cpuReqBuffer            = MessageBuffer(ordered = True)
    adapter.cpuReqBuffer.master     = am_network.slave

    adapter.networkRespBuffer       = MessageBuffer(ordered = True)
    adapter.networkRespBuffer.slave = am_network.master

    adapter.networkReqBuffer        = MessageBuffer(ordered = True)
    adapter.networkReqBuffer.slave  = am_network.master

    adapter.cpuRespBuffer           = MessageBuffer(ordered = True)
    adapter.cpuRespBuffer.master    = am_network.slave

    adapters.append(adapter)

  system.adapters = adapters

  # construct Mesh_XY topology
  makeActiveMsgNetworkTopology(adapters, am_network, SimpleIntLink, \
                                SimpleExtLink, Switch, options)

  # init network
  am_network.buffer_size = 0  # infinite size
  am_network.setup_buffers()

  system.am_network = am_network

  # connect CPUs to adapters
  for i in xrange(np):
    system.cpu[i].out_nw_req_port = adapters[i].cpu_req_port
    adapters[i].network_req_port  = system.cpu[i].in_nw_req_port

#------------------------------------------------------------------------------
# Create root object
#------------------------------------------------------------------------------

root = Root(full_system = False, system = system)

#------------------------------------------------------------------------------
# Set up for fast-forward mode
#------------------------------------------------------------------------------

if options.brg_fast_forward:
  for i in xrange(np):
    copy_cpu_configs(warmup_cpu_list[i], main_cpu_list[i])
    copy_cpu_configs(main_cpu_list[i],   cooldown_cpu_list[i])

  system.main_cpu     = main_cpu_list
  system.cooldown_cpu = cooldown_cpu_list

  switch_warmup_cpu_pairs   = [ ( warmup_cpu_list[i], main_cpu_list[i] ) \
                                        for i in xrange(np) ]
  switch_cooldown_cpu_pairs = [ ( main_cpu_list[i], cooldown_cpu_list[i] ) \
                                        for i in xrange(np) ]

#------------------------------------------------------------------------------
# Instantiate all m5 objects
#------------------------------------------------------------------------------

checkpoint_dir = None
m5.instantiate(checkpoint_dir)

maxtick = m5.MaxTick
if options.abs_max_tick:
    maxtick = options.abs_max_tick

#------------------------------------------------------------------------------
# Run simulation
#------------------------------------------------------------------------------

#
# check if SIGINT has killed the simulation
#
def checkExitEvent(exit_event):
  if exit_event is not None and exit_event.getCode() != 0:
    print('Exiting @ tick %i because %s. Exit code %i' % \
              (m5.curTick(), exit_event.getCause(), exit_event.getCode()))
    sys.exit(exit_event.getCode())

if options.brg_fast_forward:
  print("\n\n----- Entering warmup simulation -----\n")
  exit_event = m5.simulate(maxtick)
  print("\n\n----- Exiting warmup simulation @ tick %i because %s -----\n\n" %\
                                (m5.curTick(), exit_event.getCause()))
  checkExitEvent(exit_event)

  print("\n\n----- Switching to main CPUs ----\n")
  exit_event = m5.switchCpus(system, switch_warmup_cpu_pairs)
  checkExitEvent(exit_event)

  print("\n\n----- Entering main simulation -----\n")
  exit_event = m5.simulate(maxtick - m5.curTick())
  print("\n\n----- Exiting main simulation @ tick %i because %s -----\n\n" %\
                                (m5.curTick(), exit_event.getCause()))
  checkExitEvent(exit_event)

  # FIXME: @Tuan Switching back to non-caching atomic simple CPU is not working
  # yet with Ruby, so turn it off for now.
  warn("Cooldown phase that may contain verification code is off\n")

#  print("\n\n----- Switching to cooldown CPUs -----\n")
#  exit_event = m5.switchCpus(system, switch_cooldown_cpu_pairs)
#  checkExitEvent(exit_event)
#
#  print("\n\n----- Entering cooldown simulation -----\n")
#  exit_event = m5.simulate(maxtick - m5.curTick())
#  print("\n\n----- Exiting cooldown simulation @ tick %i because %s ----\n\n"\
#                              % (m5.curTick(), exit_event.getCause()))
#  checkExitEvent(exit_event)
else:
  print("\n\n----- Entering main simulation -----\n")
  exit_event = m5.simulate(maxtick)
  print("\n\n----- Exiting main simulation @ tick %i because %s -----\n\n" % \
                                (m5.curTick(), exit_event.getCause()))

print('Exit code %i' % exit_event.getCode())

# Set exit code
sys.exit(exit_event.getCode())
