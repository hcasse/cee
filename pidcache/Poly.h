/*
 *	Poly class -- Poly analysis domain
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2011-12, IRIT UPS.
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
#ifndef OTAWA_DFA_POLY_H_
#define OTAWA_DFA_POLY_H_

#include <elm/types.h>
#include <elm/alloc/StackAllocator.h>
#include <otawa/cfg/BasicBlock.h>

namespace otawa { namespace pidcache {

using namespace elm;

class Poly {
public:
	static const int cap = 8;
	typedef t::uint32 address_t;
	typedef t::int32 coef_t;
	class pair_t {
	public:
		coef_t c;
		BasicBlock *h;
		inline pair_t(void): c(0), h(0) { }
		inline pair_t(coef_t _c, BasicBlock *_h): c(_c), h(_h) { }
		inline bool operator==(const pair_t& v) const { return h == v.h && c == v.c; }
		inline bool operator!=(const pair_t& v) const { return h == v.h && c == v.c; }
	};
	typedef pair_t *t;
	static pair_t top[], bot[];

	inline Poly(StackAllocator& alloc): allocator(alloc) { }

	bool toAddress(t v, address_t& base, address_t& top, ot::size& off);
	bool isTopPrecise(t v);
	inline bool isConst(t v) { return v != top && !v[0].h; }
	coef_t count(t v);
	inline bool isConstant(t v) { return v != top && !v[0].h; }
	coef_t base(t v);

	t make(coef_t c);
	inline t neg(t p) { return top; }
	inline t inv(t p) { return top; }
	t add(t p1, t p2);
	t sub(t p1, t p2);
	t shl(t p1, t p2);
	inline t shr(t p1, t p2) { return top; }
	inline t asr(t p1, t p2) { return top; }
	inline t _and(t p1, t p2) { return top; }
	t _or(t p1, t p2);
	inline t _xor(t p1, t p2) { return top; }
	t mul(t p1, t p2);
	inline t div(t p1, t p2) { return top; }
	inline t mod(t p1, t p2) { return top; }
	inline t umul(t p1, t p2) { return top; }
	inline t udiv(t p1, t p2) { return top; }
	inline t umod(t p1, t p2) { return top; }
	bool equals(t p1, t p2) const;
	t join(t p1, t p2);
	t loop_join(BasicBlock *h, t in, t back);
	t widen(BasicBlock *h, t prev, t next);
	t filter(Edge *edge, t p);

	t exwiden(BasicBlock *h, t prev, t next);
	t exloop_join(t s1, t s2);
	bool includes(t set, t sub);

	void dump(io::Output& out, t p) const;

private:
	inline t allocate(int n = cap) { return t(allocator.allocate(sizeof(pair_t) * n)); }
	coef_t looselyAdd(coef_t v1, coef_t v2, bool& lost);
	coef_t looselyShl(coef_t v, coef_t shift, bool& lost);
	coef_t looselyMul(coef_t v1, coef_t v2, bool & lost);
	StackAllocator& allocator;
};

extern Identifier<Poly::pair_t *> POLY_TOP;
extern Identifier<Poly::pair_t *> POLY_BOTTOM;

} }		// otawa::dfa

#endif /* OTAWA_DFA_POLY_H_ */
