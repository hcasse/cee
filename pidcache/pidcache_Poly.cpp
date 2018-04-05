/*
 *	Poly class -- OTAWA loader to support TriCore with GLISS2
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

#include <elm/util/array.h>
#include <otawa/util/FlowFactLoader.h>
#include <otawa/util/Dominance.h>
#include <otawa/cfg/Edge.h>
#include <elm/log/Log.h>
#include "Poly.h"

namespace otawa { namespace pidcache {

Poly::pair_t Poly::top[] = { };
Poly::pair_t Poly::bot[] = { };

/**
 * Convert a value to an address.
 * @param v		Value to convert.
 * @param base	Base address.
 * @param top	Top address (after last reference).
 * @param off	Offset.
 * @return		True if the address is valid, false else.
 */
bool Poly::toAddress(t v, address_t& base, address_t& top, ot::size& off) {
	ASSERT(v != Poly::top && v != Poly::bot);
	if(v->h == 0) {
		base = top = v->c;
		off = 0;
	}
	else {
		// TODO		Loosely version for now
		address_t pos = 0, neg = 0;
		pair_t *p;
		for(p = v; p->h; p++) {
			int n = MAX_ITERATION(p->h);
			if(n < 0)
				return false;
			if(n != 0) {
				if(p->c < 0)
					neg += p->c * (n - 1);
				else
					pos += p->c * (n - 1);
			}
		}
		base = p->c + neg;
		top = p->c + pos;
		off = v->c;
	}
	return true;
}


/**
 * Check if the top address if precise, that is,
 * the result of loops with constant number of iterations.
 */
bool Poly::isTopPrecise(t v) {
	ASSERT(v != Poly::top && v != Poly::bot);
	while(v->h) {
		int min = MIN_ITERATION(v->h);
		int max = MAX_ITERATION(v->h);
		if(min < 0 || min != max)
			return false;
		v++;
	}
	return true;
}


/**
 * Compute the max number of elements represented by these poly value.
 * @return	Max number of element (if positive), negative denotes the missing of a loop bound.
 */
Poly::coef_t Poly::count(t v) {
	coef_t cnt = 1;
	t p = v;
	while(p->h) {
		int max = MAX_ITERATION(p->h);
		if(max >= 0)
			cnt *= max;
		else {
			cnt = -1;
			break;
		}
		p++;
	}
	return cnt;
}


/**
 * Get the base (K0) of the given value.
 * @param v		Value to look base for.
 * @return		Poly value base.
 */
Poly::coef_t Poly::base(t v) {
	t p = v;
	while(p->h)
		p++;
	return p->c;
}


/**
 * Perform addition and check result for loose of precision.
 * @param v1	First value to add.
 * @param v2	Second value to add.
 * @param lost	Return true if precision is lost, false else.
 * @return		Loosely v1 + v2.
 */
Poly::coef_t Poly::looselyAdd(coef_t v1, coef_t v2, bool& lost) {
	if(v1 < 0) {
		if(v2 < 0)
			lost = max(msb(-v1), msb(-v2)) >= 30; // maybe 31 is ok?
		else
			lost = false;
	}
	else {
		if(v2 < 0)
			lost = false;
		else
			lost = max(msb(v1), msb(v2)) >= 30; // maybe 31 is ok?
	}
	return v1 + v2;
}


/**
 * Perform shift-left but look for loose of precision (left-bit losts).
 * @param v		Value to shift.
 * @param s		Shift to perform.
 * @param lost	(out) True if precision is lost, false else.
 * @return		v shift s times to left.
 */
Poly::coef_t Poly::looselyShl(coef_t v, coef_t s, bool& lost) {
	lost = (v < 0 && ((v >> (31 - s)) != -1))
		|| (v > 0 && ((v >> (31 - s)) != 0));
	return v << s;
}


/**
 * Perform a multiplication of the coefficient and the consistency of the result.
 * @param v1	First value to multiply.
 * @param v2	Second value to multiply.
 * @param lost	True if precision is lost, false else.
 * @return		v1 times v2.
 */
Poly::coef_t Poly::looselyMul(coef_t v1, coef_t v2, bool & lost) {
	lost = msb(elm::abs(v1)) + msb(elm::abs(v2)) >= 31;
	return v1 * v2;
}


Poly::t Poly::make(coef_t c) {
	t r = t(allocator.allocate(sizeof(pair_t)));
	*r = pair_t(c, 0);
	return r;
}


/**
 * @fn bool Poly::isConstant(t v);
 * Test if value v is a constant.
 * @param v		Value to test.
 * @return		True if it is a constant, false else.
 */


Poly::t Poly::_or(t p1, t p2) {
	if(isConstant(p1) && isConstant(p2))
		return make(p1->c | p2->c);
	else
		return top;
}

