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
import pprint

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
print "Phase Counts",
print endPhase

# If your L2 has a single bank, this is all the L2 hits. Otherwise it's the
# hits of the first L2 bank
'''
l2_0_hits = dset[-1]['l2'][0]['hGETS'] + dset[-1]['l2'][0]['hGETX']
print "L2 Hits in the first bank", 
print l2_0_hits
'''

# Hits into all L2s
l2_hits = np.sum(dset[-1]['l2']['hGETS'] + dset[-1]['l2']['hGETX'])
print "L2 Hits in all banks", 
print l2_hits

l2_misses = np.sum(dset[-1]['l2']['mGETS'] + dset[-1]['l2']['mGETXIM'] + dset[-1]['l2']['mGETXSM'])
print "L2 Misses in all banks", 
print l2_misses

l2_latency = np.sum(dset[-1]['l2']['latGETnl'])
print "L2 Latency in all banks", 
print l2_latency

l1d_latency = np.sum(dset[-1]['l1d'][0]['latGETnl'])
print "L1D Latency", 
print l1d_latency
# Total number of instructions executed, counted by adding per-core counts
# (you could also look at procInstrs)
totalInstrs = np.sum(dset[-1]['simpleCore']['instrs'])
print "Total Instructon Counts: ",
print totalInstrs

totalCycle = np.sum(dset[-1]['simpleCore']['cycles'])
print "Total Cycle Counts: ",
print totalCycle

# You can also focus on one sample, or index over multiple steps, e.g.,
lastSample = dset[-1]
allHitsS = lastSample['l2']['hGETS']
firstL2HitsS = allHitsS[0]
#print firstL2HitsS

'''
# There is a certain slack in the positions of numeric and non-numeric indices,
# so the following are equivalent:
print dset[-1]['l2'][0]['hGETS'] 
#print dset[-1][0]['l2']['hGETS'] # can't do
print dset[-1]['l2']['hGETS'][0]
print dset['l2']['hGETS'][-1,0]
print dset['l2'][-1,0]['hGETS']
print dset['l2']['hGETS'][-1,0]
'''

# However, you can't do things like dset[-1][0]['l2']['hGETS'], because the [0]
# indexes a specific element in array 'l2'. The rule of thumb seems to be that
# numeric indices can "flow up", i.e., you can index them later than you should.
# This introduces no ambiguities.

# Slicing works as in numpy, e.g.,

'''
print(dset['l2']['hGETS']) # a 2D array with samples*per-cache data
print(dset['l2']['hGETS'][-1]) # a 1D array with per-cache numbers, for the last sample
print(dset['l2']['hGETS'][:,0]) # 1D array with all samples, for the first L2 cache
'''

# OK, now go bananas!

