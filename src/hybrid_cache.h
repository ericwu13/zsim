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

#ifndef HYBRID_CACHE_H_
#define HYBRID_CACHE_H_

#include "cache.h"

class Network;

/* General coherent modular cache. The replacement policy and cache array are
 * pretty much mix and match. The coherence controller interfaces are general
 * too, but to avoid virtual function call overheads we work with MESI
 * controllers, since for now we only have MESI controllers
 */
class HybridCache: public Cache {
    public:
        HybridCache(uint32_t _numLines, CC* _cc, CacheArray* _array, ReplPolicy* _rp, uint32_t _accLat, uint32_t _accSlowLat, uint32_t _accWrLat, uint32_t _invLat, const g_string& _name);
        uint64_t access(MemReq& req);
};

#endif  // HYBRID_CACHE_H_
