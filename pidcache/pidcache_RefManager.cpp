/*
 *	pidcache::RefManager class
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

#include <otawa/hard/CacheConfiguration.h>
#include <otawa/util/FlowFactLoader.h>
#include <otawa/util/Dominance.h>
#include "PIDCache.h"

namespace otawa { namespace pidcache {

/**
 * @class RefManager
 * This class provides facilities to handle the PI expressions.
 * It is usually provided by the @ref REF_MANAGER_FEATURE.
 */

/**
 * @fn bool RefManager::concerns(ref_t r, unsigned int set);
 * Test if a reference is concerned by the given set.
 * @param r		Reference to test.
 * @param set	Set to test.
 * @return		True if r reference a block in set, false else.
 */

/**
 * @fn bool RefManager::mayMeet(ref_t r1, int gen1, ref_t r2, int gen2);
 * Test if both references may reference the same cache block.
 * @param r1	First reference.
 * @param gen1	Generation for reference 1.
 * @param r2	Second reference.
 * @param gen2	Generation for reference 2.
 * @return		True if they addresses may meet, false else.
 */

/**
 * Test if tr is compatible with reference rr, that is,
 * tr headers are included in rr headers
 * (order in not meaningful because headers are sorted by dominance).
 * @param rr	Reference reference.
 * @param tr	Tested reference.
 * @return		True if compatible, false else.
 */
bool RefManager::isCompatible(ref_t rr, ref_t tr) {
	while(tr->h && rr->h) {
		if(tr->h == rr->h)
			tr++;
		rr++;
	}
	return !tr->h;
}

/**
 */
RefManager::~RefManager(void) { }

/**
 * @fn bool RefManager::sameSets(ref_t r1, ref_t r2);
 * Test if two references always access the same cache set.
 * @param r1	First reference.
 * @param r2	Second reference.
 * @return		True if both access the same sets, false.
 */

/**
 * @fn bool RefManager::sameBlocks(ref_t r1, ref_t r2);
 * Test if two references always access the same cache blocks.
 * @param r1	First reference.
 * @param r2	Second reference.
 * @return		True if both access the same blocks, false.
 */



/**
 * @param RefManager::RefIter
 * Iterator on the ordered values of a reference (in the same order
 * as the program execution).
 *
 * @warning Before using this iterator, check the failed() method
 * that may state that some loop bounds are missing.
 */

/**
 */
RefManager::RefIter::RefIter(ref_t r): e(false), _failed(false), u(0) {
	while(r->h) {
		steps[u].i = 0;
		steps[u].m = MAX_ITERATION(r->h);
		// ASSERT(steps[u].m != 0);
		if(steps[u].m == -1) // if unbound
			_failed = true;
		steps[u].c = r->c;
		steps[u].h = r->h;
		u++;
		r++;
	}
	addr = r->c;
}

/**
 */
void RefManager::RefIter::next(void) {
	for(int i = 0; i < u; i++) {
		steps[i].i++;
		addr += steps[i].c;
		if(steps[i].i < steps[i].m)
			return;
		steps[i].i = 0;
		addr -= steps[i].m * steps[i].c;
	}
	e = true;
}

/**
 * Build the the address of reference r matching
 * the iteration configuration of the iterator.
 * @param r		Reference to build address for.
 */
RefManager::address_t RefManager::RefIter::apply(ref_t r) {
	address_t a = 0;
	for(int i = 0; i < u; i++)
		if(r->h == steps[i].h) {
			a += steps[i].i * r->c;
			r++;
		}
	return a + r->c;
}


/**
 * @class CoRefIter
 * Iterator on the addresses of 2 references along the different
 * iterations.
 *
 * @warning	Check the fail() method before using this iterator.
 * A initialization failure may be happend if some loop bounds are
 * missing, or if both references does not concern the same loop
 * nest.
 */

/**
 */
RefManager::CoRefIter::CoRefIter(ref_t r1, ref_t r2): e(false), _failed(false), u(0) {

		// copy common
		while(r1->h && r2->h) {
			if(r1->h == r2->h) {
				steps[u].c1 = r1->c;
				steps[u].c2 = r2->c;
				steps[u].h = r1->h;
				r1++;
				r2++;
			}
			else if(Dominance::dominates(r1->h, r2->h)) {
				steps[u].c1 = 0;
				steps[u].c2 = r2->c;
				steps[u].h = r2->h;
				r2++;
			}
			else if(Dominance::dominates(r2->h, r1->h)) {
				steps[u].c1 = r1->c;
				steps[u].c2 = 0;
				steps[u].h = r1->h;
				r1++;
			}
			else {
				_failed = true;
				return;
			}

			// common init
			steps[u].i = 0;
			steps[u].m = MAX_ITERATION(steps[u].h);
			if(steps[u].m == -1) {
				_failed = true;
				return;
			}
			u++;
		}

		// copy remaining
		while(r1->h) {
			steps[u].c1 = r1->c;
			steps[u].c2 = 0;
			steps[u].h = r1->h;
			r1++;
		}
		while(r2->h) {
			steps[u].c1 = 0;
			steps[u].c2 = r2->c;
			steps[u].h = r2->h;
			r2++;
		}

		// set initial address
		ap = pair(address_t(r1->c), address_t(r2->c));
	}

