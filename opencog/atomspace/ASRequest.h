#ifndef _OPENCOG_ATOMSPACE_REQUEST_H
#define _OPENCOG_ATOMSPACE_REQUEST_H

#include <iostream>
#include <pthread.h>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

#include <opencog/util/foreach.h>

#include "AtomSpaceImpl.h"
#include "Handle.h"
#include "types.h"

using namespace std;

namespace opencog {

class ASRequest {

protected:
	//! Overkill, but doing atomic operations on bool is not technically threadsafe
    mutable boost::mutex complete_mutex;
    bool completed;

    //! For signalling that the request has been completed
    boost::condition_variable complete_cond;
    //! For blocking while fulfilling the request
    mutable boost::mutex the_mutex;

    AtomSpaceImpl* atomspace;
public:
    ASRequest() : completed(false) {};

    void set_atomspace(AtomSpaceImpl* as) {
        boost::mutex::scoped_lock lock(the_mutex);
        atomspace = as;
    }
    //! We wrap the actual do_work thread so that it doesn't have
    //! to worry about obtaining the lock or notifying of completion
    void run() {
        boost::mutex::scoped_lock lock(the_mutex);
        do_work();
        boost::mutex::scoped_lock lock2(complete_mutex);
        completed = true;
        complete_cond.notify_all();
    }
    virtual void do_work() = 0;

    bool is_complete() {
        // Rely on separate mutex for complete, since we don't want to stall
        // if the do_work method takes a while.
        boost::mutex::scoped_lock lock(complete_mutex);
        return completed;
    }
};

class AtomSpace;

/**
 * Request that stores and returns type T
 */
template<typename T>
class GenericASR: public ASRequest {

    friend class AtomSpace;
protected:
    T result;
    void set_result(T _result) { result = _result; }

public:
    GenericASR(AtomSpaceImpl* a){ set_atomspace(a); };

    virtual void do_work() = 0;

    T get_result() {
        boost::mutex::scoped_lock lock(the_mutex);
        if (!is_complete()) complete_cond.wait(lock);
        return result;
    }

};

/**
 * These should be replaced by variadic templates, or we should just replace
 * arguments with protocol buffers.
 */
template<typename T, typename A>
class OneParamASR: public GenericASR<T> {
protected:
    A p1;
public:
    OneParamASR(AtomSpaceImpl *a, const A& param1) : GenericASR<T>(a) {
        p1 = param1;
    }
};

template<typename T, typename A, typename B>
class TwoParamASR: public GenericASR<T> {
protected:
    A p1;
    B p2;
public:
    TwoParamASR(AtomSpaceImpl *a, const A& param1, const B& param2) : GenericASR<T>(a) {
        p1 = param1;
        p2 = param2;
    }
};

template<typename T, typename A, typename B, typename C>
class ThreeParamASR: public GenericASR<T> {
protected:
    A p1;
    B p2;
    C p3;
public:
    ThreeParamASR(AtomSpaceImpl *a, const A& param1, const B& param2, const C& param3) : GenericASR<T>(a) {
        p1 = param1;
        p2 = param2;
        p3 = param3;
    }
};

class AddNodeASR : public ThreeParamASR <Handle,Type,std::string,const TruthValue*> {
public:
    AddNodeASR(AtomSpaceImpl *a, Type type, const std::string& name, const TruthValue* tvn) :
        ThreeParamASR<Handle,Type,std::string,const TruthValue*>(a,type,name,tvn)
        { return; };
    ~AddNodeASR() {
        // clean up TruthValue as we are responsible for any passed parameters.
        if (!p3->isNullTv())
            delete p3;
    }
    
    virtual void do_work() {
        Handle r;
        try {
            r = atomspace->addNode(p1, p2, *p3);
            set_result(r);
        } catch (InvalidParamException &e) {
            logger().error(e.what());
            set_result(r);
        }
    };
    
};

class AddLinkASR : public ThreeParamASR <Handle,Type,HandleSeq,const TruthValue*> {
public:
    AddLinkASR(AtomSpaceImpl *a, Type type, const HandleSeq& outgoing, const TruthValue* tvn) :
        ThreeParamASR<Handle,Type,HandleSeq,const TruthValue*>(a,type,outgoing,tvn)
        {};
    ~AddLinkASR() {
        // clean up TruthValue as we are responsible for any passed parameters.
        if (!p3->isNullTv())
            delete p3;
    }
    
