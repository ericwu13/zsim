/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MEM_CTRLS_H_
#define MEM_CTRLS_H_

#include "g_std/g_string.h"
#include "memory_hierarchy.h"
#include "pad.h"
#include "stats.h"
#include "zsim.h"

typedef struct mem_tran_s {
    uint64_t cycle;
    uint64_t addr;
} mem_trans_s;

typedef struct mem_rd_tran_s {
    uint64_t cycle;
    uint64_t addr;
} mem_rd_trans_s;

#define LOG_LENGTH 10000
#define LOG_LENGTH_1 11000
#define MEM_SIZE 262144
#define PAGE_SIZE 16384 // 16Klines ==> 1MB for 64Byte/line

/* Simple memory (or memory bank), has a fixed latency */
class SimpleMemory : public MemObject {
    private:
        g_string name;
        uint32_t latency;

    public:
        uint64_t access(MemReq& req);

        const char* getName() {return name.c_str();}

        SimpleMemory(uint32_t _latency, g_string& _name) : name(_name), latency(_latency) {}
};


/* Implements a memory controller with limited bandwidth, throttling latency
 * using an M/D/1 queueing model.
 */
class MD1Memory : public MemObject {
    private:
        uint64_t lastPhase;
        double maxRequestsPerCycle;
        double smoothedPhaseAccesses;
        uint32_t zeroLoadLatency;
        uint32_t curLatency;
        uint32_t zeroLoadWrLatency;
        uint32_t curWrLatency;

        PAD();

        Counter profReads;
        Counter profWrites;
        Counter profTotalRdLat;
        Counter profTotalWrLat;
        Counter profLoad;
        Counter profUpdates;
        Counter profClampedLoads;
        Counter profBuffered;
        uint32_t curPhaseAccesses;

        g_string name; //barely used
        lock_t updateLock;
        lock_t PUTXLock;

        FILE *logFile;
        FILE *rdLogFile;
        mem_trans_s memoryWriteLog[LOG_LENGTH_1];
        mem_rd_trans_s memoryReadLog[LOG_LENGTH_1];

        uint32_t index;
        uint32_t rdIndex;
        uint32_t numMemCtrls;
        uint64_t *pageTable;
        uint64_t *cycleTable;
        uint32_t numPages;
        uint32_t filledTableIdx;
        Address Buffer[65536];
        uint64_t cycles[65536];
        uint32_t bufferSize;
        uint32_t detailedMemory;
        uint32_t bufferHit;
        uint32_t bufferMiss;
        uint32_t filled; 
        uint64_t writtenBits[MEM_SIZE];
        uint64_t flushCounter;
        uint64_t previousFlush;
        uint64_t flushPeriod;
        uint32_t removedIdx;

        PAD();

    public:
        MD1Memory(uint32_t lineSize, uint32_t megacyclesPerSecond, uint32_t megabytesPerSecond, uint32_t _zeroLoadLatency, uint32_t _zeroLoadWrLatency, uint32_t _buffer, uint32_t _bufferHit, uint32_t _bufferMiss, uint32_t _detailedMemory, uint64_t _flushPeriod, g_string& _name);

        void initStats(AggregateStat* parentStat) {
            AggregateStat* memStats = new AggregateStat();
            memStats->init(name.c_str(), "Memory controller stats");
            profReads.init("rd", "Read requests"); memStats->append(&profReads);
            profWrites.init("wr", "Write requests"); memStats->append(&profWrites);
            profTotalRdLat.init("rdlat", "Total latency experienced by read requests"); memStats->append(&profTotalRdLat);
            profTotalWrLat.init("wrlat", "Total latency experienced by write requests"); memStats->append(&profTotalWrLat);
            profLoad.init("load", "Sum of load factors (0-100) per update"); memStats->append(&profLoad);
            profUpdates.init("ups", "Number of latency updates"); memStats->append(&profUpdates);
            profClampedLoads.init("clampedLoads", "Number of updates where the load was clamped to 95%"); memStats->append(&profClampedLoads);
            profBuffered.init("bufWrites", "Writes to the SRAM buffer"); memStats->append(&profBuffered);
            parentStat->append(memStats);
        }

        //uint32_t access(Address lineAddr, AccessType type, uint32_t childId, MESIState* state /*both input and output*/, MESIState initialState, lock_t* childLock);
        uint64_t access(MemReq& req);

        const char* getName() {return name.c_str();}

        ~MD1Memory() {
            info("Destroyer");
            fwrite(memoryWriteLog, sizeof(mem_trans_s), index, logFile);
            fclose(logFile);
        }

    private:
        void updateLatency();
};

#endif  // MEM_CTRLS_H_
