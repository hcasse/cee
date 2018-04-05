/*
 *	PIDCache support
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2014, IRIT UPS.
 *
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef OTAWA_DCACHE_PIDCACHE_H_
#define OTAWA_DCACHE_PIDCACHE_H_

#include <elm/genstruct/Table.h>
#include <otawa/base.h>
#include <elm/type_info.h>
#include <otawa/hard/CacheConfiguration.h>
#include <otawa/util/FlowFactLoader.h>
#include <otawa/util/Bag.h>
#include "PolyAnalysis.h"
#include <otawa/util/Dominance.h>

namespace otawa { namespace pidcache {

using namespace elm;

// PolyAccess class
class PolyAccess: public PropList {
public:
	typedef enum {
		NONE = 0,
		LOAD = 1,
		STORE = 2
	} access_t;
	typedef Poly::t ref_t;

	inline PolyAccess(void): _inst(0), _access(NONE), _ref(0), _cached(false) { }
	inline PolyAccess(Inst *inst, access_t access, ref_t ref, bool cached = true)
		: _inst(inst), _access(access), _ref(ref), _cached(cached) { }
	inline operator bool(void) const { return _access != NONE; }
	inline Inst *inst(void) const { return _inst; }
	inline access_t access(void) const { return _access; }
	inline ref_t ref(void) const { return _ref; }
	inline bool cached(void) const { return _cached; }
	void print(io::Output& out, const Poly& poly) const;

private:
	Inst *_inst;
	access_t _access: 8;
	bool _cached: 1;
	ref_t _ref;
};


// RefManager class
class RefManager {
public:
	typedef Poly::t ref_t;
	typedef t::uint32 address_t;

	inline RefManager(const hard::Cache& cache): _cache(cache) { }
	virtual ~RefManager(void);
	inline const hard::Cache& cache(void) const { return _cache; }
	bool isCompatible(ref_t rr, ref_t tr);

	virtual bool concerns(ref_t r, unsigned int set) = 0;
	virtual bool mayMeet(ref_t r1, int gen1, ref_t r2, int gen2) = 0;
	virtual void range(ref_t r, address_t& base, address_t& top) = 0;
	virtual bool sameSets(ref_t r1, ref_t r2) = 0;
	virtual bool sameBlocks(ref_t r1, ref_t r2) = 0;

	class RefIter: public PreIterator<RefIter, address_t> {
	public:
		RefIter(ref_t r);
		inline bool ended(void) const { return e; }
		inline address_t item(void) const { return addr; }
		inline bool failed(void) const { return _failed; }
		void next(void);
		address_t apply(ref_t r);

	private:
		static const int max = 16;
		struct {
			int i;
			int m;
			Poly::coef_t c;
			BasicBlock *h;
		} steps[max];
		int u;
		bool e;
		bool _failed;
		address_t addr;
	};

	class CoRefIter: public PreIterator<CoRefIter, Pair<address_t, address_t> > {
	public:
		CoRefIter(ref_t r1, ref_t r2);
		Pair<address_t, address_t> item(void) const { return ap; }
		inline bool ended(void) const { return e; }
		inline bool failed(void) const { return _failed; }
		void next(void);

	private:
		static const int max = 16;
		struct {
			int i;
			int m;
			Poly::coef_t c1, c2;
			BasicBlock *h;
		} steps[max];
		int u;
		bool _failed;
		bool e;
		Pair<address_t, address_t> ap;
	};

private:
	const hard::Cache& _cache;
};


// properties and features
extern p::feature ACCESSES_FEATURE;
extern Identifier<Bag<PolyAccess> > ACCESSES;

extern p::feature REF_MANAGER_FEATURE;
extern Identifier<RefManager *> REF_MANAGER;

extern p::feature ANALYSIS_FEATURE;
typedef t::uint64 miss_count_t;
const miss_count_t UNBOUNDED = elm::type_info<t::uint64>::max;
extern Identifier<miss_count_t> MISS_COUNT;
extern Identifier<BasicBlock *> RELATIVE_TO;

typedef struct stat_t {

	inline stat_t(void): am(0), ah(0), mm(0), nc(0), pe(0) { }
	inline int total(void) const { return am + ah + mm + nc + pe; }

	int am; // always miss
	int ah; // always hit
	int mm; // may miss (often miss)
	int nc; // not classified
	int pe; // persistent

} stat_t;

extern Identifier<stat_t> STAT;

} }		// otawa::pidcache

#endif /* OTAWA_DCACHE_PIDCACHE_H_ */
