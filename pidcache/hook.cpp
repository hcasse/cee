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

#include <otawa/hard/Memory.h>
#include <otawa/ilp/expr.h>
#include <otawa/ipet.h>
#include <otawa/proc/ProcessorPlugin.h>

#include "PIDCache.h"
#include "PolyAnalysis.h"

namespace otawa { namespace pidcache {

using namespace otawa;
using namespace otawa::ilp;

// Building of constraints

Identifier<ilp::Var *> MISS_VAR("otawa::pidcache::MISS_VAR");

class ConstraintBuilder: public BBProcessor {
public:
	static p::declare reg;
	ConstraintBuilder(p::declare& r = reg): BBProcessor(r), sys(0), _explicit(false) {
	}

	virtual void configure(const PropList& props) {
		BBProcessor::configure(props);
		_explicit = ipet::EXPLICIT(props);
	}

protected:

	virtual void setup(WorkSpace *ws) {
		sys = ipet::SYSTEM(ws);
		ASSERT(sys);
	}

	virtual void processBB(WorkSpace *ws, CFG *cfg, BasicBlock *bb) {
		static string label = "miss for data cache";

		if(bb->isEnd())
			return;
		Bag<PolyAccess>& accesses = *ACCESSES(bb);
		for(int i = 0; i < accesses.count(); i++) {
			PolyManager *man = POLY_MANAGER(ws);

			// get count of misses
			PolyAccess& access = accesses[i];
			miss_count_t miss = MISS_COUNT(access);
			if(!miss)
				continue;

			// get relative BB variable
			BasicBlock *rel = RELATIVE_TO(bb);

			// build x_miss
			string name;
			if(_explicit)
				name = _ << "x_miss_" << bb->number() << "_" << cfg->number() << "_" << access.inst()->address();
			var x_miss(sys, Var::INT, name);
			MISS_VAR(access) = x_miss;
			model m(sys);

			// 0 <= x_miss <= x_i /\ x_miss <= MISS * x_relative
			m(label) % 0 <= x_miss;
			m(label) % x_miss <= var(ipet::VAR(bb));
			if(rel)
				m(label) % x_miss <= miss * var(ipet::VAR(rel));
			else
				m(label) % x_miss <= miss;
		}
	}

private:
	bool _explicit;
	ilp::System *sys;
};

p::feature CONSTRAINTS_FEATURE("otawa::pidcache::CONSTRAINTS_FEATURE", new Maker<ConstraintBuilder>());

p::declare ConstraintBuilder::reg = p::init("otawa::pidcache::ConstraintBuilder", Version(1, 0, 0))
	.maker<ConstraintBuilder>()
	.base(BBProcessor::reg)
	.provide(CONSTRAINTS_FEATURE)
	.require(otawa::ipet::ASSIGNED_VARS_FEATURE)
	.require(ANALYSIS_FEATURE);


// WCETFunctionBuilder processor

class WCETFunctionBuilder: public BBProcessor {
public:
	static p::declare reg;
	WCETFunctionBuilder(p::declare& r = reg): BBProcessor(r), sys(0), mem(0), poly(0), max_load(0), max_store(0) { }

protected:

	virtual void setup(WorkSpace *ws) {
		sys = ipet::SYSTEM(ws);
		ASSERT(sys);
		mem = hard::MEMORY(ws);
		ASSERT(mem);
		PolyManager *man = pidcache::POLY_MANAGER(ws);
		ASSERT(man);
		poly = &man->poly();
	}

	virtual void processBB(WorkSpace *ws, CFG *cfg, BasicBlock *bb) {
		if(bb->isEnd())
			return;
		Bag<PolyAccess>& accesses = *ACCESSES(bb);
		for(int i = 0; i < accesses.count(); i++) {

			// get x_miss
			PolyAccess& access = accesses[i];
			ilp::Var *x_miss = pidcache::MISS_VAR(access);
			if(!x_miss)
				continue;

			// compute the access time
			ot::time time;
			if(poly->equals(access.ref(), poly->top))
				time = access.access() == PolyAccess::LOAD ? mem->worstReadAccess() : mem->worstWriteAccess();
			else {
				const hard::Bank *b = mem->get(Address(poly->base(access.ref())));
				if(!b)
					throw ProcessorException(*this, _ << "access to " << Address(poly->base(access.ref())) << " at " << access.inst()->address() << " does not point into known memory bank.");
				time = access.access() == PolyAccess::LOAD ? b->latency() : b->writeLatency();
			}

			// time * x_miss added to WCET function
			sys->addObjectFunction(time, x_miss);
		}

	}

private:
	ilp::System *sys;
	const hard::Memory *mem;
	Poly *poly;
	ot::time max_load, max_store;
};

p::feature WCET_FUNCTION_FEATURE("otawa::pidcache::WCET_FUNCTION_FEATURE", new Maker<WCETFunctionBuilder>());

p::declare WCETFunctionBuilder::reg = p::init("otawa::pidcache::WCETFunctionBuilder", Version(1, 0, 0))
	.require(CONSTRAINTS_FEATURE)
	.require(hard::MEMORY_FEATURE)
	.require(POLY_FEATURE)
	.provide(WCET_FUNCTION_FEATURE)
	.base(BBProcessor::reg)
	.maker<WCETFunctionBuilder>();


// Plugin declaration
class Plugin: public ProcessorPlugin {
public:
	Plugin(void): ProcessorPlugin("otawa::pidcache", Version(1, 0, 0), OTAWA_PROC_VERSION) { }
};

} }		// otawa::pidcache

otawa::pidcache::Plugin OTAWA_PROC_HOOK;
otawa::pidcache::Plugin& otawa_pidcache = OTAWA_PROC_HOOK;