    virtual void do_work() {
        Handle r;
        try {
            r = atomspace->addLink(p1, p2, *p3);
        } catch (InvalidParamException &e) {
            logger().error(e.what());
        }
        set_result(r);
    };
    
};

class GetNodeHandleASR : public TwoParamASR <Handle,Type,std::string> {
public:
    GetNodeHandleASR(AtomSpaceImpl *a, Type type, const std::string& name) :
        TwoParamASR<Handle,Type,std::string>(a,type,name)
        {};
    
    virtual void do_work() {
        set_result(atomspace->getHandle(p1,p2));
    };
    
};

class GetLinkHandleASR : public TwoParamASR <Handle,Type,HandleSeq> {
public:
    GetLinkHandleASR(AtomSpaceImpl *a, Type type, const HandleSeq& outgoing) :
        TwoParamASR<Handle,Type,HandleSeq>(a,type,outgoing)
        {};
    
    virtual void do_work() {
        set_result(atomspace->getHandle(p1, p2));
    };
    
};

// store and fetching atoms is low level stuff that should be handled by the
// AtomSpace internally
class StoreAtomASR : public OneParamASR <Handle, Handle> {
public:
    StoreAtomASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<Handle, Handle>(a,h)
        {};
    
    virtual void do_work() {
        atomspace->storeAtom(p1);
        set_result(p1);
    };
    
};

class FetchAtomASR : public OneParamASR <Handle, Handle> {
public:
    FetchAtomASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<Handle, Handle>(a,h)
        {};
    
    virtual void do_work() {
        set_result(atomspace->fetchAtom(p1));
    };
    
};

class FetchIncomingSetASR : public TwoParamASR <Handle, Handle, bool> {
public:
    FetchIncomingSetASR(AtomSpaceImpl *a, Handle h, bool recurse) :
        TwoParamASR<Handle, Handle, bool>(a,h,recurse)
        {};
    
    virtual void do_work() {
        set_result(atomspace->fetchIncomingSet(p1,p2));
    };
    
};

class GetSizeASR : public GenericASR<int> {
public:
    GetSizeASR(AtomSpaceImpl *a) : GenericASR<int>(a) {};
    
    virtual void do_work() {
        set_result(atomspace->getSize());
    };
    
};

class NodeCountASR : public OneParamASR<int,VersionHandle> {
public:
    NodeCountASR(AtomSpaceImpl *a,const VersionHandle &vh) :
        OneParamASR<int,VersionHandle>(a, vh) {};
    
    virtual void do_work() {
        set_result(atomspace->Nodes(p1));
    };
    
};

class LinkCountASR : public OneParamASR<int,VersionHandle> {
public:
    LinkCountASR(AtomSpaceImpl *a,const VersionHandle &vh) :
        OneParamASR<int,VersionHandle>(a,vh) {};
    
    virtual void do_work() {
        set_result(atomspace->Links(p1));
    };
    
};

class ClearASR : public GenericASR<bool> {
public:
    ClearASR(AtomSpaceImpl *a) : GenericASR<bool>(a) {};
    
    virtual void do_work() {
        atomspace->clear();
        set_result(true);
    };
    
};

class PrintASR : public TwoParamASR<bool,Type,bool> {
    std::ostream* _output;
public:
    PrintASR(AtomSpaceImpl *a,std::ostream& o,Type t,bool subclass) :
        TwoParamASR<bool,Type,bool>(a,t,subclass) 
    {
        _output=&o;
    };
    
    virtual void do_work() {
        std::ostream& output(*_output);
        atomspace->print(output,p1,p2);
        set_result(true);
    };
    
};

class RemoveAtomASR : public TwoParamASR<bool,Handle,bool> {
public:
    RemoveAtomASR(AtomSpaceImpl *a,Handle h,bool recursive) :
        TwoParamASR<bool,Handle,bool>(a,h,recursive) { };
    
