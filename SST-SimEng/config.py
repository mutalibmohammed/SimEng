import sst
import os

# Based off of https://github.com/sstsimulator/sst-tutorials/blob/master/exercises/ex1/ex1.py

sst.setProgramOption("timebase", "1ps")
sst.setProgramOption("stopAtCycle", "0s")
sst.setStatisticLoadLevel(4)

cpu_clock = "2.5GHz"
link_latency = "300ps"
cache_line_size = 128
# executable_path = "/home/br-arutterford/stream-simeng/gcc_stream_00699050"
executable_path = os.environ.get('SIMENG_EXE')
executable_args = os.environ.get('SIMENG_ARG')
config_path = os.environ.get('SIMENG_CNF')

simeng = sst.Component("SimEngElement", "SimEngElement.SimEngElement")
simeng.addParams({
    "cpu_clock":        cpu_clock,
    "cache_line_size":  cache_line_size,
    "executable_path":  executable_path,
    "config_path":      config_path,
    "executable_args":    executable_args,
})

l1 = sst.Component("l1cache", "memHierarchy.Cache")
l1.addParams({
    "cache_frequency":          cpu_clock,
    "cache_size":               "32 KiB",
    "associativity":            8,
    "access_latency_cycles":    4,
    "L1":                       1,
    "cache_line_size":          cache_line_size,
    "replacement_policy": "lru",
    "coherence_protocol": "MESI",
    "prefetcher": "cassini.StridePrefetcher",
    "debug": "0"
})

# print("Configuring L2...")

# l2 = sst.Component("l2cache", "memHierarchy.Cache")
# l2.addParams({
#     "cache_frequency":          cpu_clock,
#     "cache_size":               "256KB",
#     "associativity":            8,
#     "access_latency_cycles":    11,
#     "mshr_latency_cycles":      2,
#     "cache_line_size":          cache_line_size,
# })

# print("Configuring L3...")

# l3 = sst.Component("l3cache", "memHierarchy.Cache")
# l3.addParams({
#     "cache_frequency":          "2400MHz",
#     "cache_size":               "32MB",
#     "associativity":            8,
#     "access_latency_cycles":    21,
#     "mshr_latency_cycles":      2,
#     "cache_line_size":          cache_line_size,
# })

mem = sst.Component("memory", "memHierarchy.MemController")
mem.addParams({
    "clock":  "2400 MHz",
    "addr_range_start": "0",
    "addr_range_end": "10000000000"
})

# Had to add this to silence some warnings
backend = mem.setSubComponent("backend", "memHierarchy.simpleMem")
backend.addParams({
    "access_time":  "10ns",
    "mem_size":     "1030 MiB"
})

sst.setStatisticOutput("sst.statOutputCSV")
sst.enableAllStatisticsForAllComponents()

sst.setStatisticOutputOptions({
    "filepath": "output.csv"
})

cpu_cache_link = sst.Link("simeng_cache_link")
cpu_cache_link.connect((simeng, "cache_link", link_latency),
                       (l1, "high_network_0", link_latency))

# print("Configuring L1-L2 link...")

# l2_cache_link = sst.Link("l2cache_link")
# l2_cache_link.connect((l1, "low_network_0", link_latency),
#                       (l2, "high_network_0", link_latency))

# print("Configuring L2-L3 link...")

# l3_cache_link = sst.Link("l3cache_link")
# l3_cache_link.connect((l2, "low_network_0", link_latency),
#                       (l3, "high_network_0", link_latency))

# print("Configuring L3-mem link...")

mem_link = sst.Link("l1cache_mem_link")
mem_link.connect((l1, "low_network_0", link_latency),
                 (mem, "direct_link", link_latency))

# sst.setStatisticLoadLevel(1)
# sst.enableAllStatisticsForAllComponents()

# sst.setStatisticOutput("sst.statOutputCSV")
# sst.setStatisticOutputOptions({
#     "filepath":   "stats.csv",
#     "separator":  ", ",
# })
