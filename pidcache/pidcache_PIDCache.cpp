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

#include <otawa/proc/BBProcessor.h>
#include <otawa/hard/CacheConfiguration.h>
#include <otawa/util/LoopInfoBuilder.h>
#include <otawa/util/FlowFactLoader.h>
#include <otawa/hard/Memory.h>
#include <otawa/dfa/ai.h>

#include "PIDCache.h"
#include "PIDAnalysis.h"

//#define WITH_GEN(t)

#define QDCACHE_DEBUG(t)	//t
// #define QDCACHE_CHECK

#	ifdef QDCACHE_CHECK
#		define QDCACHE_DO_CHECK(s) \
			{ \
				for(t prev = 0, cur = s; cur; prev = cur, cur = cur->next) \
					if(prev) \
						ASSERT(!(prev->gen == cur->gen && pman->poly().equals(prev->ref, cur->ref))); \
			}
#	else
#		define QDCACHE_DO_CHECK(s)
#	endif

namespace otawa { namespace pidcache {

Identifier<stat_t> STAT("otawa::pidcache::STAT");


/**
 * Non-optimized version!
 */
class PIDManager {
public:
	typedef Poly::t ref_t;

private:

	class Node {
	public:
		inline Node(void): next(0), ref(0), must(NO_AGE), gen(0) { }
		inline Node(ref_t _ref): next(0), ref(_ref), must(NO_AGE), gen(0) { }
		inline Node(Node *node): next(0), ref(node->ref), must(node->must), gen(node->gen)
			{ 	pers = node->pers; }
		inline void *operator new(size_t s, StackAllocator& alloc) { return alloc.allocate<Node>(); }

		Node *next;
		ref_t ref;
		int gen;
		Must::t must;
		Persistence::t pers;
	};

	inline Node *make(Node *n) { return new(alloc) Node(n); }

	Node *make(ref_t ref, BasicBlock *bb) {
		Node *n = new(alloc) Node(ref);
		n->must = 0;
		pers.init(n->pers, bb);
		return n;
	}

public:
	typedef Node *t;

	PIDManager(int _set, Poly& _pman, RefManager& _rman)
		:	cache(&_rman.cache()),
		 	set(_set),
		 	A(_rman.cache().wayCount()),
		 	_bot(&bot_node),
		 	_top(0),
		 	poly(_pman),
		 	rman(_rman),
		 	must(A),
		 	pers(A) { 
		}

	inline t bot(void) const { return _bot; }
	inline t init(void) const { return _top; }

	bool equals(t s1, t s2) {
		//cerr << "EQUALS(\n"; dump(cerr, s1); dump(cerr, s2); cerr << ") = ";
		if(s1 != _bot && s2 != _bot)
			while(s1 != s2) {
				if(!s1
				|| !s2
				|| !poly.equals(s1->ref, s2->ref)
				|| s1->gen != s2->gen
				|| !must.equals(s1->must, s2->must)
				|| !pers.equals(s1->pers, s2->pers)) {
					//cerr << "false\n";
					return false;
				}
				s1 = s1->next;
				s2 = s2->next;
			}
		//cerr << (s1 == s2) << io::endl;
		return s1 == s2;
	}

	void dump(io::Output& out, t s) {
		if(s == _bot) {
			out << "{ }_bot\n";
			return;
		}
		out << "{ ";
		bool fst = true;
		for(Node *n = s; n; n = n->next) {
			if(fst)
				fst = false;
			else
				out << ", ";
			poly.dump(out, n->ref);
			if(n->gen)
				out << "[" << n->gen << "]";
			out << ":";
			must.print(out, n->must);
			out << ":";
			pers.print(out, n->pers);
		}
		out << " }\n";
	}

