/*
 *	PIDAnalysis utilities
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
#ifndef OTAWA_PIDCACHE_PIDANALYSIS_H_
#define OTAWA_PIDCACHE_PIDANALYSIS_H_

#include <elm/type_info.h>
#include <elm/int.h>
#include <elm/io.h>
#include <elm/genstruct/Vector.h>
#include <otawa/util/LoopInfoBuilder.h>

namespace otawa { namespace pidcache {

using namespace elm;

// useful definitions
typedef elm::t::int8 age_t;
static const age_t NO_AGE = -1;

inline void print(io::Output& out, age_t a, age_t A) {
	if(a == NO_AGE)
		out << '_';
	else if(a >= A)
		out << 'E';
	else
		out << a;
}


// MUST Problem
class Must {
public:
	typedef age_t t;

	inline Must(age_t A): _A(A) { }
	inline void init(t& a) const { a = NO_AGE; }
	inline bool equals(t a1, t a2) const { return a1 == a2; }

	inline void join(t& r, t a1, t a2)
		{ if(a1 == NO_AGE || a2 == NO_AGE) r = NO_AGE; else r = max(a1, a2); }

	inline void update(t& r, t a, age_t h) const
		{ if(a <= h) r = older(a); else r = a;  }

	inline void undef(t& v) const { v = _A; }

	inline void purge(t& a) const { a = _A; }

	inline void touch(t& r) const { r = 0; }

	inline void print(io::Output& out, t a) const { otawa::pidcache::print(out, a, _A); }

	inline bool isAlive(age_t age) { return age >= 0 && age < _A; }

private:
	inline age_t older(age_t a) const
		{ if(a == NO_AGE || a == _A) return a; else return a + 1; }
	age_t _A;
};


// MAY Problem
class May {
public:
	typedef age_t t;

	inline May(age_t A): _A(A) { ASSERT(_A > 0); }
	inline void init(t& a) const { a = 0; }
	inline void undef(t& v) const { v = 0; }
	inline bool equals(t a1, t a2) const { return a1 == a2; }

	inline void join(t& r, t a1, t a2) { r = min(a1, a2); }

	inline void update(t& r, t a, age_t h) const
		{ if(a <= h) r = older(a); else r = a;  }

	inline void touch(t& r) const { r = 0; }

	inline void print(io::Output& out, t a) const { otawa::pidcache::print(out, a, _A); }

	inline bool isAlive(age_t age) { return age >= 0 && age < _A; }

private:
	inline age_t older(age_t a) const
		{ if(a == NO_AGE || a == _A) return a; else return a + 1; }
	age_t _A;
};


// Persistence problem
class Persistence {
public:
	typedef genstruct::Vector<age_t> t;

	inline Persistence(age_t A): _A(A) { }

	inline void init(t& a, BasicBlock *bb) const {

		// count levels
		int cnt = 1;
		if(LOOP_HEADER(bb))
			cnt++;
		BasicBlock *header = ENCLOSING_LOOP_HEADER(bb);
		while(header) {
			cnt++;
			header = ENCLOSING_LOOP_HEADER (header);
		}

		// build the value
		a.setLength(cnt);
		for(int i = 0; i < cnt; i++)
			a[i] = 0;
	}

	inline void undef(t& a, BasicBlock *bb) const { init(a, bb); }

	inline void undef(t& a) const
		{ for(int i = 0; i < a.length(); i++) a[i] = NO_AGE; }

	inline void purge(t& a) const
		{ for(int i = 0; i < a.length(); i++) a[i] = _A; }

	inline bool equals(const t& a1, const t& a2) const {
		for(int i = 0; i < a1.length(); i++)
			if(a1[i] != a2[i])
				return false;
			return true;
	}

	inline void join(t& r, const t& a1, const t& a2) const {
		ASSERTP(a1.length() == a2.length(), "Persitence::join : a2.length() != a1.length()")
		for(int i = 0; i < a1.length(); i++)
			r[i] = join(a1[i], a2[i]);
	}

	inline void update(t& r, const t& a, age_t h) const {
		for(int i = 0; i < a.length(); i++)
			r[i] = a[i] <= h ? older(a[i]) : a[i];
	}

	inline void touch(t& a) const {
		for(int i = 0; i < a.length(); i++)
			a[i] = 0;
	}

	inline void print(io::Output& out, const t& a) {
		for(int i = 0; i < a.length(); i++)
			otawa::pidcache::print(out, a[i], _A);
	}

	inline void enter(t& r, const t& a) {
		r.setLength(a.length() + 1);
		r[0] = NO_AGE;
		for(int i = 0; i < a.length(); i++)
			r[i + 1] = a[i];
	}

	inline void leave(t& r, const t& a) {
		ASSERT(a.length() > 1);
		r.setLength(a.length() - 1);
		r[0] = join(a[0], a[1]);
		for(int i = 2; i < a.length(); i++)
			r[i - 1] = a[i];
	}

	inline bool isAlive(const t& a) const { return a[0] >= 0 && a[0] < _A; }

private:
	inline age_t join(age_t a1, age_t a2) const { return max(a1, a2); }

	inline age_t older(age_t a) const
		{ if(a == NO_AGE || a == _A) return a; else return a + 1; }
	age_t _A;
};

} }		// otawa::pidcache

#endif /* OTAWA_PIDCACHE_PIDANALYSIS_H_ */