Poly::t Poly::add(t p1, t p2) {
	//cerr << "DEBUG: add("; dump(cerr, p1); cerr << ", "; dump(cerr, p2); cerr << ") = ";
	if(p1 == bot)
		return p2;
	else if(p2 ==  bot)
		return p1;
	else if(p1 == top || p2 == top)
		return top;
	else {
		t r = allocate(), p = r;
		bool lost;
		while(p1->h || p2->h) {
			if(p1->h == p2->h) {
				coef_t c = looselyAdd(p1->c,  p2->c, lost);
				if(lost)
					return top;
				if(c)
					*p++ = pair_t(c, p1->h);
				p1++;
				p2++;
			}
			else if(p1->h && p2->h) {
				if(Dominance::dominates(p1->h, p2->h))
					*p++ = *p2++;
				else
					*p++ = *p1++;
			}
			else if(!p1->h)
				*p++ = *p2++;
			else
				*p++ = *p1++;
			if(p >= r + cap)
				throw MessageException("Poly: too complex polynom");
		}
		*p = pair_t(p1->c + p2->c, 0);
		return r;
	}
}


Poly::t Poly::sub(t p1, t p2) {
	if(p1 == bot)
		return p2;
	else if(p2 ==  bot)
		return p1;
	else if(p1 == top || p2 == top)
		return top;
	else {
		t r = allocate(), p = r;
		bool lost;
		while(p1->h || p2->h) {
			if(p1->h == p2->h) {
				coef_t c = looselyAdd(p1->c,  -p2->c, lost);
				if(lost)
					return top;
				if(c)
					*p++ = pair_t(c, p1->h);
				p1++;
				p2++;
			}
			else if(p1->h && p2->h) {
				if(Dominance::dominates(p1->h, p2->h))
					*p++ = *p2++;
				else
					*p++ = *p1++;
			}
			else if(!p1->h)
				*p++ = *p2++;
			else
				*p++ = *p1++;
			if(p >= r + cap)
				throw MessageException("Poly: too complex polynom");
		}
		*p = pair_t(p1->c - p2->c, 0);
		return r;
	}
}


Poly::t Poly::shl(t p1, t p2) {
	if(p1 == bot)
		return p2;
	else if(p2 ==  bot)
		return p1;
	else if(p1 == top || p2 == top || p2[0].c >= 31)
		return top;
	else {
		t p = allocate();
		bool done = false, lost;
		for(int i = 0; !done; i++) {
			p[i] = pair_t(looselyShl(p1[i].c, p2[0].c, lost), p1[i].h);
			if(lost)
				return top;
			done = !p1[i].h;
		}
		return p;
	}
}


Poly::t Poly::mul(t p1, t p2) {
	if(p1 == bot)
		return p2;
	else if(p2 ==  bot)
		return p1;
	else if(p1 == top || p2 == top)
		return top;
	else if(!p2[0].h) {
		t p = allocate();
		bool done = false, lost;
		for(int i = 0; !done; i++) {
			p[i] = pair_t(looselyMul(p1[i].c, p2[0].c, lost), p1[i].h);
			if(lost)
				return top;
			done = !p1[i].h;
		}
		return p;
	}
	else if(!p1[0].h)
		return mul(p2, p1);
	else
		return top;
}


/**
 * Test whether both values are equal.
 * @param v1	First value.
 * @param v2	Second value.
 */
bool Poly::equals(t p1, t p2) const {
	if(p1 == p2)
		return true;
	if(p1 == bot || p2 == bot || p1 == top || p2 == top)
		return false;
	while(*p1 == *p2) {
		if(!p1->h)
			return true;
		p1++;
		p2++;
	}
	return false;
}


Poly::t Poly::join(t p1, t p2) {
	// TODO	Generally false implementation, should fixed using paths.
	return exloop_join(p1, p2);
#	if 0
		t r;
		if(p1 == p2)
			r = p1;
		if(equals(p1, p2))
			r = p1;
		else
			r = top;
		return r;
#	endif
}


/**
 * Test if set includes(not strictly)  the given subset.
 * @param set	Set.
 * @param sub	Subset.
 * @return		True if set includes sub, false else.
 */
bool Poly::includes(t set, t sub) {
	if(equals(set, sub))
		return true;
	else if(!set->h)
		return false;
	else
		return equals(set + 1, sub);
}


/**
 * Extended widening.
 * @param h		Concerned header file.
 * @param prev	Previous value.
 * @param next	Next value.
 * @return		Widened value.
 */
Poly::t Poly::exwiden(BasicBlock *h, t prev, t next) {
	Poly::t r;
	if(equals(bot, top))						// a \/ a = a
		r = next;
	else if(prev == bot)						// _ \/ a = a
		r = next;
	else if(next == bot)						// a \/ _ = a
		r = prev;
	else if(prev == top || next == top)			// 	a \/ T = T \/ a = T
		r = top;
	else if(includes(next, prev))				// a \/ b = b if a subset of b
		r = next;
	else {
		t k = sub(next, prev);
		if(k == top)
			r = top;
		else {
			if(k[0].h)
				r = top;
			else if(next->h == h && next->c == k[0].c)	// a \/ b = a + ki if b - a = k and ki not in a
				r = prev;
			else {										// a \/ b = b if b - a = k and kin in a
				r = allocate();
				r[0] = pair_t(k[0].c, h);
				int i;
				for(i = 0; prev[i].h; i++)
					r[i + 1] = prev[i];
				r[i + 1] = prev[i];
			}
		}
	}
	//cerr << "DEBUG: "; dump(cerr, prev); cerr << " W "; dump(cerr, next); cerr << " = "; dump(cerr, r); cerr << io::endl;
	return r;
}