	t join(t s1, t s2) {
		Node *r;

		// simple case of bot
		if(s1 == _bot)
			r = s2;
		else if(s2 == _bot)
			r = s1;

		// prepare the join
		else {
			Node **q = &r;
			Node *p1 = s1, *p2 = s2;
			r = 0;

			// merge the states
			while(p1 && p2) {
				Node *nn;
				int c = compare(p1->ref, p1->gen, p2->ref, p2->gen);

				// build the new new node
				if(c == 0) {
					nn = make(p1);
					must.join(nn->must, p1->must, p2->must);
					pers.join(nn->pers, p1->pers, p2->pers);
					p1 = p1->next;
					p2 = p2->next;
				}
				else if(c < 0) {
					nn = make(p1);
					must.undef(nn->must);
					nn->pers = p1->pers;
					p1 = p1->next;
				}
				else {
					nn = make(p2);
					must.undef(nn->must);
					nn->pers = p2->pers;
					p2 = p2->next;
				}

				// link the new node
				*q = nn;
				q = &(nn->next);
			}

			// copy the remaining list
			Node *p = p1 ? p1 : p2;
			while(p) {
				Node *nn = make(p);
				must.undef(nn->must);
				nn->pers = p->pers;
				*q = nn;
				q = &(nn->next);
				p = p->next;
			}
		}

		// return result
		//cerr << "JOIN(\n"; dump(cerr, s1); dump(cerr, s2);
		//cerr << ") = "; dump(cerr, r);
		return r;
	}

	t update(BasicBlock *bb, const PolyAccess& a, t s) {

		// are we concerned by this access?
		//QDCACHE_DEBUG(cerr << "\ttesting "; a.print(cerr, poly); cerr << io::endl);
		if(!a.cached() || a.ref() == poly.bot || !rman.concerns(a.ref(), set))
			return s;
		QDCACHE_DEBUG(cerr << "\t"; a.print(cerr, poly); cerr << io::endl);

		// convert bot to top (for standard processing)
		if(s == _bot)
			s = _top;

		// find the age of the reference if any
		ref_t ref = normalize(a.ref());
		age_t age = -1;
		if(a.ref() == poly.top)
			age = A;
		else {
			for(Node *n = s; n && age < A; n = n->next) {

				// find worst age
				age_t wage = n->must;
				for(int i = 0; i < n->pers.length(); i++)
					wage = max(wage, n->pers[i]);

				// if needed, test for meet of references
				if(wage < A									// out of the cache
				&& (	(poly.equals(n->ref, ref) && n->gen == 0)		// equal but older generation
					||	(   !poly.equals(n->ref, ref)
						 && rman.mayMeet(n->ref, n->gen, ref, 0))))		// not equal but meet
					age = max(age, wage);
			}
			if(age == -1)
				age = A;
		}

		// rebuild the list
		Node *r = 0, **q = &r;
		bool found = ref == poly.top;
		while(s) {
			Node *n;
			int c = compare(ref, 0, s->ref, s->gen);

			// node is current access (age to 0)
			if(c == 0) {
				found = true;
				n = make(s);
				must.touch(n->must);
				pers.touch(n->pers);
				s = s->next;
			}

			else {

				// node is after current node and not processed, create the new node
				if(!found && c <= 0) {
					found = true;
					n = make(ref, bb);
					*q = n;
					q = &(n->next);
				}

				// update the current node
				n = make(s);
				must.update(n->must, s->must, age);
				pers.update(n->pers, s->pers, age);
				s = s->next;
			}

			// next node
			*q = n;
			q = &n->next;
		}

		// ensure the node has been created
		if(!found)
			*q = make(ref, bb);
		QDCACHE_DEBUG(cerr << "age = " << age << ", s = "; dump(cerr, r));
		return r;
	}

	/**
	 * Called to transform a state when entering a loop.
	 * Basically, add a new persistence level.
	 * @param s		State to traform.
	 * @return		Transformed state.
	 */
	t enter(t s) {
		if(s == _bot)
			return s;
		Node *r = 0, **q = &r;
		while(s) {
			Node *n = make(s);
			n->must = s->must;
			pers.enter(n->pers, s->pers);
			*q = n;
			q = &n->next;
			s = s->next;
		}
		return r;
	}