/**
 */
void RefManager::CoRefIter::next(void) {
	for(int i = 0; i < u; i++) {
		steps[i].i++;
		ap = pair(ap.fst + steps[i].c1, ap.snd + steps[i].c2);
		if(steps[i].i < steps[i].m)
			return;
		steps[i].i = 0;
		ap = pair(ap.fst - steps[i].m * steps[i].c1, ap.snd - steps[i].m * steps[i].c2);
	}
	e = true;
}


/**
 */
class ExhaustiveRefManager: public RefManager {
public:
	typedef Poly::coef_t coef_t;

	/**
	 */
	ExhaustiveRefManager(const hard::Cache& cache, const Poly& _poly): RefManager(cache), poly(_poly) { }

	/**
	 */
	virtual bool concerns(ref_t r, unsigned int set) {

		// test basic cases
		if(r == poly.top)
			return true;
		if(r == poly.bot)
			return false;

		// r = simple constant
		if(!r->h)
			return cache().set(Address(r->c)) == set;

		// size bigger than cache
		address_t base, top;
		range(r, base, top);
		if(top - base >= (1 << (cache().blockBits() + cache().setBits())))
			return true;

		// test on sets
		elm::t::uint32 base_set = cache().set(Address(base));
		elm::t::uint32 top_set = cache().set(Address(top));
		// cerr << "DEBUG: "; poly.dump(cerr, r); cerr << " -> [" << Address(base) << ", " << Address(top) << "]: [" << base_set << ", " << top_set << "]\n";
		if(base_set <= top_set) {
			if(set < base_set || top_set < set)
				return false;
		}
		else if(base_set < set && set < top_set)
			return false;

		// prepare enumeration
		typedef struct {
			int i;
			int m;
			coef_t c;
		} step_t;
		static const int max_step = 16;
		step_t steps[max_step];
		int n = 0;
		ref_t p;
		for(p = r; p->h; p++) {
			steps[n].i = 0;
			steps[n].m = MAX_ITERATION(p->h);
			if(steps[n].m < 0)
				return true;
			steps[n].c = p->c;
			n++;
		}

		// enumerate all the values
		while(true) {

			// test the value
			address_t a = p->c;
			for(int i = 0; i < n; i++)
				a += steps[i].c * steps[i].i;
			//cerr << "DEBUG: concern is testing " << cache->set(a) << io::endl;
			if(cache().set(Address(a)) == set)
				return true;

			// increment
			steps[0].i++;
			for(int i = 0; steps[i].i >= steps[i].m; i++) {
				steps[i].i = 0;
				if(i == n - 1)
					return false;
				steps[i + 1].i++;
			}
		}
		return false;
	}

	/**
	 */
	virtual bool mayMeet(ref_t r1, int gen1, ref_t r2, int gen2) {

		// process top and bot
		if(r1 == poly.top || r2 == poly.top)
			return true;
		if(r1 == poly.bot || r2 == poly.bot)
			return false;

		// simple equality
		if(poly.equals(r1, r2)) {
			if(gen1 == gen2)
				return true;
			else
				return r1->c < cache().blockSize();
		}

		// coarse-grain test
		address_t base1, top1;
		address_t base2, top2;
		range(r1, base1, top1);
		range(r2, base2, top2);
		if(top1 <= base2 || top2 <= base1)
			return false;

		// here we need something special to resolve precisely this (SAT, CSP, ILP?)
		// for the meanwhile, we will just enumerate all possibilities

		// prepare the traversal
		typedef struct {
			BasicBlock *h;
			address_t c1, c2;
			int f, m, i;
		} step_t;
		int total = 1;
		static const int step_max = 16;
		step_t steps[step_max];
		int n = 0;
		ref_t p1 = r1, p2 = r2;
		while(p1->h || p2->h) {
			if(p1->h == p2->h) {
				steps[n].h = p1->h;
				steps[n].c1 = p1->c;
				steps[n].c2 = p2->c;
				steps[n].m = MAX_ITERATION(p1->h);
				p1++;
				p2++;
			}
			else if(!p1->h || (p2->h && Dominance::dominates(p1->h, p2->h))) {
				steps[n].h = p2->h;
				steps[n].c1 = 0;
				steps[n].c2 = p2->c;
				steps[n].m = MAX_ITERATION(p2->h);
				p2++;
			}
			else {
				steps[n].h = p1->h;
				steps[n].c1 = p1->c;
				steps[n].c2 = 0;
				steps[n].m = MAX_ITERATION(p1->h);
				p1++;
			}
			total *= steps[n].m;
			steps[n].f = 0;
			// TODO		Find another way to take into account the generation
			/*if(p1 == r1)
				steps[n].f = max(steps[n].f, gen1);
			if(p2 == r2)
				steps[n].f = max(steps[n].f, gen2);*/
			steps[n].i = steps[n].f;
			n++;
			ASSERT(n <= step_max);
		}

		// check threshold
		if(total >= 1000000)
			return true;

		// perform the traversal
		while(true) {

			// perform the test
			address_t a1 = 0, a2 = 0;
			for(int i = 0; i < n; i++) {
				a1 += steps[i].c1 * steps[i].i;
				a2 += steps[i].c2 * steps[i].i;
			}
			//cerr << "DEBUG: " << io::hex(a1) << ", " << io::hex(a2) << io::endl;
			if(cache().tag(a1) == cache().tag(Address(a2)))
				return true;

			// increment
			steps[0].i++;
			for(int i = 0; steps[i].i == steps[i].m; i++) {
				steps[i].i = steps[i].f;
				if(i == n - 1)
					return false;
				steps[i + 1].i++;
			}
		}
		return false;
	}