    virtual void do_work() {
        set_result(atomspace->removeAtom(p1,p2));
    };
    
};

class ValidateHandleASR : public OneParamASR <bool, Handle> {
public:
    ValidateHandleASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<bool, Handle>(a,h)
        {};
    
    virtual void do_work() {
        set_result(atomspace->isValidHandle(p1));
    };
    
};

class GetNameASR : public OneParamASR <std::string, Handle> {
public:
    GetNameASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<std::string, Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getName(p1));
    };
};

class GetAtomASR : public OneParamASR <boost::shared_ptr<Atom>, Handle> {
public:
    GetAtomASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<boost::shared_ptr<Atom>, Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->cloneAtom(p1));
    };
};

class CommitAtomASR : public GenericASR <bool> {
    boost::shared_ptr<Atom> atom;
public:
    CommitAtomASR(AtomSpaceImpl *a, const Atom& _atom) :
       GenericASR<bool>(a) {
           atom = boost::shared_ptr<Atom>((&_atom)->clone());
       };
    
    virtual void do_work() {
        set_result(atomspace->commitAtom(*atom));
    };
    
};

class AtomAsStringASR : public TwoParamASR <std::string, Handle, bool> {
public:
    AtomAsStringASR(AtomSpaceImpl *a, Handle h, bool terse) :
        TwoParamASR<std::string, Handle,bool>(a,h,terse) {};
    
    virtual void do_work() {
        set_result(atomspace->atomAsString(p1,p2));
    };
    
};

class GetOutgoingASR : public OneParamASR <HandleSeq, Handle> {
public:
    GetOutgoingASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<HandleSeq,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getOutgoing(p1));
    };
    
};

class GetOutgoingIndexASR : public TwoParamASR <Handle, Handle, int> {
public:
    GetOutgoingIndexASR(AtomSpaceImpl *a, Handle h, int idx) :
        TwoParamASR<Handle,Handle,int>(a,h,idx) {};
    
    virtual void do_work() {
        set_result(atomspace->getOutgoing(p1,p2));
    };
    
};

class GetArityASR : public OneParamASR <int, Handle> {
public:
    GetArityASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<int,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getArity(p1));
    };
    
};

class GetIncomingASR : public OneParamASR <HandleSeq, Handle> {
public:
    GetIncomingASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<HandleSeq,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getIncoming(p1));
    };
    
};

class GetTypeASR : public OneParamASR <Type, Handle> {
public:
    GetTypeASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<Type,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getType(p1));
    };
    
};

class IsSourceASR : public TwoParamASR<bool,Handle,Handle> {
public:
    IsSourceASR(AtomSpaceImpl *a,Handle h,Handle link) :
        TwoParamASR<bool,Handle,Handle>(a,h,link) { };
    
    virtual void do_work() {
        set_result(atomspace->isSource(p1,p2));
    };
    
};

class GetTruthValueMeanASR : public GenericASR <float> {
    Handle h;
    VersionHandle vh;
public:
    GetTruthValueMeanASR (AtomSpaceImpl *a, Handle _h, VersionHandle& _vh) :
        GenericASR<float> (a)  {
        h=_h; vh=_vh;
    };
    
    virtual void do_work() {
        const TruthValue& tv = atomspace->getTV(h,vh);
        set_result(tv.getMean());
    };
    
};

class GetTruthValueConfidenceASR : public GenericASR <float> {
    Handle h;
    VersionHandle vh;
public:
    GetTruthValueConfidenceASR (AtomSpaceImpl *a, Handle _h, VersionHandle& _vh) :
        GenericASR<float> (a)  {
        h=_h; vh=_vh;
    };
    
    virtual void do_work() {
        const TruthValue& tv = atomspace->getTV(h,vh);
        set_result(tv.getConfidence());
    };
    
};

class GetCompleteTruthValueASR : public GenericASR <TruthValue*> {
    Handle h;
    VersionHandle vh;
public:
    GetCompleteTruthValueASR (AtomSpaceImpl *a, Handle _h, VersionHandle& _vh) :
        GenericASR<TruthValue*> (a)  {
        h=_h; vh=_vh;
        result = NULL;
    };
    ~GetCompleteTruthValueASR() {
        if (result) delete result;
    }
    
