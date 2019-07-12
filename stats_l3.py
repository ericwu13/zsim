#!/usr/bin/python
# zsim stats README
# Author: Daniel Sanchez <sanchezd@stanford.edu>
# Date: May 3 2011
#
# Stats are now saved in HDF5, and you should never need to write a stats
# parser. This README explains how to access them in python using h5py. It
# doubles as a python script, so you can just execute it with "python
# README.stats" and see how everything works (after you have generated a stats
# file).
#

import h5py # presents HDF5 files as numpy arrays
import numpy as np
import sys
print ""
print "{} {} {}".format("{:=<20}".format(""), sys.argv[1], "{:=>20}".format(""))

# Open stats file
f = h5py.File(sys.argv[1], 'r')

# Get the single dataset in the file
dset = f["stats"]["root"]

# Each dataset is first indexed by record. A record is a snapshot of all the
# stats taken at a specific time.  All stats files have at least two records,
# at beginning (dest[0])and end of simulation (dset[-1]).  Inside each record,
# the format follows the structure of the simulated objects. A few examples:

# Phase count at end of simulation
endPhase = dset[-1]['phase']
width = 15
width_2 = 30
print "Phase Counts",
print endPhase

# Hits into all L3s
l3_misses = np.sum(dset[-1]['l3']['mGETS'] + dset[-1]['l3']['mGETXIM'] + dset[-1]['l3']['mGETXSM'])
l3_hits = np.sum(dset[-1]['l3']['hGETS'] + dset[-1]['l3']['hGETX'])
l3_access = l3_hits + l3_misses
l3_MRUhits = np.sum(dset[-1]['l3']['hMRU'])

print "{: <{}}:".format("L3 Total Accesses", width_2),
print "{: >{}}".format("{:,}".format(l3_access), width)

print "{: <{}}:".format("  - L3 Misses", width_2),
print "{: >{}}".format("{:,}".format(l3_misses), width)

print "{: <{}}:".format("  - L3 Hits", width_2),
# print "{:,} read({:,}) + write({:,})".format(l3_hits, np.sum(dset[-1]['l3']['hGETS']), np.sum(dset[-1]['l3']['hGETX']))

print "{: >{}}".format("{:,}".format(l3_hits), width)

print "{: <{}}:".format("     - L3 MRU Hits", width_2),
# print "{:,} read({:,}) + write({:,})".format(l3_hits, np.sum(dset[-1]['l3']['hGETS']), np.sum(dset[-1]['l3']['hGETX']))
print "{: >{}}".format("{:,}".format(l3_MRUhits), width)

print "{: <{}}:".format("     * L3 MRU Hit Rate", width_2),
print "{: >{}}".format("{:.2%}".format(l3_MRUhits/ float(l3_hits)), width)

print "{: <{}}:".format("  * L3 Miss Rate", width_2),
print "{: >{}}".format("{:.2%}".format(l3_misses / float(l3_access)), width)









# print "L2 Latency ", 
# print l2_latency

l2_latency = np.sum(dset[-1]['l2']['latGETnl'])
print "{: <{}}:".format("L2 Latency", width_2),
print "{: >{}}".format("{:,}".format(l2_latency), width)
# Total number of instructions executed, counted by adding per-core counts
# (you could also look at procInstrs)
# totalInstrs = np.sum(dset[-1]['simpleCore']['instrs'])
# print "Total Instructon Counts: ",
# print totalInstrs

totalCycle = np.sum(dset[-1]['simpleCore']['cycles'])
totalInst = np.sum(dset[-1]['simpleCore']['instrs'])

print "{: <{}}:".format("Instruction Counts", width_2),
print "{: >{}}".format("{:,}".format(totalInst), width)

print "{: <{}}:".format("Cycle Counts", width_2),
print "{: >{}}".format("{:,}".format(totalCycle), width)

print "{: <{}}:".format("  * IPC", width_2),
print "{: >{}}".format("{:.5f}".format(totalInst / float(totalCycle)), width)



print "{:=>{}}".format("", 21+21+len(sys.argv[1]))
print "",

