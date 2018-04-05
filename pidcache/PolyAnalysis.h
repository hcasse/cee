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
#ifndef OTAWA_DFA_POLYANALYSIS_H_
#define OTAWA_DFA_POLYANALYSIS_H_

#include <otawa/sem/PathIter.h>
#include <otawa/prog/Inst.h>
#include <otawa/dfa/FastState.h>
#include "Poly.h"

namespace otawa { namespace pidcache {

class PolyManager {
public:
	typedef dfa::FastState<Poly>::t t;
	typedef Poly::t value_t;
	typedef dfa::FastState<Poly>::register_t reg_t;
	typedef dfa::FastState<Poly>::address_t addr_t;
private:
	class SpecialJoin {
	public:
		inline SpecialJoin(BasicBlock *header, Poly& poly): h(header), p(poly) { }
	private:
		BasicBlock *h;
		Poly& p;
	};

public:

	PolyManager(WorkSpace *ws, CFG *cfg);
	~PolyManager(void) { delete [] tmps; }
	inline dfa::FastState<Poly>& state(void) { return _state; }
	inline Poly& poly(void) { return _poly; }

	inline t init(void) { return _init; }
	inline t bot(void) { return _state.bot;  }
	inline t top(void) { return _state.top; }
	inline t join(const t& d, const t& s) { return _state.join(d, s); }
	inline bool equals(const t& v1, const t& v2) { return _state.equals(v1, v2); }
	inline void set(t& d, const t& s) { d = s; }
	inline void dump(io::Output& out, const t& v) { _state.print(out, v); }
	inline void dump(io::Output& out, value_t v) { _poly.dump(out, v); }

	class Iter: public PreIterator<Iter, t> {
	public:
		inline Iter(PolyManager& m): man(m), _s(man.bot()), _out(man.bot()), i(0) { }
		inline void start(Inst *inst, t s)
			{	path.start(inst); stack.clear(); _s = s; _out = man.bot(); i = inst;
			 	 if(path.isCond()) stack.push(s); }

		inline bool ended(void) const { return path.ended(); }
		inline t item(void) const { return _s; }

		inline void next(void) {
			_s = man.update(i, *path, _s);
			path.next();
			if(path.pathEnd()) {
				_out = man.join(_out, _s);
				if(!path.ended())
					_s = stack.pop();
				else
					_s = man.bot();
			}
			else if(path.isCond())
				stack.push(_s);
		}

		inline sem::inst inst(void) const { return *path; }
		inline t state(void) const { return _s; }
		inline t out(void) { if(_s != man.bot()) { _out = man.join(_out, _s); _s = man.bot(); } return _out; }

	private:
		PolyManager& man;
		Inst *i;
		t _s;
		t _out;
		genstruct::Vector<t> stack;
		otawa::sem::PathIter path;
	};

	t update(Inst *i, sem::inst si, t s);
	t update(Iter& iter, Inst *inst, t s);
	t update(Iter& iter, BasicBlock *bb, t d);

	inline value_t get(t s, int r) {
		if(r < 0)
			return tmps[-r];
		else
			return _state.get(s, r);
	}

	BasicBlock *relativeTo(BasicBlock *bb, value_t r) const;

private:

	inline t set(t s, int r, value_t v) {
		if(r < 0) {
			tmps[-r] = v;
			return s;
		}
		else
			return _state.set(s, r, v);
	}

	t load(t s, sem::inst i, Inst *inst);
	t store(t s, sem::inst i, Inst *inst);

	StackAllocator allocator;
	Poly _poly;
	dfa::FastState<Poly> _state;
	value_t *tmps;
	t _init;
	dfa::State *istate;
};

extern p::feature POLY_FEATURE;
extern Identifier<PolyManager *> POLY_MANAGER;
extern Identifier<dfa::FastState<Poly>::t> POLY_STATE;

} }	// otawa::dfa

#endif	// OTAWA_DFA_POLYANALYSIS_H_
