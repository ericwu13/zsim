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

//#include "timing_event.h"
//#include "event_recorder.h"
#include "mem_ctrls.h"
#include "zsim.h"

uint64_t SimpleMemory::access(MemReq& req) {
    switch (req.type) {
        case PUTS:
        case PUTX:
            *req.state = I;
            break;
        case GETS:
            *req.state = req.is(MemReq::NOEXCL)? S : E;
            break;
        case GETX:
            *req.state = M;
            break;

        default: panic("!?");
    }

    uint64_t respCycle = req.cycle + latency;
    assert(respCycle > req.cycle);
/*
    if ((req.type == GETS || req.type == GETX) && eventRecorders[req.srcId]) {
        Address addr = req.lineAddr<<lineBits;
        MemAccReqEvent* memEv = new (eventRecorders[req.srcId]->alloc<MemAccReqEvent>()) MemAccReqEvent(nullptr, false, addr);
        TimingRecord tr = {addr, req.cycle, respCycle, req.type, memEv, memEv};
        eventRecorders[req.srcId]->pushRecord(tr);
    }
*/
    return respCycle;
}




MD1Memory::MD1Memory(uint32_t requestSize, uint32_t megacyclesPerSecond, uint32_t megabytesPerSecond, uint32_t _zeroLoadLatency, uint32_t _zeroLoadWrLatency, uint32_t _buffer, uint32_t _bufferHit, uint32_t _bufferMiss,uint32_t _detailedMemory, uint64_t _flushPeriod, g_string& _name)
    : zeroLoadLatency(_zeroLoadLatency), name(_name)
{
    lastPhase = 0;

    double bytesPerCycle = ((double)megabytesPerSecond)/((double)megacyclesPerSecond);
    maxRequestsPerCycle = bytesPerCycle/requestSize;
    assert(maxRequestsPerCycle > 0.0);

    zeroLoadLatency = _zeroLoadLatency;
    zeroLoadWrLatency = _zeroLoadWrLatency;

    smoothedPhaseAccesses = 0.0;
    curPhaseAccesses = 0;
    curLatency = zeroLoadLatency;
    curWrLatency = zeroLoadWrLatency;

    index = 0;
    rdIndex = 0;
    bufferSize = _buffer;
    bufferHit = _bufferHit;
    bufferMiss = _bufferMiss;
    numPages = 0;
    filledTableIdx = 0;
    detailedMemory = _detailedMemory;
    filled = 0;
    removedIdx = 0;

    for (uint32_t i = 0; i < MEM_SIZE; ++i) {
        writtenBits[i] = 0;
    }

    flushCounter = 0;
    previousFlush = 0;
    flushPeriod = _flushPeriod;
    g_string filename("Memory_writings_" + _name + ".txt");
    g_string filenameRd("Memory_readings_" + _name + ".txt");
    logFile = fopen(filename.c_str(), "wb");
    rdLogFile = fopen(filenameRd.c_str(), "wb");


    futex_init(&updateLock);
    futex_init(&PUTXLock);
}

void MD1Memory::updateLatency() {
    uint32_t phaseCycles = (zinfo->numPhases - lastPhase)*(zinfo->phaseLength);
    if (phaseCycles < 10000) return; //Skip with short phases

    smoothedPhaseAccesses =  (curPhaseAccesses*0.5) + (smoothedPhaseAccesses*0.5);
    double requestsPerCycle = smoothedPhaseAccesses/((double)phaseCycles);
    double load = requestsPerCycle/maxRequestsPerCycle;

    //Clamp load
    if (load > 0.95) {
        //warn("MC: Load exceeds limit, %f, clamping, curPhaseAccesses %d, smoothed %f, phase %ld", load, curPhaseAccesses, smoothedPhaseAccesses, zinfo->numPhases);
        load = 0.95;
        profClampedLoads.inc();
    }

    double latMultiplier = 1.0 + 0.5*load/(1.0 - load); //See Pollancek-Khinchine formula
    curLatency = (uint32_t)(latMultiplier*zeroLoadLatency);
    curWrLatency = (uint32_t)(latMultiplier*zeroLoadWrLatency);

    //info("%s: Load %.2f, latency multiplier %.2f, latency %d", name.c_str(), load, latMultiplier, curLatency);
    uint32_t intLoad = (uint32_t)(load*100.0);
    profLoad.inc(intLoad);
    profUpdates.inc();

    curPhaseAccesses = 0;
    __sync_synchronize();
    lastPhase = zinfo->numPhases;
}