    virtual void do_work() {
        set_result(atomspace->getTV(h,vh).clone());
    };
    
};

class SetTruthValueASR : public GenericASR <bool> {
    Handle h;
    TruthValue* tv;
    VersionHandle vh;
public:
    SetTruthValueASR(AtomSpaceImpl *a, Handle _h, const TruthValue& _tv, const VersionHandle& _vh) :
            GenericASR<bool>(a) {
        tv = NULL;
        h = _h;
        tv = _tv.clone();
        vh = _vh;
    }
    ~SetTruthValueASR() {
        if (tv) delete tv;
    }
    
    virtual void do_work() {
        atomspace->setTV(h,*tv,vh);
        set_result(true);
    };
    
};

class SetTruthValueMeanASR : public TwoParamASR <bool,Handle,float> {
    Handle h;
public:
    SetTruthValueMeanASR(AtomSpaceImpl *a, Handle _h, float mean) :
            TwoParamASR<bool,Handle,float>(a,_h,mean) {
    }
    
    virtual void do_work() {
        atomspace->setMean(p1,p2);
        set_result(true);
    };
    
};

class DecaySTIASR : public GenericASR<bool> {
public:
    DecaySTIASR(AtomSpaceImpl *a) :
        GenericASR<bool>(a) { };
    
    virtual void do_work() {
        atomspace->decayShortTermImportance();
        set_result(true);
    };

};

class GetAttentionValueASR : public OneParamASR <AttentionValue, Handle> {
public:
    GetAttentionValueASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<AttentionValue,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getAV(p1));
    };
    
};

class SetAttentionValueASR : public TwoParamASR <bool, Handle, AttentionValue> {
public:
    SetAttentionValueASR(AtomSpaceImpl *a, Handle h, const AttentionValue& av) :
        TwoParamASR<bool,Handle,AttentionValue>(a,h,av) {};
    
    virtual void do_work() {
        atomspace->setAV(p1,p2);
        set_result(true);
    };
    
};

class GetAttentionValueSTIASR : public OneParamASR <AttentionValue::sti_t, Handle> {
public:
    GetAttentionValueSTIASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<AttentionValue::sti_t,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getSTI(p1));
    };
    
};

class GetNormalisedAttentionValueSTIASR : public GenericASR<float> {
    Handle h;
    bool average, clip, positive;
public:
    GetNormalisedAttentionValueSTIASR(AtomSpaceImpl *a, Handle _h, bool _average=true,
            bool _clip=false, bool _positive=false) :
        GenericASR<float>(a), h(_h), average(_average), clip(_clip), positive(_positive) {};
    
    virtual void do_work() {
        if (positive)
            set_result(atomspace->getNormalisedZeroToOneSTI(h,average,clip));
        else
            set_result(atomspace->getNormalisedSTI(h,average,clip));
    };
    
};

class SetAttentionValueSTIASR : public TwoParamASR <bool, Handle, AttentionValue::sti_t> {
public:
    SetAttentionValueSTIASR(AtomSpaceImpl *a, Handle h, AttentionValue::sti_t sti) :
        TwoParamASR<bool,Handle,AttentionValue::sti_t>(a,h,sti) {};
    
    virtual void do_work() {
        atomspace->setSTI(p1,p2);
        set_result(true);
    };
    
};

class GetAttentionValueLTIASR : public OneParamASR <AttentionValue::lti_t, Handle> {
public:
    GetAttentionValueLTIASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<AttentionValue::lti_t,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getLTI(p1));
    };
    
};

class SetAttentionValueLTIASR : public TwoParamASR <bool, Handle, AttentionValue::lti_t> {
public:
    SetAttentionValueLTIASR(AtomSpaceImpl *a, Handle h, AttentionValue::lti_t lti) :
        TwoParamASR<bool,Handle,AttentionValue::lti_t>(a,h,lti) {};
    
    virtual void do_work() {
        atomspace->setLTI(p1,p2);
        set_result(true);
    };
    
};