/**
 * Extended loop join (supporting entering and looping state traces).
 */
Poly::t Poly::exloop_join(t s1, t s2) {
	t r, p1 = s1, p2 = s2;

	// trivial cases
	if(p1 == bot)
		r = p2;
	else if(p2 == bot)
		r = p1;
	else if(p1 == top || p2 == top)
		r = top;

	// compute it
	else {
		r = allocate();
		int i = 0;

		// join
		while(p1->h && p2->h) {
			ASSERT(i < cap);
			if(p1->h == p2->h) {
				if(p1->c == p2->c) {
					r[i++] = *p1;
					//cerr << "DEBUG: -->  " << p1->c << " " << p1->h << " U " << p2->c << " " << p2->h << " = " << r[i - 1].c << ' ' << r[i - 1].h << io::endl;
					p1++;
					p2++;
				}
				else {
					r = top;
					break;
				}
			}
			else if(Dominance::dominates(p1->h, p2->h)) {
				r[i++] = *p2;
				//cerr << "DEBUG: -->  " << p1->c << " " << p1->h << " U " << p2->c << " " << p2->h << " = " << r[i - 1].c << ' ' << r[i - 1].h << io::endl;
				p2++;
			}
			else {
				r[i++] = *p1;
				//cerr << "DEBUG: -->  " << p1->c << " " << p1->h << " U " << p2->c << " " << p2->h << " = " << r[i - 1].c << ' ' << r[i - 1].h << io::endl;
				p1++;
			}
		}

		// process remaining
		if(r != top) {
			while(p1->h) {
				ASSERT(i < cap);
				r[i++] = *p1;
				//cerr << "DEBUG: +++ " << p1->c << " " << p1->h << io::endl;
				p1++;
			}
			while(p2->h) {
				ASSERT(i < cap);
				r[i++] = *p2;
				//cerr << "DEBUG: +++ " << p2->c << " " << p2->h << io::endl;
				p2++;
			}
			if(p1->c == p2->c)
				r[i++] = *p1;
			else
				r = top;
		}
	}

	// return result
	//cerr << "DEBUG: "; dump(cerr, s1); cerr << " U "; dump(cerr, s2); cerr << " = "; dump(cerr, r); cerr << io::endl;
	return r;
}


Poly::t Poly::widen(BasicBlock *h, t prev, t next) {
	if(prev == bot)
		return next;
	if(equals(prev, next))
		return prev;
	else if(next->h == h && equals(prev, next + 1))
		return next;
	else
		return top;
}


Poly::t Poly::loop_join(BasicBlock *h, t in, t back) {

	// no back
	if(equals(in, back))
		return in;
	if(back == bot)
		return in;
	if(in == bot || in == top || back == top)
		return top;

	// already loop-extended
	if(back->h == h) {
		pair_t k(back->c, 0);
		t nback = sub(back, &k);
		if(!equals(nback + 1, in))
			return top;
		return nback;
	}

	// perform extension
	t k = sub(back, in);
	if(k[0].h)
		return top;
	t r = allocate();
	r[0] = pair_t(k[0].c, h);
	int i;
	for(i = 0; in[i].h; i++)
		r[i + 1] = in [i];
	r[i + 1] = in[i];
	return r;
}


Poly::t Poly::filter(Edge *edge, t p) {
	if(p[0].h != edge->source())
		return p;
	int max = MAX_ITERATION(edge->source()),
		min = MIN_ITERATION(edge->source());
	if(max < 0 || max != min)
		return top;
	t r = allocate();
	int i;
	for(i = 1; p[i].h; i++)
		r[i - 1] = p[i];
	r[i - 1] = pair_t(p[0].c * max + p[i].c, 0);
	return top;
}


/**
 * Dump the current poly value.
 */
void Poly::dump(io::Output& out, t p) const {
	if(p == top)
		out << "T";
	else if(p == bot)
		out << "_";
	else {
		int i;
		for(i = 0; p[i].h; i++)
			out << p[i].c << " I" << p[i].h->number() << " + ";
		out << "0x" << io::hex(p[i].c);
	}
}

Identifier<Poly::pair_t *> POLY_TOP("otawa::dfa::POLY_TOP", Poly::top);
Identifier<Poly::pair_t *> POLY_BOTTOM("otawa::dfa::POLY_BOTTOM", Poly::bot);

} }	// otawa::dfa
