/*
 *	PolyAnalysis class -- Poly analysis
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

#include <otawa/proc/CFGProcessor.h>
#include <otawa/cfg/features.h>
#include <otawa/util/LoopInfoBuilder.h>
#include <otawa/ipet.h>
#include <otawa/dfa/ai.h>
#include <otawa/util/FlowFactLoader.h>
#include "PolyAnalysis.h"
// #include <elm/log/Log.h>


//#define DCACHE_BB
//#define DCACHE_INST
//#define DCACHE_SEM
//#define DCACHE_MEM
#define DCACHE_MEM_WARN_TOP
//#define DCACHE_STATE

namespace otawa { namespace pidcache {

Identifier<dfa::FastState<Poly>::t> PREV("", 0);

class PolyAnalysis: public CFGProcessor {
public:
	static p::declare reg;
	PolyAnalysis(p::declare& r = reg): CFGProcessor(r) { }

protected:
	typedef Poly::t value_t;
	typedef PolyManager::t state_t;

	class Filter {
	public:
		inline Filter(Edge *edge, PolyManager& poly): e(edge), p(poly) { }
		inline value_t process(value_t v) { return p.poly().filter(e, v); }
	private:
		Edge *e;
		PolyManager& p;
	};

	state_t filter(Edge *edge, state_t s, PolyManager& man) {
		Filter filter(edge, man);
		return man.state().map(s, filter);
	}

	class LoopJoiner {
	public:
		inline LoopJoiner(BasicBlock *header, PolyManager& poly): h(header), p(poly) { }
		value_t process(value_t in, value_t back) {
			value_t r = p.poly().loop_join(h, in, back);
#ifdef DEBUG_POLY_ANALYSIS
			cerr << "loop_join("; p.poly().dump(cerr, in); cerr << ", "; p.poly().dump(cerr, back); cerr << ") = "; p.poly().dump(cerr, r); cerr << io::endl;
#endif
			return r;
		}
	private:
		BasicBlock *h;
		PolyManager& p;
	};

	class Widener {
	public:
		inline Widener(BasicBlock *header, PolyManager& poly): h(header), p(poly) { }
		value_t process(value_t prev, value_t next) {
			value_t r = p.poly().widen(h, prev, next);
#ifdef DEBUG_POLY_ANALYSIS
			cerr << "widen("; p.poly().dump(cerr, prev); cerr << ", "; p.poly().dump(cerr, next); cerr << ") = "; p.poly().dump(cerr, r); cerr << io::endl;
#endif
			return r;
		}
	private:
		BasicBlock *h;
		PolyManager& p;
	};

	class ExWidener {
	public:
		inline ExWidener(BasicBlock *header, PolyManager& poly): h(header), p(poly) { }
		value_t process(value_t prev, value_t next) {
			value_t r = p.poly().exwiden(h, prev, next);
			return r;
		}
	private:
		BasicBlock *h;
		PolyManager& p;
	};

	class ExJoiner {
	public:
		inline ExJoiner(BasicBlock *header, PolyManager& poly): h(header), p(poly) { }
		value_t process(value_t prev, value_t next) {
			value_t r = p.poly().exloop_join(prev, next);
			return r;
		}
	private:
		BasicBlock *h;
		PolyManager& p;
	};

	state_t widen(BasicBlock *header, PolyManager& man, ai::EdgeStore<PolyManager, ai::CFGGraph>& store) {

		// join entries and back states
		state_t in = man.bot(), back = man.bot();
		for(BasicBlock::InIterator e(header); e; e++) {
			state_t s = store.get(*e);
			//cerr << "DEBUG: state of " << *e << " is "; man.dump(cerr, s);
			if(otawa::BACK_EDGE(e))
				back = man.join(back, s);
			else
				in = man.join(in, s);
		}
#ifdef DEBUG_POLY_ANALYSIS
		cerr << "DEBUG: IN JOIN = "; man.dump(cerr, in);
		cerr << "DEBUG: BACK JOIN = "; man.dump(cerr, back);
#endif

		// widen the back state
		state_t prev = PREV(header);
		if(!prev)
			prev = man.bot();
#ifdef DEBUG_POLY_ANALYSIS
		cerr << "DEBUG: PREV = "; man.dump(cerr, prev);
#endif
		ExWidener widener(header, man);
		state_t ws = man.state().combine(prev, back, widener);
#ifdef DEBUG_POLY_ANALYSIS
		cerr << "DEBUG: WIDEN = "; man.dump(cerr, ws);
#endif

		// join the result
		ExJoiner joiner(header, man);
		state_t r = man.state().combine(in, ws, joiner);
		PREV(header) = r;
		return r;

#	if 0
		// join  entries and back states
		LoopJoiner joiner(header, man);
		state_t next = man.state().combine(in, back, joiner);
		cerr << "DEBUG: JOIN* = "; man.dump(cerr, next);

		// widening
		state_t prev = PREV(header);
		if(!prev)
			prev = man.bot();
		cerr << "DEBUG: PREV = "; man.dump(cerr, prev);
		Widener widener(header, man);
		state_t ws = man.state().combine(prev, next, widener);
		cerr << "DEBUG: WIDEN = "; man.dump(cerr, ws);
		PREV(header) = ws;
		return ws;
#	endif
	}

	virtual void processCFG(WorkSpace *ws, CFG *cfg) {
		PolyManager *man = new PolyManager(ws, cfg);

		// data initialization
		ai::CFGGraph graph(cfg);
		ai::EdgeStore<PolyManager, ai::CFGGraph> store(*man, graph);
		ai::WorkListDriver<PolyManager, ai::CFGGraph, ai::EdgeStore<PolyManager, ai::CFGGraph> >
			ana(*man, graph, store);
		PolyManager::Iter iter(*man);
		//log << "INFO: Poly analysis initialized!\n";

		// perform the analysis
		while(ana) {
			state_t s;

			//  normal processing
			if(!otawa::LOOP_HEADER(*ana)) {
#				ifdef DCACHE_STATE
					cerr << "JOIN(\n";
					for(BasicBlock::InIterator in(*ana); in; in++)
						man->dump(cerr, store.get(*in));
					cerr << ") = ";
#				endif
				s = ana.input();
			}

			// widening and filtering for look header
			else
				s = widen(*ana, *man, store);

			// update the state
			s = man->update(iter, *ana, s);

			// set output state and filter exit edges
			for(BasicBlock::OutIterator e(*ana); e; e++) {
				if(!otawa::LOOP_EXIT_EDGE(*e))
					ana.check(*e, s);
				else
					ana.check(*e, filter(*e, s, *man));
			}
			ana++;
		}

		// store the input values
		for(CFG::BBIterator bb(cfg); bb; bb++) {
			state_t s;
			if(!otawa::LOOP_HEADER(bb))
				s = ana.input(bb);
			else
				s = widen(bb, *man, store);
			POLY_STATE(bb) = s;
		}
		POLY_MANAGER(ws) = man;
	}

};

PolyManager::t PolyManager::update(Inst *i, sem::inst si, t s) {
#		ifdef DCACHE_SEM
		//cerr << "DEBUG: s  = "; dump(cerr, s);
		cerr << "SEM:\t\t" << si << io::endl;
#		endif
	switch(si.op) {
	case sem::NOP:
	case sem::BRANCH:
	case sem::TRAP:
	case sem::CONT:
	case sem::IF:		break;
	case sem::LOAD:		s = load(s, si, i); break;
	case sem::STORE:	s = store(s, si, i); break;
	case sem::SET:		s = set(s, si.d(), get(s, si.a())); break;
	case sem::SETI:		s = set(s, si.d(), _poly.make(si.cst())); break;
	case sem::SETP:		ASSERTP(false, "unsupported sem::setp"); break;
	case sem::CMP:
	case sem::CMPU:
	case sem::NEG:
	case sem::NOT:
	case sem::AND:
	case sem::XOR:
	case sem::MULU:
	case sem::MULH:
	case sem::DIV:
	case sem::DIVU:
	case sem::MOD:
	case sem::MODU:
	case sem::SCRATCH:	s = set(s, si.d(), Poly::top); break;
	case sem::ADD:		s = set(s, si.d(), _poly.add(get(s, si.a()), get(s, si.b()))); break;
	case sem::SUB:		s = set(s, si.d(), _poly.sub(get(s, si.a()), get(s, si.b()))); break;
	case sem::SHL:		s = set(s, si.d(), _poly.shl(get(s, si.a()), get(s, si.b()))); break;
	case sem::SHR:		s = set(s, si.d(), _poly.shr(get(s, si.a()), get(s, si.b()))); break;
	case sem::ASR:		s = set(s, si.d(), _poly.asr(get(s, si.a()), get(s, si.b()))); break;
	case sem::MUL:		s = set(s, si.d(), _poly.mul(get(s, si.a()), get(s, si.b()))); break;
	case sem::OR:		s = set(s, si.d(), _poly._or(get(s, si.a()), get(s, si.b()))); break;
	case sem::SPEC:		ASSERTP(false, "unsupported sem::spec"); break;
	default:			ASSERTP(false, "unknown semantic instruction"); break;
	}
#	ifdef DCACHE_SEM
		cerr << "DEBUG:\t\t\t"; _state.print(cerr, s);
		/*
		static Inst *old_i = NULL;
		if(!(i == old_i)) {
			DBG(color::On_IGre() << "INST:\t\t" << i << " (" << i->address() << ")" << color::RCol() << io::endl)			
			old_i = i;
		}
		DBG(color::Red() << "SEM:\t\t" << si << " (" << i->address() << ")" << color::RCol() << io::endl)
		DBG(color::Blu() << "DEBUG: s = "); dump(cout, s); cout << color::RCol();
		*/
