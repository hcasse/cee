#include <otawa/app/Application.h>
#include <otawa/cache/cat2/features.h>
#include <otawa/dcache/features.h>
#include <otawa/cfg/features.h>
#include <otawa/hard/CacheConfiguration.h>
#include <otawa/etime/features.h>
#include <otawa/ipet/features.h>

#include "pidcache/PIDCache.h"

using namespace elm;
using namespace otawa;

class CEE: public Application {
public:
	CEE(void): Application("CEE", Version(1, 0, 0)),
	quiet(option::SwitchOption::Make(*this).cmd("-q").cmd("--quiet").description("Do not display header line")),
	dcache(option::SwitchOption::Make(*this).cmd("-d").cmd("--dcache").description("Perform simple data cache analysis")),
	pcache(option::SwitchOption::Make(*this).cmd("-p").cmd("--pidcache").description("Perform PID data cache analysis")),
	icache(option::SwitchOption::Make(*this).cmd("-i").cmd("--icache").description("Perform instruction cache analysis")),
	wcet(option::SwitchOption::Make(*this).cmd("-w").cmd("--wcet").description("Compute the WCET"))
	{
	}
	
protected:

	void performICacheAnalysis(void) {
		// launch instruction cache analysis
		require(ICACHE_ACS_MAY_FEATURE);
		require(ICACHE_CATEGORY2_FEATURE);
		if(wcet)
			require(ICACHE_ONLY_CONSTRAINT2_FEATURE);

		// compute statistics
		t::uint64
			cnt = 0,
			ah = 0,
			am = 0,
			pe = 0,
			nc = 0;
		const CFGCollection& coll = **INVOLVED_CFGS(workspace());
		for(int i = 0; i < coll.count(); i++)
			for(CFG::BBIterator bb(coll.get(i)); bb; bb++) {
				AllocatedTable<LBlock *> *tab = BB_LBLOCKS(bb);
				if(tab)
					for(int i = 0; i < tab->count(); i++) {
						switch(CATEGORY(tab->get(i))) {
						case ALWAYS_HIT:		ah++; break;
						case ALWAYS_MISS:		am++; break;
						case FIRST_MISS:		pe++; break;
						case NOT_CLASSIFIED:	nc++; break;
						default:				ASSERT(false); break;
						}
						cnt++;
					}
			}
		cout
			<< io::width(8, cnt).right()
			<< io::width(8, ah).right()
			<< io::width(8, am).right()
			<< io::width(8, pe).right()
			<< io::width(8, nc).right()
			<< ' ' << workspace()->process()->program()->name()
			<< io::endl;
	}

	void performDCacheAnalysis(void) {
		
		// data cache analysis
		require(dcache::CLP_BLOCK_FEATURE);
		require(dcache::MAY_ACS_FEATURE);
		require(dcache::CATEGORY_FEATURE);
		if(wcet)
			require(dcache::CONSTRAINTS_FEATURE);
		
		// compute statistics
		t::uint64
			cnt = 0,
			ah = 0,
			am = 0,
			pe = 0,
			nc = 0;
		const CFGCollection& coll = **INVOLVED_CFGS(workspace());
		for(int i = 0; i < coll.count(); i++)
			for(CFG::BBIterator bb(coll.get(i)); bb; bb++) {
				Pair<int, dcache::BlockAccess *> tab = dcache::DATA_BLOCKS(bb);
				for(int i = 0; i < tab.fst; i++) {
					switch(dcache::CATEGORY(tab.snd[i])) {
					case ALWAYS_HIT:		ah++; break;
					case ALWAYS_MISS:		am++; break;
					case FIRST_MISS:		pe++; break;
					case NOT_CLASSIFIED:	nc++; break;
					default:				ASSERT(false); break;
					}
					cnt++;
				}
			}
		cout
			<< io::width(8, cnt).right()
			<< io::width(8, ah).right()
			<< io::width(8, am).right()
			<< io::width(8, pe).right()
			<< io::width(8, nc).right()
			<< ' ' << workspace()->process()->program()->name()
			<< io::endl;
	}

	void performPIDCacheAnalysis(void) {
		
		// perform analysis
		require(pidcache::ANALYSIS_FEATURE);

		// compute statistics
		t::uint64
			cnt = 0,
			ah = 0,
			am = 0,
			pe = 0,
			nc = 0;
		const CFGCollection& coll = **INVOLVED_CFGS(workspace());
		for(int i = 0; i < coll.count(); i++)
			for(CFG::BBIterator bb(coll.get(i)); bb; bb++) {
				const Bag<pidcache::PolyAccess> &accs = pidcache::ACCESSES(bb);
				for(int i = 0; i < accs.count(); i++) {
					struct pidcache::stat_t s = pidcache::STAT(accs[i]);
					cnt += s.total();
					ah += s.ah;
					am += s.am;
					pe += s.pe;
					nc += s.nc + s.mm;
				}
			}
		cout
			<< io::width(8, cnt).right()
			<< io::width(8, ah).right()
			<< io::width(8, am).right()
			<< io::width(8, pe).right()
			<< io::width(8, nc).right()
			<< ' ' << workspace()->process()->program()->name()
			<< io::endl;
	}

	void computeWCET(void) {
		// TODO add etime feature to the feature list
		
		// compute block time
		require(etime::EDGE_TIME_FEATURE);
		
		// compute WCET
		require(ipet::WCET_FEATURE);
		
		// display result
		if(!quiet)
			cout << "WCET = ";
		cout << ipet::WCET(workspace()) << io::endl;
	}

	void work(const string& task, PropList& props) throw(elm::Exception) {
		require(VIRTUALIZED_CFG_FEATURE);
		CACHE_CONFIG_PATH(props) = "cache.xml";
		PROCESSOR_PATH(props) = "pipeline.xml";

		if(!quiet)
			cout
				<< "   Total"
				<< "      AH"
				<< "      AM"
				<< "      PE"
				<< "      NC"
				<< " Benchmark"
				<< io::endl;
		
		if(icache)
			performICacheAnalysis();
		
		if(dcache)
			performDCacheAnalysis();
			
		if(pcache)
			performPIDCacheAnalysis();
		
		if(wcet)
			computeWCET();
	}

private:
	option::SwitchOption quiet;
	option::SwitchOption icache;
	option::SwitchOption dcache;
	option::SwitchOption pcache;
	option::SwitchOption wcet;
};

OTAWA_RUN(CEE);