	/**
	 * Called to transform a state when leaving a loop.
	 * Remove a persistence level and references depending on the current
	 * (replaced if possible by constant values).
	 * @param s			State to traform.
	 * @param header	Header of the left loop.
	 * @return			Transformed state.
	 */
	t leave(t s, BasicBlock *header) {
		if(s == _bot)
			return s;
		Node *r = 0, **q = &r;
		while(s) {

			// current loop reference
			if(s->ref->h == header) {
				// known last value
				address_t base, top;
				ot::size off;
				if(poly.toAddress(s->ref, base, top, off) && poly.isTopPrecise(s->ref)) {
					Node *n = make(poly.make(top - (s->gen + 1) * off), 0);
					n->must = s->must;
					pers.leave(n->pers, s->pers);
					*q = n;
					q = &n->next;
				}
			}

			// outer loop reference
			else {
				//Node *n = new(alloc) Node(s);
				Node *n = make(s);
				n->must = s->must;
				pers.leave(n->pers, s->pers);
				*q = n;
				q = &n->next;
			}

			// next node
			s = s->next;
		}
		return r;
	}

	/**
	 * Called to transform a state when passing by a back-edge.
	 * Basically, increase generation of the references depending on the loop.
	 * @param s		State to traform.
	 * @return		Transformed state.
	 */
	t back(t s) {
		Node *r = 0, **q = &r;
		while(s) {
			if(s->gen < A) {

				// build the node
				Node *n = make(s);
				n->must = s->must;
				n->pers = s->pers;

				// array reference: increase generation
#				ifdef WITH_GEN
					if(s->ref->h)
						n->gen = s->gen + 1;
#				endif

				// link the new node
				*q = n;
				q = &n->next;
			}
			s = s->next;
		}
		return r;
	}

	/**
	 * Count the number of misses for the current line.
	 * @param access	Concerned access.
	 * @param s			State before access.
	 * @return			Count of misses.
	 * @param count		Used to return the count.
	 * @param bb		Count relative to the given header or null (absolute value).
	 */
	miss_count_t countMisses(PolyAccess& access, t s) {
#ifdef DEBUG_COUNT_MISSES
		cerr << "set = " << set << "\t";
		access.print(cerr, poly);
		cerr << " ... mcount = ";
#endif
		if(access.ref() == poly.top) {
			(*STAT(access)).nc++;
#ifdef DEBUG_STATS			
			cerr << "DEBUG: reference to T\n";
#endif			
			return 0;  // useless
		}
		else if(!rman.concerns(access.ref(), set)) {
			return 0;
		}

		ref_t ref = normalize(access.ref());
		genstruct::Vector<Node *> to_scan;

		// scan the ACS for interesting information
		bool persistent = false;
		for(Node *n = s; n; n = n->next) {
			if(poly.equals(n->ref, ref)) {
				if(must.isAlive(n->must)) {
#ifdef DEBUG_COUNT_MISSES
					cerr << "in MUST\n";
#endif
					(*STAT(access)).ah++;
#ifdef DEBUG_STATS
					cerr << "DEBUG: AH at " << access.inst()->address() << " to "; poly.dump(cerr, access.ref()); cerr << io::endl;
#endif					
					return 0; // always hit
				}
				if(pers.isAlive(n->pers)) {
					persistent = true;
					(*STAT(access)).pe++;
#ifdef DEBUG_STATS					
					cerr << "DEBUG: PE at " << access.inst()->address() << " to "; poly.dump(cerr, access.ref()); cerr << io::endl;
#endif					
					break;
				}
			}
			else if((must.isAlive(n->must) || pers.isAlive(n->pers))
				&& rman.mayMeet(ref, 0, n->ref, n->gen)
				&& rman.isCompatible(ref, n->ref))
				to_scan.add(n);
		}

		// build the initial list of misses
		// TODO		Take into account generations!
		int mcount = 0;
		elm::t::uint32 last_tag = -1;
		RefManager::RefIter iter(ref);
		if(iter.failed()) // if we could not bound the maxiter of the loop
		{
			//cerr << "WARNING: loop bounds not found, cannot count misses\n";
			(*STAT(access)).nc++;
			return UNBOUNDED;
		}
		bool is_mm = false;
		for(; iter; iter++)
			if(cache->set(*iter) == set && (!persistent || cache->tag(*iter) != last_tag)) {
				last_tag = cache->tag(*iter);

				// examine if some other ref is already loaded the block
				bool found = false;
				for(int i = 0; i < to_scan.length(); i++)
					if(cache->tag(iter.apply(to_scan[i]->ref)) == last_tag) {
						found = true;
						break;
					}

				// not found
				if(!found)
					mcount++;
				else
					is_mm = true;

			}
		if(!persistent) {
			if(is_mm) {
				(*STAT(access)).mm++;
#ifdef DEBUG_STATS				
				cerr << "DEBUG: MM at " << access.inst()->address() << " to "; poly.dump(cerr, access.ref()); cerr << io::endl;
#endif
			}
			else {
				(*STAT(access)).am++;
#ifdef DEBUG_STATS				
				cerr << "DEBUG: AM at " << access.inst()->address() << " to "; poly.dump(cerr, access.ref()); cerr << io::endl;
#endif
			}
		}

		//cerr << mcount << io::endl;
		return mcount;
	}

private:
	typedef Poly::address_t address_t;
	typedef Poly::coef_t coef_t;