#	endif
	return s;
}

PolyManager::t PolyManager::update(Iter& iter, Inst *inst, t s) {
#	if defined(DCACHE_INST)
		cerr << "INST: \t" << inst->address() << "\t" << inst << io::endl;
#	endif
	for(iter.start(inst, s); iter; iter++);
#	if defined(DCACHE_INST)
		cerr << "s = "; dump(cerr, iter.out());
#	endif
	return iter.out();
}

PolyManager::t PolyManager::update(Iter& iter, BasicBlock *bb, t d) {
	t s = d;
#		if defined(DCACHE_BB) || defined(DCACHE_INST) || defined(DCACHE_SEM)
		cerr << "\nBB: " << bb->number() << " [";
		for(BasicBlock::InIterator in(bb); in; in++)
			cerr << " " << in->source()->number();
		cerr << "] @ " << bb->address() << "\n";
		cerr << "input = "; dump(cerr, s);
#		endif
	for(BasicBlock::InstIter inst(bb); inst; inst++) {
		s = update(iter, inst, s);
	}
#		if defined(DCACHE_BB)
		cerr << "output = "; dump(cerr, s);
#		endif
	return s;
}


PolyManager::t PolyManager::load(t s, sem::inst i, Inst *inst) {
	Poly::t v = get(s, i.addr());
	if(v != Poly::bot && v != Poly::top) {
#			ifdef DCACHE_MEM
			cerr << "MEM:\t\tload to "; _poly.dump(cerr, v); cerr << io::endl;
#			endif

		// look in the current state
		Poly::address_t a, b;
		ot::size off;
		if(!_poly.toAddress(v, a, b, off)) {
#			if defined(DCACHE_MEM) || defined(DCACHE_MEM_WARN_TOP)
			cerr << "WARNING: load at any address at " << inst->address() << io::endl;
#			endif
			v = Poly::top;
		}
		else {
			if(a == b)
				v = _state.load(s, a);
			else
				v = _state.load(s, a, b, off);

			// if nothing, look in the initialized memory
			if((v == Poly::top || v == Poly::bot) && a == b && istate->isInitialized(a))
				switch(i.type()) {
				case sem::INT8: 	{ elm::t::uint8  rv; istate->get(a, rv); v = _poly.make(elm::t::int8  (rv)); } break;
				case sem::UINT8: 	{ elm::t::uint8  rv; istate->get(a, rv); v = _poly.make(elm::t::uint8 (rv)); } break;
				case sem::INT16: 	{ elm::t::uint16 rv; istate->get(a, rv); v = _poly.make(elm::t::int16 (rv)); } break;
				case sem::UINT16:	{ elm::t::uint16 rv; istate->get(a, rv); v = _poly.make(elm::t::uint16(rv)); } break;
				case sem::INT32: 	{ elm::t::uint32 rv; istate->get(a, rv); v = _poly.make(elm::t::int32 (rv)); } break;
				case sem::UINT32: 	{ elm::t::uint32 rv; istate->get(a, rv); v = _poly.make(elm::t::uint32(rv)); } break;
				}
		}
	}
#		ifdef DCACHE_MEM
		else
			cerr << "MEM:\t\tload to T\n";
#		endif
	return set(s, i.d(), v);
}