	virtual bool sameSets(ref_t r1, ref_t r2) {

		// process top and bot
		if(r1 == poly.top || r2 == poly.top)
			return false;
		if(r1 == poly.bot || r2 == poly.bot)
			return false;

		// simple equality
		if(poly.equals(r1, r2))
			return true;

		// test all values
		CoRefIter i(r1, r2);
		if(i.failed())
			return false;
		for(; i; i++)
			if(cache().set((*i).fst) != cache().set((*i).snd))
				return false;
		return true;
	}

	virtual bool sameBlocks(ref_t r1, ref_t r2) {

		// process top and bot
		if(r1 == poly.top || r2 == poly.top)
			return false;
		if(r1 == poly.bot || r2 == poly.bot)
			return false;

		// simple equality
		if(poly.equals(r1, r2))
			return true;

		// test all values
		CoRefIter i(r1, r2);
		if(i.failed())
			return false;
		for(; i; i++)
			if(cache().block((*i).fst) != cache().block((*i).snd))
				return false;
		return true;

	}

	/**
	 * Compute the base and the top address of the given reference.
	 * @param r		Reference to compute top address for.
	 * @param base	Return base address.
	 * @param top	Return top address (exclusive).
	 */
	virtual void range(ref_t r, address_t& base, address_t& top) {
		top = 0;
		while(r->h) {
			int max = otawa::MAX_ITERATION(r->h);
			if(max == -1) {
				// if loop bounds could not be determined
				// set range to [0,+inf[
				base = otawa::sem::uintmin;
				top = otawa::sem::uintmax;
				return;
			}
			top += r->c * (max - 1);
			r++;
		}
		top += r->c;
		base = r->c;
		if(base > top) {
			address_t tmp = base;
			base = top;
			top = tmp;
		}
	}


private:

	const Poly& poly;
};


/**
 */
class L1RefManagerBuilder: public Processor {
public:
	static p::declare reg;
	L1RefManagerBuilder(p::declare& r = reg): Processor(reg) { }
protected:

	virtual void processWorkSpace(WorkSpace *ws) {
		const hard::CacheConfiguration *conf = hard::CACHE_CONFIGURATION(ws);
		if(!conf->dataCache())
			throw ProcessorException(*this, "no data cache available");
		REF_MANAGER(ws) = new ExhaustiveRefManager(*(conf->dataCache()), POLY_MANAGER(ws)->poly());
	}

};

p::declare L1RefManagerBuilder::reg = p::init("otawa::pidcache::L1RefManagerBuilder", Version(1, 0, 0))
	.maker<L1RefManagerBuilder>()
	.provide(REF_MANAGER_FEATURE)
	.require(hard::CACHE_CONFIGURATION_FEATURE)
	.require(POLY_FEATURE);


/**
 * This ensures that a reference manager is available.
 *
 * @p Properties
 * @li @ref REF_MANAGER;
 *
 */
p::feature REF_MANAGER_FEATURE("otawa::pidcache::REF_MANAGER_FEATURE", new Maker<L1RefManagerBuilder>());


/**
 * This property provides a reference manager.
 *
 * @p Hooks
 * @li @ref WorkSpace
 *
 * @p Features
 * @li @ref REF_MANAGER_FEATURE
 */
Identifier<RefManager *> REF_MANAGER("otawa::pidcache::REF_MANAGER", 0);


} }	// otawa::pidcache