	/**
	 * Normalize a reference to store in the ACS.
	 * @param r		Reference to normalize.
	 * @return		Normalized reference.
	 */
	ref_t normalize(ref_t r) {
		if(r->h || r == poly.top || r == poly.bot)
			return r;
		else
			return poly.make(cache->round(r->c));
	}

	/**
	 * Test if two references are the same considering
	 * the cache block access.
	 */
	bool same(ref_t r1, int g1, ref_t r2, int g2) {

		// simple case of constant addresses
		if(!r1->h && !r2->h)
			return cache->tag(r1->c) == cache->tag(r2->c);

		// else just consider strict equality
		// TODO		May be improved by block equivalent checking (really useful?)
		else
			return g1 == g2 && poly.equals(r1, r2);
	}

	/**
	 * Compare two references according to the order chosen
	 * for the cache state list. The order is induced by
	 * the coefficient tested in sequence and, if equality,
	 * the generation is also tested.
	 * @param r1	First reference.
	 * @param gen1	Generation of first reference.
	 * @param r2	Second reference.
	 * @param gen2	Generation of second reference.
	 */
	int compare(ref_t r1, int gen1, ref_t r2, int gen2) {

		// fast simple equality
		if(r1 == r2) {
			//QDCACHE_DEBUG(cerr << (gen1 - gen2) << " (gen)\n");
			return gen1 - gen2;
		}

		// lookup the coefs
		while(r1->h && r2->h && r1->c == r2->c) {
			r1++;
			r2++;
		}

		// end process
		int r;
		if(r1->c < r2->c)
			r = -1;
		else if(r1->c > r2->c)
			r = +1;
		else
			r = gen1 - gen2;

		QDCACHE_DEBUG(cerr << "compare("; poly.dump(cerr, r1); cerr << "[" << gen1 << "], "; poly.dump(cerr, r2); cerr << "[" << gen1 << "]) = " << r << io::endl);
		return r;
	}

	int A;
	const hard::Cache *cache;
	unsigned int set;
	StackAllocator alloc;
	t _bot, _top;
	Node bot_node;
	Poly& poly;
	RefManager& rman;
	Must must;
	Persistence pers;
};

class PIDCacheAnalysis: public CFGProcessor {
public:
	static p::declare reg;
	PIDCacheAnalysis(p::declare& r = reg): CFGProcessor(r) { }

protected:

	virtual void processCFG(WorkSpace *ws, CFG *cfg) {

		// get cache configuration
		const hard::CacheConfiguration *conf = hard::CACHE_CONFIGURATION(ws);
		ASSERT(conf);
		const hard::Cache *cache = conf->dataCache();
		if(!cache)
			throw ProcessorException(*this, "no data cache available");
		if(cache == conf->instCache())
			throw ProcessorException(*this, "unified cache unsupported");
		if(cache->replacementPolicy() != hard::Cache::LRU)
			throw ProcessorException(*this, "only LRU replacement policy supported");

		// process each set in turn
		for(int i = 0; i < cache->setCount(); i++)
			process(ws, cfg, i);

		// put the RELATIVE_TO property
		PolyManager *pman = POLY_MANAGER(ws);
		ASSERT(pman);
		QDCACHE_DEBUG(cerr << "\n\n");
		for(CFG::BBIterator bb(cfg); bb; bb++) {
			QDCACHE_DEBUG(cerr << *bb << io::endl);
			Bag<PolyAccess>& accesses = *ACCESSES(bb);
			for(int i = 0; i < accesses.count(); i++) {

				// determine the relativity (and the count if any)
				if(accesses[i].ref() != pman->poly().top)
					RELATIVE_TO(accesses[i]) = pman->relativeTo(bb, accesses[i].ref());
				else { // top
					BasicBlock *header;
					if(LOOP_HEADER(bb))
						header = bb;
					else
						header = otawa::ENCLOSING_LOOP_HEADER(bb);
					if(!header)
						MISS_COUNT(accesses[i]) = 1;
					else {
						MISS_COUNT(accesses[i]) = MAX_ITERATION(header);
						RELATIVE_TO(accesses[i]) = header;
					}
				}

				// debug
				QDCACHE_DEBUG(cerr << "\t"; accesses[i].print(cerr, pman->poly()); cerr << " -> "
					<< *MISS_COUNT(accesses[i]) << " misses";
					if(RELATIVE_TO(accesses[i])) cerr << " / " << *RELATIVE_TO(accesses[i]);
					int cnt = pman->poly().count(accesses[i].ref());
					if(cnt > 1)
						cerr << " on " << cnt;
					cerr << io::endl;
				);
			}
		}

		// collect statistics
		double am = 0, ah = 0, pe = 0, nc = 0, mm = 0;
		int total = 0;
		double a_am = 0, a_ah = 0, a_pe = 0, a_nc = 0, a_mm = 0;
		int a_total = 0;
		for(CFG::BBIterator bb(cfg); bb; bb++) {
			Bag<PolyAccess>& accesses = *ACCESSES(bb);
			for(int i = 0; i < accesses.count(); i++) {
				total++;
				const stat_t& s = STAT(accesses[i]);
				double t = s.total();
				ah += s.ah / t;
				am += s.am / t;
				pe += s.pe / t;
				nc += s.nc / t;
				mm += s.mm / t;
				if(t > 1) {
					a_total++;
					a_ah += s.ah / t;
					a_am += s.am / t;
					a_pe += s.pe / t;
					a_nc += s.nc / t;
					a_mm += s.mm / t;
				}
			}
		}

		if(logFor(LOG_FUN)){
			cout
				<< "\t[stats] all references\n"
				<< "\t[stats] ah   = \t" << ((int)(ah*1000))/1000.f << " \t(" << (ah*100)/((float)total) << "%)\n"
				<< "\t[stats] am   = \t" << ((int)(am*1000))/1000.f << " \t(" << (am*100)/((float)total) << "%)\n"
				<< "\t[stats] pe   = \t" << ((int)(pe*1000))/1000.f << " \t(" << (pe*100)/((float)total) << "%)\n"
				<< "\t[stats] mm   = \t" << ((int)(mm*1000))/1000.f << " \t(" << (mm*100)/((float)total) << "%)\n"
				<< "\t[stats] nc   = \t" << ((int)(nc*1000))/1000.f << " \t(" << (nc*100)/((float)total) << "%)\n"
				<< "\t[stats] total= \t" << total << "\n";
			if(a_total > 0)
				cout
					<< "\t[stats] array references\n"
					<< "\t[stats] ah   = \t" << ((int)(a_ah*1000))/1000.f << " \t(" << (a_ah*100)/((float)a_total) << "%)\n"
					<< "\t[stats] am   = \t" << ((int)(a_am*1000))/1000.f << " \t(" << (a_am*100)/((float)a_total) << "%)\n"
					<< "\t[stats] pe   = \t" << ((int)(a_pe*1000))/1000.f << " \t(" << (a_pe*100)/((float)a_total) << "%)\n"
					<< "\t[stats] mm   = \t" << ((int)(a_mm*1000))/1000.f << " \t(" << (a_mm*100)/((float)a_total) << "%)\n"
					<< "\t[stats] nc   = \t" << ((int)(a_nc*1000))/1000.f << " \t(" << (a_nc*100)/((float)a_total) << "%)\n"
					<< "\t[stats] total= \t" << a_total << "\n";
		}
	}

private:
	typedef PIDManager::t t;