PolyManager::t PolyManager::store(t s, sem::inst i, Inst *inst) {
	Poly::t v = get(s, i.addr());

	// storing to T (bad news)
	if(v == Poly::top) {

		// save: we find a range!
		if(inst->hasProp(otawa::ACCESS_RANGE)) {
			Pair<Address, Address> range = otawa::ACCESS_RANGE(inst);
			s = _state.store(s, range.fst.offset(), range.snd.offset(), 1, get(s, i.d()));
#			ifdef DCACHE_MEM
				cerr << "MEM: store to range [" << range.fst << ", " << range.snd << "]\n";
#			endif
		}

		// T: no more hope
		else {
#			if defined(DCACHE_MEM) || defined(DCACHE_MEM_WARN_TOP)
			cerr << "WARNING:\t\tstore to any address at " << inst->address() << io::endl;
			// DBG(color::IRed() << "WARNING:\t\tstore to any address at " << inst->address() << color::RCol() <<io::endl);
#			endif
			s = _state.storeAtTop(s);		// TODO	Maybe, this should be improved (based on CLP?)
#			ifdef DCACHE_MEM
				cerr << "MEM: store to T\n";
#			endif
		}
	}

	// storing to a known address
	else if(v != Poly::bot) {
#			ifdef DCACHE_MEM
			cerr << "MEM:\t\tstore to "; _poly.dump(cerr, v); cerr << io::endl;
#			endif
		Poly::address_t base, top;
		ot::size off;
		if(!_poly.toAddress(v, base, top, off)) {
#			if defined (DCACHE_MEM) || defined(DCACHE_MEM_WARN_TOP)
			cerr << "MEM:\t\tstore to T\n";
#			endif
			return _state.storeAtTop(s);
		}
		else if(base == top)
			s = _state.store(s, base, get(s, i.d()));
		else
			s = _state.store(s, base, top, off, get(s, i.d()));
	}

	// return result
#		ifdef DCACHE_MEM
		else
			cerr << "MEM:\t\tstore to _\n";
#		endif
	return s;
}