class GetAttentionValueVLTIASR : public OneParamASR <AttentionValue::vlti_t, Handle> {
public:
    GetAttentionValueVLTIASR(AtomSpaceImpl *a, Handle h) :
        OneParamASR<AttentionValue::vlti_t,Handle>(a,h) {};
    
    virtual void do_work() {
        set_result(atomspace->getVLTI(p1));
    };
    
};

class IncAttentionValueVLTIASR : public OneParamASR <bool, Handle> {
public:
    IncAttentionValueVLTIASR(AtomSpaceImpl *a, Handle h) :
    OneParamASR<bool,Handle>(a,h) {}
    
    virtual void do_work() {
        atomspace->incVLTI(p1);
        set_result(true);
    }
    
};

class DecAttentionValueVLTIASR : public OneParamASR <bool, Handle> {
public:
    DecAttentionValueVLTIASR(AtomSpaceImpl *a, Handle h) :
    OneParamASR<bool,Handle>(a,h) {}
    
    virtual void do_work() {
        atomspace->decVLTI(p1);
        set_result(true);
    }
    
};

class SaveToXMLASR : public OneParamASR<bool,std::string> {
public:
    SaveToXMLASR(AtomSpaceImpl *a, const std::string& filename) :
        OneParamASR<bool,std::string>(a,filename) {}
    
    virtual void do_work() {
        set_result(atomspace->saveToXML(p1));
    }
    
};

class GetAtomHashASR : public OneParamASR<size_t,Handle> {
public:
    GetAtomHashASR (AtomSpaceImpl *a, const Handle& h) :
        OneParamASR<size_t,Handle>(a,h) {}
    
    virtual void do_work() {
        set_result(atomspace->getAtomHash(p1));
    }
};


// -----------------
// Search requests

class FilterASR : public GenericASR<HandleSeq> {
    AtomPredicate* p;
    Type t;
    bool subclass;
    VersionHandle vh;
public:
    FilterASR(AtomSpaceImpl *a, AtomPredicate *_p, Type _t = ATOM, bool _subclass=true,
            VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a), p(_p), t(_t), subclass(_subclass), vh(_vh) { }
    
    virtual void do_work() {
        HandleSeq _result;
        HandleSeq hs;
        atomspace->getHandleSet(back_inserter(hs), t, subclass, vh);
        foreach (Handle h, hs) {
            if ((*p)(atomspace->getAtom(h)) && atomspace->containsVersionedTV(h, vh))
                _result.push_back(h);
        }
        set_result(_result);
    }
    
};

class GetNeighborsASR : public GenericASR<HandleSeq> {
    Handle h;
    Type linkType;
    bool subclasses;
    bool fanin;
    bool fanout;

public:
    GetNeighborsASR(AtomSpaceImpl* a,
            const Handle& _h, bool _fanin, bool _fanout,
            Type _linkType, bool _subclasses) :
        GenericASR<HandleSeq>(a), h(_h), linkType(_linkType),
        subclasses(_subclasses), fanin(_fanin), fanout(_fanout)
            { }

    ~GetNeighborsASR() { }

    virtual void do_work() {
        set_result(atomspace->getNeighbors(h,fanin,fanout,linkType,subclasses));
    }
};

class GetHandlesByOutgoingSetASR : public GenericASR<HandleSeq> {
    HandleSeq handles;
    Type* types;
    bool* subclasses;
    Arity arity;
    Type type;
    bool subclass;
    VersionHandle vh;

public:
    GetHandlesByOutgoingSetASR(AtomSpaceImpl* a,
            const HandleSeq& _handles, Type* _types, bool* _subclasses,
             Arity _arity, Type _type, bool _subclass,
             VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        types = NULL; subclasses = NULL;
        handles = _handles;
        // This is nasty - having to malloc...
        // but we can't rely on the parameters being around when the
        // request is actioned.
        arity = _arity;
        if (_types) {
            types = (Type*) malloc(sizeof(Type) * arity);
            memcpy(types, _types, sizeof(Type) * arity);
        }
        if (_subclasses) {
            subclasses = (bool*) malloc(sizeof(bool) * arity);
            memcpy(subclasses, _subclasses,sizeof(bool) * arity);
        }
        type = _type;
        subclass = _subclass;
        vh = _vh;
    }
    ~GetHandlesByOutgoingSetASR() {
        if (types) delete types;
        if (subclasses) delete subclasses;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),handles,types,subclasses,
                arity,type,subclass,vh);
    }
};

