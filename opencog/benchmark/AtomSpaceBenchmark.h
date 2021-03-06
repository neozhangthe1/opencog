#ifndef _OPENCOG_AS_BENCHMARK_H
#define _OPENCOG_AS_BENCHMARK_H

#include <opencog/atomspace/types.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/mt19937ar.h>
#include <boost/tuple/tuple.hpp>

using boost::tuple;

namespace opencog
{

typedef boost::tuple<clock_t,clock_t> timepair_t;

class AtomSpaceBenchmark
{
    // size of AtomSpace, time taken for operation, rss memory max
    typedef boost::tuple<size_t,clock_t,long> record_t;


    struct TimeStats {
        clock_t t_total;
        clock_t t_max;
        clock_t t_min;
        clock_t t_mean;
        clock_t t_std;
        long t_N;
        TimeStats(const std::vector<record_t>& records);
        void print();
    };


    void recordToFile(std::ofstream& file, const record_t record) const;

    float linkSize_mean;
    float linkSize_std;

    Type defaultLinkType;
    float chanceOfNonDefaultLink;
    Type defaultNodeType;
    float chanceOfNonDefaultNode;

    float maxSize; //! never make the atomspace bigger than this while building it

    AtomSpace* a;
    //AtomSpaceImpl asBackend;
    MT19937RandGen* rng;

    Type randomType(Type t);

    clock_t makeRandomNode(const std::string& s);
    clock_t makeRandomLink();

    long getMemUsage();
    int counter;

    std::vector<std::string>  methodNames;
public:
    int N;
    int sizeIncrease;
    bool saveToFile;
    int saveInterval;
    bool doStats;
    bool buildTestData;
    bool testBackend;
    UUID UUID_begin;
    typedef timepair_t (AtomSpaceBenchmark::*BMFn)();
    std::vector< BMFn > methodsToTest;

    float percentLinks;
    long atomCount; //! number of nodes to build atomspace with before testing

    bool showTypeSizes;
    void printTypeSizes();

    AtomSpaceBenchmark();
    ~AtomSpaceBenchmark();

    void setMethod(std::string method);
    void showMethods();
    void startBenchmark(int numThreads=1);
    void doBenchmark(const std::string& methodName, BMFn methodToCall);

    void buildAtomSpace(long atomspaceSize=(1 << 16), float percentLinks = 0.1, 
            bool display = true);
    Handle getRandomHandle();

    timepair_t bm_addNode();
    timepair_t bm_addLink();

    timepair_t bm_getType();
    timepair_t bm_getNodeHandles();
    timepair_t bm_getHandleSet();
    timepair_t bm_getOutgoingSet();

    void bm_getHandleNode() {};
    void bm_getHandleLink() {};
    void bm_getName() {};

    // Get and set TV and AV
    float chanceUseDefaultTV; // if set, this will use default TV for new atoms and bm_setTruthValue
    timepair_t bm_getTruthValue();

#ifdef ZMQ_EXPERIMENT
    timepair_t bm_getTruthValueZmq();
#endif
    timepair_t bm_setTruthValue();
    void bm_getAttentionValue() {};
    void bm_setAttentionValue() {};

    void bm_erase() {};

    size_t estimateOfAtomSize(Handle h);

};

} // namespace opencog

#endif //_OPENCOG_AS_BENCHMARK_H