/**
 * Compute the loop controlling the memory access,
 * that is, the number of miss is relative to.
 *
 * Top or bottom references are relative to nothing.
 *
 * @param bb	Container BB.
 * @param r		Looked reference
 * @return	Loop header the access is relative or 0 if it is an absolute number of misses.
 */
BasicBlock *PolyManager::relativeTo(BasicBlock *bb, value_t r) const {

	// trivial cases
	if(r == _poly.top || r == _poly.bot)
		return 0;

	// single access case: relative to the container loop
	if(!r->h) {
		if(LOOP_HEADER(bb))
			return bb;
		else
			return otawa::ENCLOSING_LOOP_HEADER(bb);
	}

	// lookup in the reference
	BasicBlock *h = 0;
	while(r->h) {
		h = r->h;
		r++;
	}
	return h;
}


p::declare PolyAnalysis::reg = p::init("otawa::pidcache::PolyAnalysis", Version(1, 0, 0))
	.provide(POLY_FEATURE)
	.base(CFGProcessor::reg)
	.require(otawa::VIRTUALIZED_CFG_FEATURE)
	.require(otawa::LOOP_INFO_FEATURE)
	.require(dfa::INITIAL_STATE_FEATURE)
	.require(ipet::FLOW_FACTS_FEATURE)
	.maker<PolyAnalysis>();
p::feature POLY_FEATURE("otawa::pidcache::POLY_FEATURE", new Maker<PolyAnalysis>());


extern p::feature POLY_FEATURE;
Identifier<PolyManager *> POLY_MANAGER("otawa::pidcache::POLY_MANAGER", 0);
Identifier<dfa::FastState<Poly>::t> POLY_STATE("otawa::pidcache::POLY_STATE", 0);


/**
 * Manager to use result of the poly-analysis.
 */
PolyManager::PolyManager(WorkSpace *ws, CFG *cfg)
: _poly(allocator),
  _state(&_poly, dfa::INITIAL_STATE(ws), allocator),
  tmps(new value_t[ws->process()->maxTemp()]),
  istate(dfa::INITIAL_STATE(ws))
{
	ASSERTP(istate, "no initial state available");
	_init = _state.bot;

	// initialize default stack address
	_init = _state.set(_init,
		ws->process()->platform()->getSP()->platformNumber(),
		_poly.make(ws->process()->defaultStack()));

	// initialize registers according to the initial state
	for(dfa::State::RegIter reg(istate); reg; reg++) {
		dfa::Value v = (*reg).snd;
		if(v.isConst())
			_init = _state.set(_init, (*reg).fst->platformNumber(), _poly.make(v.value()));
	}

	// initialize memory according to the initial state
	for(dfa::State::MemIter mem(istate); mem; mem++) {
		Address addr = (*mem).address();
		const dfa::Value& val = (*mem).value();
		if(val.isConst())
			_init = _state.store(_init, addr, _poly.make(val.value()));
	}
}

} }		// otawa::dfa