class GetHandlesByNameASR: public GenericASR<HandleSeq> {
    std::string name;
    Type type;
    bool subclass;
    VersionHandle vh;

public:
    GetHandlesByNameASR(AtomSpaceImpl* a,
            const std::string& _name, Type _type, bool _subclass,
             VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        name = _name;
        type = _type;
        subclass = _subclass;
        vh = _vh;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),type,name,subclass,vh);
    }
};

class GetHandlesByTypeASR: public GenericASR<HandleSeq> {
    Type type;
    bool subclass;
    VersionHandle vh;

public:
    GetHandlesByTypeASR(AtomSpaceImpl* a,
            Type _type, bool _subclass,
            VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        type = _type;
        subclass = _subclass;
        vh = _vh;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),type,subclass,vh);
    }
};

class GetHandlesByTargetASR: public GenericASR<HandleSeq> {
    Type type, targetType;
    bool subclass, targetSubclass;
    VersionHandle vh, targetVh;

public:
    GetHandlesByTargetASR(AtomSpaceImpl* a,
            Type _type, Type _targetType,
            bool _subclass, bool _targetSubclass,
            VersionHandle _vh = NULL_VERSION_HANDLE,
            VersionHandle _targetVh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        type = _type;
        subclass = _subclass;
        vh = _vh;
        targetType = _targetType;
        targetSubclass = _targetSubclass;
        targetVh = _targetVh;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),type,targetType,
                subclass,targetSubclass,vh,targetVh);
    }
};

class GetHandlesByTargetHandleASR: public GenericASR<HandleSeq> {
    Handle h;
    Type type;
    bool subclass;
    VersionHandle vh;

public:
    GetHandlesByTargetHandleASR(AtomSpaceImpl* a,
            Handle _h,
            Type _type,
            bool _subclass,
            VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        h = _h;
        type = _type;
        subclass = _subclass;
        vh = _vh;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),h,type,
                subclass,vh);
    }
};

class GetHandlesByTargetTypesASR: public GenericASR<HandleSeq> {
    Type* types;
    bool* subclasses;
    Arity arity;
    Type type;
    bool subclass;
    VersionHandle vh;

public:
    GetHandlesByTargetTypesASR(AtomSpaceImpl* a,
             Type* _types, bool* _subclasses,
             Arity _arity, Type _type, bool _subclass,
             VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        // This is nasty - having to malloc...
        // but we can't rely on the parameters being around when the
        // request is actioned.
        arity = _arity;
        types = NULL; subclasses = NULL;
        if (_types) {
            types = (Type*) malloc(sizeof(Type) * arity);
            memcpy(types, _types, sizeof(Type) * arity);
        }
        if (_subclasses) {
            subclasses = (bool*) malloc(sizeof(bool) * arity);
            memcpy(subclasses, _subclasses,sizeof(bool) * arity);
        }
        type = _type;
        subclass = _subclass;
        vh = _vh;
    }
    ~GetHandlesByTargetTypesASR() {
        if (types) delete types;
        if (subclasses) delete subclasses;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),types,subclasses,
               arity,type,subclass,vh);
    }
};