	void process(WorkSpace *ws, CFG *cfg, int set) {
		if(logFor(LOG_FILE))
			log << "\tset " << set << io::endl;
		QDCACHE_DEBUG(cerr << "\n====== SET " << set << " ======\n");

		// prepare the analysis
		PolyManager *pman = POLY_MANAGER(ws);
		ASSERT(pman);
		PIDManager man(set, pman->poly(), **REF_MANAGER(ws));
		ai::CFGGraph graph(cfg);
		ai::EdgeStore<PIDManager, ai::CFGGraph> store(man, graph);
		ai::WorkListDriver<PIDManager, ai::CFGGraph, ai::EdgeStore<PIDManager, ai::CFGGraph> >
			iter(man, graph, store);
		iter.changeAll();

		// perform the analysis
		while(iter) {
			QDCACHE_DEBUG(cerr << "\n--- " << *iter << " ---\n");

			// apply update
			t s = iter.input();
			QDCACHE_DEBUG(man.dump(cerr, s));
			QDCACHE_DO_CHECK(s);
			Bag<PolyAccess>& accesses = *ACCESSES(*iter);
			for(int i = 0; i < accesses.count(); i++) {
				s = man.update(*iter, accesses[i], s);
					QDCACHE_DO_CHECK(s);
			}

			// refine result according to edges
			for(BasicBlock::OutIterator out(*iter); out; out++) {
				t ss;
				if(BACK_EDGE(out)) {
					QDCACHE_DEBUG(cerr << "back: ");
					ss = man.back(s);
				}
				else if(LOOP_EXIT_EDGE(out)) {
					QDCACHE_DEBUG(cerr << "leave: ");
					BasicBlock* innmost_lh = LOOP_HEADER(*iter) ? *iter : ENCLOSING_LOOP_HEADER(*iter);
					const BasicBlock* outmost_lh = LOOP_EXIT_EDGE(out); // contains header of outmost loop
					bool first = true;
					do
					{
						if(first){
							first = false;
							ss = man.leave(s, innmost_lh);
						} else {
							innmost_lh = ENCLOSING_LOOP_HEADER(innmost_lh);
							ss = man.leave(ss, innmost_lh); 
						}
					} while(innmost_lh != outmost_lh);
				}
				else if(LOOP_HEADER(out->target())) {
					QDCACHE_DEBUG(cerr << "enter: ");
					ss = man.enter(s);
				}
				else
					ss = s;
				QDCACHE_DO_CHECK(ss);
				iter.check(out, ss);
				QDCACHE_DEBUG(cerr << "-> " << *out << io::endl; man.dump(cerr, ss););
			}

			// next
			iter++;
		}

		// use analysis results
		for(CFG::BBIterator bb(cfg); bb; bb++) {
			t s = iter.input(bb);
			Bag<PolyAccess>& accesses = *ACCESSES(bb);
			for(int i = 0; i < accesses.count(); i++) {
				miss_count_t c = man.countMisses(accesses[i], s);
				if(c ==  UNBOUNDED)
					MISS_COUNT(accesses[i]) = UNBOUNDED;
				else
					MISS_COUNT(accesses[i]) += c;
				s = man.update(*iter, accesses[i], s);
			}
		}
	}

};

p::declare PIDCacheAnalysis::reg = p::init("otawa::pidcache::PIDCacheAnalysis", Version(1, 0, 0))
	.base(CFGProcessor::reg)
	.maker<PIDCacheAnalysis>()
	.require(ACCESSES_FEATURE)
	.require(hard::CACHE_CONFIGURATION_FEATURE)
	.require(LOOP_INFO_FEATURE)
	.require(REF_MANAGER_FEATURE)
	.provide(ANALYSIS_FEATURE);

p::feature ANALYSIS_FEATURE("otawa::pidcache::ANALYSIS_FEATURE", new Maker<PIDCacheAnalysis>());
Identifier<miss_count_t> MISS_COUNT("otawa::pidcache::MISS_COUNT", 0);
Identifier<BasicBlock *> RELATIVE_TO("otawa::pidcache::RELATIVE_TO", 0);

} }	// otawa::pidcache

