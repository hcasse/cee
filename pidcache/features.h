/*
 *	otawa::pidcache plugin hook
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
#ifndef PIDCACHE_FEATURES_H
#define  PIDCACHE_FEATURES_H

#include <otawa/ipet.h>
#include <otawa/proc/Feature.h>
#include "Poly.h"

namespace otawa { namespace pidcache {

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

extern p::feature ACCESSES_FEATURE;
extern Identifier<Bag<PolyAccess> > ACCESSES;

extern p::feature CONSTRAINTS_FEATURE;
extern Identifier<ilp::Var *> MISS_VAR;

extern p::feature WCET_FUNCTION_FEATURE;

extern p::feature EVENT_FEATURE;

} }	// otawa::pidcache

#endif	// PIDCACHE_FEATURES_H