class GetHandlesByTargetNamesASR: public GenericASR<HandleSeq> {
    char** names;
    Type* types;
    bool* subclasses;
    Arity arity;
    Type type;
    bool subclass;
    VersionHandle vh;

public:
    GetHandlesByTargetNamesASR(AtomSpaceImpl* a,
            const char** _names, Type* _types, bool* _subclasses,
             Arity _arity, Type _type, bool _subclass,
             VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        // This is nasty - having to malloc...
        // but we can't rely on the parameters being around when the
        // request is actioned.
        arity = _arity;
        names = NULL; types = NULL; subclasses = NULL;
        if (_names) {
            names = (char**) malloc(sizeof(char*) * arity);
            for (int i=0; i < arity; i++) {
                names[i] = NULL;
                if (_names[i]) names[i] = (char*) malloc(sizeof(char) * strlen(_names[i]));
            }
        }
        if (_types) {
            types = (Type*) malloc(sizeof(Type) * arity);
            memcpy(types, _types, sizeof(Type) * arity);
        }
        if (_subclasses) {
            subclasses = (bool*) malloc(sizeof(bool) * arity);
            memcpy(subclasses, _subclasses,sizeof(bool) * arity);
        }
        type = _type;
        subclass = _subclass;
        vh = _vh;
    }
    ~GetHandlesByTargetNamesASR() {
        if (types) delete types;
        if (subclasses) delete subclasses;
        if (names) {
            for (int i=0; i < arity; i++) {
                if (names[i]) delete names[i];
            }
            delete names;
        }
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),
               (const char**)names,types,subclasses,
               arity,type,subclass,vh);
    }
};

class GetHandlesByTargetNameASR: public GenericASR<HandleSeq> {
    std::string targetName;
    Type targetType;
    Type type;
    bool subclass;
    VersionHandle vh;
    VersionHandle targetVh;

public:
    GetHandlesByTargetNameASR(AtomSpaceImpl* a,
            const char* _targetName,
            Type _targetType,
            Type _type,
            bool _subclass,
            VersionHandle _vh = NULL_VERSION_HANDLE,
            VersionHandle _targetVh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        targetName = _targetName;
        targetType = _targetType;
        type = _type;
        subclass = _subclass;
        vh = _vh;
        targetVh = _targetVh;
    }

    virtual void do_work() {
        atomspace->getHandleSet(back_inserter(result),targetName.c_str(),
                targetType,type,subclass,vh,targetVh);
    }
};

class GetSortedHandleSetASR: public GenericASR<HandleSeq> {
    Type type;
    bool subclass;
    VersionHandle vh;
    AtomComparator* compare;

public:
    GetSortedHandleSetASR(AtomSpaceImpl* a,
            Type _type, bool _subclass,
            AtomComparator* _compare,
            VersionHandle _vh = NULL_VERSION_HANDLE) :
            GenericASR<HandleSeq>(a) {
        type = _type;
        subclass = _subclass;
        vh = _vh;
        compare = _compare;
    }

    virtual void do_work() {
        atomspace->getSortedHandleSet(back_inserter(result),type,subclass,compare,vh);
    }
};

// Requests are based on their parent class that defines the return type
typedef boost::shared_ptr< GenericASR<Handle> > HandleRequest;
typedef boost::shared_ptr< GenericASR<boost::shared_ptr<Atom> > > AtomRequest;
typedef boost::shared_ptr< GenericASR<AttentionValue> > AttentionValueRequest;
typedef boost::shared_ptr< GenericASR<AttentionValue::sti_t> > STIRequest;
typedef boost::shared_ptr< GenericASR<AttentionValue::lti_t> > LTIRequest;
typedef boost::shared_ptr< GenericASR<AttentionValue::vlti_t> > VLTIRequest;
typedef boost::shared_ptr< GenericASR<tv_summary_t> > TruthValueRequest;
typedef boost::shared_ptr< GenericASR<TruthValue*> > TruthValueCompleteRequest;
typedef boost::shared_ptr< GenericASR<HandleSeq> > HandleSeqRequest;
typedef boost::shared_ptr< GenericASR<Type> > TypeRequest;
typedef boost::shared_ptr< GenericASR<int> > IntRequest;
typedef boost::shared_ptr< GenericASR<float> > FloatRequest;
typedef boost::shared_ptr< GenericASR<bool> > BoolRequest;
typedef boost::shared_ptr< GenericASR<size_t> > HashRequest;
typedef boost::shared_ptr< GenericASR<std::string> > StringRequest;
// Can't actually init template with void, so use bool as stand-in.
typedef boost::shared_ptr< GenericASR<bool> > VoidRequest;

} // namespace opencog

#endif // _OPENCOG_ATOMSPACE_REQUEST_H