uint64_t MD1Memory::access(MemReq& req) {
    uint64_t correctAddress = req.lineAddr << lineBits;

    if (zinfo->numPhases > lastPhase) {
        futex_lock(&updateLock);
        //Recheck, someone may have updated already
        if (zinfo->numPhases > lastPhase) {
            updateLatency();
        }
        futex_unlock(&updateLock);
    }

    uint64_t replacedIdx = 0;
    int foundIdx = -1;
    Address temp = req.lineAddr;
    uint64_t lat = 0;
    flushCounter = req.cycle;

    if (((flushCounter - previousFlush) >= flushPeriod) && flushPeriod > 0) {
        previousFlush = flushCounter;
        for (uint32_t i = 0; i < MEM_SIZE; ++i) {
            writtenBits[i] = 0;
        }
    }

    uint64_t indexer = (req.lineAddr & 0x00FFFFFF) >> 6;
    uint64_t bitIndex = req.lineAddr & 0x0003F;
    uint64_t mask = 1 << bitIndex;
    uint32_t foundPage = 0;
    uint32_t wroteInPage = 0;

    futex_lock(&PUTXLock);

    switch (req.type) {
        case PUTX:

            if (numPages > 0) {
                uint64_t currentPageTag = req.lineAddr >> 10;
                if (foundPage == 0) {
                    if (filledTableIdx < 0) {
                        pageTable[filledTableIdx] = currentPageTag;
                        cycleTable[filledTableIdx++] = req.cycle;
                        wroteInPage = 1;
                    }
                    else {
                        replacedIdx = 0;
                    }
                }
            }

            if (wroteInPage == 0) {
                if ((writtenBits[indexer] & mask) == 0 && flushPeriod > 0) {
                    writtenBits[indexer] = writtenBits[indexer] ^ mask;
                }
                else {
                    if (bufferSize > 0) {
                        info("Buffer size %d", bufferSize);
                        for (uint32_t i = 0; i < bufferSize; ++i) {
                            if (Buffer[i] == req.lineAddr) {
                                foundIdx = i;
                                break;
                            }
                        }
                        // found it
                        if (foundIdx >= 0) {
                            cycles[foundIdx] = req.cycle;
                            lat = bufferHit;
                            profBuffered.atomicInc();
                        }

                        // not found but buffer is not full
                        else if (filled < bufferSize) {
                            Buffer[filled] = req.lineAddr;
                            cycles[filled++] = req.cycle;
                            lat = bufferHit;
                            profBuffered.atomicInc();
                        }

                        // buffer is full; do an LRU
                        else {
                            for (uint64_t i = 1; i < bufferSize; ++i) {
                                if (cycles[replacedIdx] > cycles[i]) {
                                    replacedIdx = i;
                                }
                            }

                            profBuffered.atomicInc();
                            cycles[replacedIdx] = req.cycle;
                            temp = req.lineAddr;
                            req.lineAddr = Buffer[replacedIdx];
                            Buffer[replacedIdx] = temp;
                            ++replacedIdx;

                            correctAddress = req.lineAddr;
                        }
                    }
                }

                if (bufferSize == 0 || (bufferSize > 0 && lat == 0)) {
                    profWrites.atomicInc();
                    profTotalWrLat.atomicInc(curWrLatency);
                    lat = 0;
                    uint32_t dirtyWords = req.wordIdx;
                    printf("dirty words: %u\n", dirtyWords);
                    printf("dirty line address: %zu\n", req.lineAddr);
                    uint32_t i = 0;
                    if (correctAddress > 0) {
                        while (dirtyWords > 0) {
                            if (dirtyWords % 2 == 0) {
                                memoryWriteLog[index].cycle = req.cycle;
                                memoryWriteLog[index].addr = ((req.lineAddr << 3) | i);
                                ++index;
                            }
                            dirtyWords = dirtyWords >> 1;
                            ++i;
                        }
                    }

                    if (index >= LOG_LENGTH) {
                        fwrite(memoryWriteLog, sizeof(mem_trans_s), index, logFile); 
                        index = 0;
                    }
                }
            }


            //Dirty wback
            //profWrites.atomicInc();
            //profTotalWrLat.atomicInc(curWrLatency);
            __sync_fetch_and_add(&curPhaseAccesses, 1);
            //Note no break
        case PUTS:
            //Not a real access -- memory must treat clean wbacks as if they never happened.
            *req.state = I;
            break;
        case GETS:
            memoryReadLog[rdIndex].addr = req.lineAddr;
            memoryReadLog[rdIndex].cycle = req.cycle;
            ++rdIndex;

            profReads.atomicInc();
            profTotalRdLat.atomicInc(curLatency);
            __sync_fetch_and_add(&curPhaseAccesses, 1);
            *req.state = req.is(MemReq::NOEXCL)? S : E;
            break;
        case GETX:
            memoryReadLog[rdIndex].addr = req.lineAddr;
            memoryReadLog[rdIndex].cycle = req.cycle;
            ++rdIndex;

            profReads.atomicInc();
            profTotalRdLat.atomicInc(curLatency);
            __sync_fetch_and_add(&curPhaseAccesses, 1);
            *req.state = M;
            break;

        default: panic("!?");
    }

    if (rdIndex >= LOG_LENGTH) {
        fwrite(memoryReadLog, sizeof(mem_rd_trans_s), rdIndex, rdLogFile);
        rdIndex = 0;
    }

    futex_unlock(&PUTXLock);


    if (req.type == PUTX) {
        if (lat > 0) return req.cycle + lat;
        else return req.cycle + curWrLatency;
    }
    return req.cycle + ((req.type == PUTS)? 0 /*PUTS is not a real access*/ : curLatency);
}

