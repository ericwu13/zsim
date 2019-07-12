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

#include "hybrid_cache.h"

#include "event_recorder.h"
#include "timing_event.h"
#include "zsim.h"

HybridCache::HybridCache(uint32_t _numLines, CC* _cc, CacheArray* _array, ReplPolicy* _rp, uint32_t _accLat, uint32_t _accSlowLat, uint32_t _accWrLat, uint32_t _accSlowWrLat, uint32_t _invLat, const g_string& _name, bool _dirtyWb)
    : Cache(_numLines, _cc, _array, _rp, _accLat, _accSlowLat, _accWrLat, _accSlowWrLat, _invLat, _name) {
        dirtyWb = _dirtyWb;
    }

uint64_t HybridCache::access(MemReq& req) {
    uint64_t respCycle = req.cycle;
    bool skipAccess = cc->startAccess(req); //may need to skip access due to races (NOTE: may change req.type!)
    if (likely(!skipAccess)) {
        // info("==================");
        // info("Access Line Addr: 0x%lx", req.lineAddr);
        bool updateReplacement = (req.type == GETS) || (req.type == GETX);

        uint32_t mruIdx = rp->getMRU(req.lineAddr);
        int32_t lineId = array->lookup(req.lineAddr, &req, updateReplacement); 

        bool isMRU = (int32_t)mruIdx == lineId || (int32_t)mruIdx == -1;

        if(req.type == GETS || req.type == GETX) {
            if(lineId == -1) {
                respCycle += accLat;
            } else if(isMRU) {
                respCycle += accLat;
                cc->incMRUHit();
            } else {
                respCycle += accSlowLat;
                /*if(cc->isDirty(mruIdx) && dirtyWb) {
                    uint32_t candidate = mruIdx;
                    Address wbLineAddr;
                    array->getAddress(candidate, &wbLineAddr);
                    trace(Cache, "[%s] Evicting 0x%lx", name.c_str(), wbLineAddr);
                    cc->processEviction(req, wbLineAddr, candidate, respCycle);
                }*/
            }
        } else {
            respCycle += accWrLat;
        }

        if (lineId == -1 && cc->shouldAllocate(req)) {
            // info("Cache Miss");
            //Make space for new line
            Address wbLineAddr;
            lineId = array->preinsert(req.lineAddr, &req, &wbLineAddr); //find the lineId to replace
            trace(Cache, "[%s] Evicting 0x%lx", name.c_str(), wbLineAddr);

            //Evictions are not in the critical path in any sane implementation -- we do not include their delays
            //NOTE: We might be "evicting" an invalid line for all we know. Coherence controllers will know what to do
            cc->processEviction(req, wbLineAddr, lineId, respCycle); //1. if needed, send invalidates/downgrades to lower level

            array->postinsert(req.lineAddr, &req, lineId); //do the actual insertion. NOTE: Now we must split insert into a 2-phase thing because cc unlocks us.

            /* uint32_t numSets = numLines / 8;
            uint32_t set = wbLineAddr & (numSets - 1);
            info("Set %d, Evicting 0x%lx", set, wbLineAddr);*/
        }
        // Enforce single-record invariant: Writeback access may have a timing
        // record. If so, read it.
        EventRecorder* evRec = zinfo->eventRecorders[req.srcId];
        TimingRecord wbAcc;
        wbAcc.clear();
        if (unlikely(evRec && evRec->hasRecord())) {
            wbAcc = evRec->popRecord();
        }

        respCycle = cc->processAccess(req, lineId, respCycle);

        // Access may have generated another timing record. If *both* access
        // and wb have records, stitch them together
        if (unlikely(wbAcc.isValid())) {
            if (!evRec->hasRecord()) {
                // Downstream should not care about endEvent for PUTs
                wbAcc.endEvent = nullptr;
                evRec->pushRecord(wbAcc);
            } else {
                // Connect both events
                TimingRecord acc = evRec->popRecord();
                assert(wbAcc.reqCycle >= req.cycle);
                assert(acc.reqCycle >= req.cycle);
                DelayEvent* startEv = new (evRec) DelayEvent(0);
                DelayEvent* dWbEv = new (evRec) DelayEvent(wbAcc.reqCycle - req.cycle);
                DelayEvent* dAccEv = new (evRec) DelayEvent(acc.reqCycle - req.cycle);
                startEv->setMinStartCycle(req.cycle);
                dWbEv->setMinStartCycle(req.cycle);
                dAccEv->setMinStartCycle(req.cycle);
                startEv->addChild(dWbEv, evRec)->addChild(wbAcc.startEvent, evRec);
                startEv->addChild(dAccEv, evRec)->addChild(acc.startEvent, evRec);

                acc.reqCycle = req.cycle;
                acc.startEvent = startEv;
                // endEvent / endCycle stay the same; wbAcc's endEvent not connected
                evRec->pushRecord(acc);
            }
        }
    }

    cc->endAccess(req);

    assert_msg(respCycle >= req.cycle, "[%s] resp < req? 0x%lx type %s childState %s, respCycle %ld reqCycle %ld",
            name.c_str(), req.lineAddr, AccessTypeName(req.type), MESIStateName(*req.state), respCycle, req.cycle);
    return respCycle;
}
