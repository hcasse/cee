/*
 *	PolyAccessBuilder implementation
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

#include <otawa/proc/BBProcessor.h>
#include <otawa/hard/Memory.h>

#include "PIDCache.h"
#include "PIDAnalysis.h"

namespace otawa { namespace pidcache {

/**
 * Get the list of data accesses using pidcache encoding.
 *
 * @par Hooks
 * @li @ref BasicBlock
 *
 * @par Features
 * @li @ref ACCESS_FEATURE
 */
Identifier<Bag<PolyAccess> > ACCESSES("otawa::pidcache::POLY_ACCESSES");

/**
 * Print the PID access.
 * @param out	Output stream.
 */
void PolyAccess::print(io::Output& out, const Poly& poly) const {
	switch(_access) {
	case NONE:	out << "none"; 		return;
	case LOAD:	out << "load "; 	break;
	case STORE:	out << "store ";	break;
	}
	poly.dump(out, _ref);
	if(!_cached)
		out << " [not cached]";
	out << " @" << _inst->address();
}


class AccessesBuilder: public BBProcessor {
public:
	static p::declare reg;
	AccessesBuilder(p::declare& r = reg): BBProcessor(r), man(0) { }

protected:

	void setup(WorkSpace *ws) {
		man = POLY_MANAGER(ws);
		ASSERT(man);
		mem = hard::MEMORY(ws);
	}

	void logAccess(PolyAccess acc) {
		log << "\t\t\t" << acc.inst()->address();
		switch(acc.access()) {
		case PolyAccess::LOAD:		log << " load at "; break;
		case PolyAccess::STORE:		log << " store at "; break;
		default:					ASSERT(false); break;
		}
		man->poly().dump(log, acc.ref());
		cerr << io::endl;
	}

	bool accessCache(Inst *inst, PolyManager::value_t ref) {
		PolyManager::addr_t lo, hi;
		ot::size off;

		ASSERTP(man->poly().bot != ref, "Accessing _ address @" << inst->address());

		if(man->poly().top != ref)
			man->poly().toAddress(ref, lo, hi, off);
		else if(inst->hasProp(otawa::ACCESS_RANGE)) {
			Pair<Address, Address> range = otawa::ACCESS_RANGE(inst);
			lo = range.fst.offset();
			hi = range.snd.offset();
		}
		else
			return true;

		const hard::Bank *bank = mem->get(lo);
		if(!bank)
			throw ProcessorException(*this, _ << "no bank for address " << Address(lo) << " at " << inst->address());

		return bank->isCached();
	}

	virtual void processBB(WorkSpace *ws, CFG *cfg, BasicBlock *bb) {
		genstruct::Vector<PolyAccess> accs;
		PolyManager::Iter iter(*man);
		PolyManager::value_t ref;

		// collect all accesses
		PolyManager::t s = POLY_STATE(bb);
		for(BasicBlock::InstIter inst(bb); inst; inst++) {
			for(iter.start(inst, s); iter; iter++)
				switch(iter.inst().op) {
				case sem::LOAD:
					ref = man->get(iter, iter.inst().addr());
					accs.add(PolyAccess(inst, PolyAccess::LOAD, ref, accessCache(inst, ref)));
					if(this->logFor(Processor::LOG_INST))
						logAccess(accs.top());
					if(man->poly().equals(accs.top().ref(), man->poly().top)) {
						cerr << "WARNING: load from T in " << accs.top().inst()->address() << " ";
						accs.top().print(cerr, man->poly());
						cerr << io::endl;
					}
					break;
				case sem::STORE:
					ref = man->get(iter, iter.inst().addr());
					accs.add(PolyAccess(inst, PolyAccess::STORE, ref, accessCache(inst, ref)));
					if(this->logFor(Processor::LOG_INST))
						logAccess(accs.top());
					if(man->poly().equals(accs.top().ref(), man->poly().top)) {
						cerr << "WARNING: store to T in " << accs.top().inst()->address() << " ";
						accs.top().print(cerr, man->poly());
						cerr << io::endl;
					}					break;
				}
			s = iter.out();
		}

		// finalize
		//accs.add(PolyAccess());
		ACCESSES(bb) = Bag<PolyAccess>(accs);
	}

private:
	PolyManager *man;
	const hard::Memory *mem;
};

p::declare AccessesBuilder::reg = p::init("otawa::pidcache::AccessesBuilder", Version(1, 0, 0))
	.maker<AccessesBuilder>()
	.provide(ACCESSES_FEATURE)
	.require(POLY_FEATURE)
	.require(hard::MEMORY_FEATURE);

/**
 * This feature ensures that data cache accesses has been added to the basic block (for PID cache analysis).
 *
 * @par Properties
 * @li @ref ACCESSES
 *
 * @par Default Implementation
 * @li @ref PolyAccessesBuilder
 */
p::feature ACCESSES_FEATURE("otawa::pidcache::ACCESSES_FEATURE", new Maker<AccessesBuilder>());

} }		// otawa::pidcache
