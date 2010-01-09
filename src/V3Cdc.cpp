//*************************************************************************
// DESCRIPTION: Verilator: Clock Domain Crossing Lint
//
// Code available from: http://www.veripool.org/verilator
//
// AUTHORS: Wilson Snyder with Paul Wasson, Duane Gabli
//
//*************************************************************************
//
// Copyright 2003-2010 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************
// V3Cdc's Transformations:
//
// Create V3Graph-ish graph
// Find all negedge reset flops
// Trace back to previous flop
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <deque>
#include <list>
#include <memory>

#include "V3Global.h"
#include "V3Cdc.h"
#include "V3Ast.h"
#include "V3Graph.h"
#include "V3Const.h"
#include "V3EmitV.h"
#include "V3File.h"

//######################################################################

class CdcBaseVisitor : public AstNVisitor {
public:
    static int debug() {
	static int level = -1;
	if (VL_UNLIKELY(level < 0)) level = v3Global.opt.debugSrcLevel(__FILE__);
	return level;
    }
};

//######################################################################
// Graph support classes

class CdcEitherVertex : public V3GraphVertex {
    AstScope*	m_scopep;
    AstNode*	m_nodep;
    AstSenTree*	m_srcDomainp;
    AstSenTree*	m_dstDomainp;
    bool	m_srcDomainSet;
    bool	m_dstDomainSet;
    bool	m_asyncPath;
public:
    CdcEitherVertex(V3Graph* graphp, AstScope* scopep, AstNode* nodep)
	: V3GraphVertex(graphp), m_scopep(scopep), m_nodep(nodep)
	, m_srcDomainp(NULL), m_dstDomainp(NULL)
	, m_srcDomainSet(false), m_dstDomainSet(false)
	, m_asyncPath(false) {}
    virtual ~CdcEitherVertex() {}
    // Accessors
    AstScope* scopep() const { return m_scopep; }
    AstNode* nodep() const { return m_nodep; }
    AstSenTree* srcDomainp() const { return m_srcDomainp; }
    void srcDomainp(AstSenTree* nodep) { m_srcDomainp = nodep; }
    bool srcDomainSet() const { return m_srcDomainSet; }
    void srcDomainSet(bool flag) { m_srcDomainSet = flag; }
    AstSenTree* dstDomainp() const { return m_dstDomainp; }
    void dstDomainp(AstSenTree* nodep) { m_dstDomainp = nodep; }
    bool dstDomainSet() const { return m_dstDomainSet; }
    void dstDomainSet(bool flag) { m_dstDomainSet = flag; }
    bool asyncPath() const { return m_asyncPath; }
    void asyncPath(bool flag) { m_asyncPath = flag; }
};

class CdcVarVertex : public CdcEitherVertex {
    AstVarScope* m_varScp;
    int		m_cntAsyncRst;
    bool	m_fromFlop;
public:
    CdcVarVertex(V3Graph* graphp, AstScope* scopep, AstVarScope* varScp)
	: CdcEitherVertex(graphp, scopep, varScp), m_varScp(varScp), m_cntAsyncRst(0), m_fromFlop(false) {}
    virtual ~CdcVarVertex() {}
    // Accessors
    AstVarScope* varScp() const { return m_varScp; }
    virtual string name() const { return (cvtToStr((void*)m_varScp)+" "+varScp()->name()); }
    virtual string dotColor() const { return fromFlop() ? "green" : cntAsyncRst() ? "red" : "blue"; }
    int cntAsyncRst() const { return m_cntAsyncRst; }
    void cntAsyncRst(int flag) { m_cntAsyncRst=flag; }
    bool fromFlop() const { return m_fromFlop; }
    void fromFlop(bool flag) { m_fromFlop = flag; }
};

class CdcLogicVertex : public CdcEitherVertex {
    bool	m_safe;
public:
    CdcLogicVertex(V3Graph* graphp, AstScope* scopep, AstNode* nodep, AstSenTree* sensenodep)
	: CdcEitherVertex(graphp,scopep,nodep), m_safe(true) { srcDomainp(sensenodep); dstDomainp(sensenodep); }
    virtual ~CdcLogicVertex() {}
    // Accessors
    virtual string name() const { return (cvtToStr((void*)nodep())+"@"+scopep()->prettyName()); }
    virtual string dotColor() const { return safe() ? "black" : "yellow"; }
    bool safe() const { return m_safe; }
    void clearSafe(AstNode* nodep) { m_safe = false; nodep->user3(true); }
};

//######################################################################

class CdcDumpVisitor : public CdcBaseVisitor {
private:
    // NODE STATE
    //Entire netlist:
    // {statement}Node::user3p	-> bool, indicating not safe
    ofstream*		m_ofp;		// Output file
    string		m_prefix;

    virtual void visit(AstNode* nodep, AstNUser*) {
	*m_ofp<<m_prefix;
	if (nodep->user3()) *m_ofp<<" %%";
	else *m_ofp<<"  ";
	*m_ofp<<nodep->prettyTypeName()<<" "<<endl;
	string lastPrefix = m_prefix;
	m_prefix = lastPrefix + "1:";
	nodep->op1p()->iterateAndNext(*this);
	m_prefix = lastPrefix + "2:";
	nodep->op2p()->iterateAndNext(*this);
	m_prefix = lastPrefix + "3:";
	nodep->op3p()->iterateAndNext(*this);
	m_prefix = lastPrefix + "4:";
	nodep->op4p()->iterateAndNext(*this);
	m_prefix = lastPrefix;
    }

public:
    // CONSTUCTORS
    CdcDumpVisitor(AstNode* nodep, ofstream* ofp, const string& prefix) {
	m_ofp = ofp;
	m_prefix = prefix;
	nodep->accept(*this);
    }
    virtual ~CdcDumpVisitor() {}
};

//######################################################################
// Cdc class functions

class CdcVisitor : public CdcBaseVisitor {
private:
    // NODE STATE
    //Entire netlist:
    // AstVarScope::user1p	-> CdcVarVertex* for usage var, 0=not set yet
    // {statement}Node::user1p	-> CdcLogicVertex* for this statement
    // AstNode::user3		-> bool  True indicates to print %% (via V3EmitV)
    AstUser1InUse	m_inuser1;
    AstUser3InUse	m_inuser3;

    // STATE
    V3Graph		m_graph;	// Scoreboard of var usages/dependencies
    CdcLogicVertex*	m_logicVertexp;	// Current statement being tracked, NULL=ignored
    AstScope*		m_scopep;	// Current scope being processed
    AstNodeModule*	m_modp;		// Current module
    AstSenTree*		m_domainp;	// Current sentree
    bool		m_inDly;	// In delayed assign
    int			m_senNumber;	// Number of senitems
    string		m_ofFilename;	// Output filename
    ofstream*		m_ofp;		// Output file
    uint32_t		m_userGeneration; // Generation count to avoid slow userClearVertices

    // METHODS
    void iterateNewStmt(AstNode* nodep) {
	if (m_scopep) {
	    UINFO(4,"   STMT "<<nodep<<endl);
	    m_logicVertexp = new CdcLogicVertex(&m_graph, m_scopep, nodep, m_domainp);
	    if (m_domainp && m_domainp->hasClocked()) {  // To/from a flop
		m_logicVertexp->srcDomainp(m_domainp);
		m_logicVertexp->srcDomainSet(true);
		m_logicVertexp->dstDomainp(m_domainp);
		m_logicVertexp->dstDomainSet(true);
	    }
	    m_senNumber = 0;
	    nodep->iterateChildren(*this);
	    m_logicVertexp = NULL;

	    if (0 && debug()>=9) {
		UINFO(9, "Trace Logic:\n");
		nodep->dumpTree(cout, "-log1: ");
	    }
	}
    }

    CdcVarVertex* makeVarVertex(AstVarScope* varscp) {
	CdcVarVertex* vertexp = (CdcVarVertex*)(varscp->user1p());
	if (!vertexp) {
	    UINFO(6,"New vertex "<<varscp<<endl);
	    vertexp = new CdcVarVertex(&m_graph, m_scopep, varscp);
	    varscp->user1p(vertexp);
	    if (varscp->varp()->isIO() && varscp->scopep()->isTop()) {}
	    if (varscp->varp()->isUsedClock()) {}
	}
	if (m_senNumber > 1) vertexp->cntAsyncRst(vertexp->cntAsyncRst()+1);
	return vertexp;
    }

    void warnAndFile(AstNode* nodep, V3ErrorCode code, const string& msg) {
	static bool told_file = false;
	nodep->v3warnCode(code,msg);
	if (!told_file) {
	    told_file = 1;
	    cerr<<V3Error::msgPrefix()<<"     See details in "<<m_ofFilename<<endl;
	}
	*m_ofp<<"%Warning-"<<code.ascii()<<": "<<nodep->fileline()<<" "<<msg<<endl;
    }

    void clearNodeSafe(AstNode* nodep) {
	// Need to not clear if warnings are off (rather than when report it)
	// as bypassing this warning may turn up another path that isn't warning off'ed.
	if (m_logicVertexp && !nodep->fileline()->warnIsOff(V3ErrorCode::CDCRSTLOGIC)) {
	    UINFO(9,"Clear safe "<<nodep<<endl);
	    m_logicVertexp->clearSafe(nodep);
	}
    }

    string spaces(int level) { string out; while (level--) out+=" "; return out; }

    string pad(unsigned column, const string& in) {
	string out = in;
	while (out.length()<column) out += ' ';
	return out;
    }

    void analyze() {
	UINFO(3,__FUNCTION__<<": "<<endl);
	//if (debug()>6) m_graph.dump();
	if (debug()>6) m_graph.dumpDotFilePrefixed("cdc_pre");
	m_graph.removeRedundantEdgesSum(&V3GraphEdge::followAlwaysTrue);
	m_graph.dumpDotFilePrefixed("cdc_simp");
	//
	analyzeReset();
    }

    //----------------------------------------
    // RESET REPORT

    void analyzeReset() {
	// Find all async reset wires, and trace backwards
	// userClearVertices is very slow, so we use a generation count instead
	m_graph.userClearVertices();  // user1: uint32_t - was analyzed generation
	for (V3GraphVertex* itp = m_graph.verticesBeginp(); itp; itp=itp->verticesNextp()) {
	    if (CdcVarVertex* vvertexp = dynamic_cast<CdcVarVertex*>(itp)) {
		if (vvertexp->cntAsyncRst()) {
		    m_userGeneration++;  // Effectively a userClearVertices()
		    UINFO(9, "   Trace One async: "<<vvertexp<<endl);
		    // Twice, as we need to detect, then propagate
		    CdcEitherVertex* markp = traceAsyncRecurse(vvertexp, false);
		    if (markp) { // Mark is non-NULL if something bad on this path
			UINFO(9, "   Trace One bad! "<<vvertexp<<endl);
			m_userGeneration++;  // Effectively a userClearVertices()
			traceAsyncRecurse(vvertexp, true);
			m_userGeneration++;  // Effectively a userClearVertices()
			dumpAsync(vvertexp, markp);
		    }
		}
	    }
	}
    }

    CdcEitherVertex* traceAsyncRecurse(CdcEitherVertex* vertexp, bool mark) {
	// Return vertex of any dangerous stuff attached, or NULL if OK
	// If mark, also mark the output even if nothing dangerous below
	CdcEitherVertex* mark_outp = NULL;
	UINFO(9,"      Trace: "<<vertexp<<endl);

	if (vertexp->user()>=m_userGeneration) return false;   // Processed - prevent loop
	vertexp->user(m_userGeneration);

	if (CdcLogicVertex* vvertexp = dynamic_cast<CdcLogicVertex*>(vertexp)) {
	    // Any logic considered bad, at the moment, anyhow
	    if (!vvertexp->safe() && !mark_outp) mark_outp = vvertexp;
	    // And keep tracing back so the user can understand what's up
	}
	else if (CdcVarVertex* vvertexp = dynamic_cast<CdcVarVertex*>(vertexp)) {
	    if (mark) vvertexp->asyncPath(true);
	    // If primary I/O, it's ok here back
	    if (vvertexp->varScp()->varp()->isInput()) return false;
	    // Also ok if from flop, but partially trace the flop so more obvious to users
	    if (vvertexp->fromFlop()) {
		//for (V3GraphEdge* edgep = vertexp->inBeginp(); edgep; edgep = edgep->inNextp()) {
		//    CdcEitherVertex* eFromVertexp = (CdcEitherVertex*)edgep->fromp();
		//    eFromVertexp->asyncPath(true);
		//}
		return false;
	    }
	}

	for (V3GraphEdge* edgep = vertexp->inBeginp(); edgep; edgep = edgep->inNextp()) {
	    CdcEitherVertex* eFromVertexp = (CdcEitherVertex*)edgep->fromp();
	    CdcEitherVertex* submarkp = traceAsyncRecurse(eFromVertexp, mark);
	    if (submarkp && !mark_outp) mark_outp = submarkp;
	}

	if (mark) vertexp->asyncPath(true);
	return mark_outp;
    }

    void dumpAsync(CdcVarVertex* vertexp, CdcEitherVertex* markp) {
	AstNode* nodep = vertexp->varScp();
	*m_ofp<<"\n";
	*m_ofp<<"\n";
	AstNode* targetp = vertexp->nodep();
	if (V3GraphEdge* edgep = vertexp->outBeginp()) {
	    CdcEitherVertex* eToVertexp = (CdcEitherVertex*)edgep->top();
	    targetp = eToVertexp->nodep();
	}
	warnAndFile(markp->nodep(),V3ErrorCode::CDCRSTLOGIC,"Logic in path that feeds async reset, via signal: "+nodep->prettyName());
	*m_ofp<<"Fanout: "<<vertexp->cntAsyncRst()<<"  Target: "<<targetp->fileline()<<endl;
	dumpAsyncRecurse(vertexp," +--", " |  ");
    }
    void dumpAsyncRecurse(CdcEitherVertex* vertexp, const string& prefix, const string& blank) {
	// Return true if any dangerous stuff attached
	// If mark, also mark the output even if nothing dangerous below
	if (vertexp->user()>=m_userGeneration) return;   // Processed - prevent loop
	vertexp->user(m_userGeneration);
	if (!vertexp->asyncPath()) return;  // Not part of path

	*m_ofp<<V3OutFile::indentSpaces(40)<<" "<<blank<<"\n";

	// Dump single variable/logic block
	// See also OrderGraph::loopsVertexCb(V3GraphVertex* vertexp)
	AstNode* nodep = vertexp->nodep();
	string front = pad(40,nodep->fileline()->ascii()+":");
	front += " "+prefix+" ";
	if (nodep->castVarScope()) *m_ofp<<front<<"Variable: "<<nodep->prettyName()<<endl;
	else {
	    V3EmitV::verilogPrefixedTree(nodep, *m_ofp, prefix+" ", true);
	    if (debug()) {
		CdcDumpVisitor visitor (nodep, m_ofp, front+"DBG: ");
	    }
	}

	// Now do the other logic in the path
	for (V3GraphEdge* edgep = vertexp->inBeginp(); edgep; edgep = edgep->inNextp()) {
	    CdcEitherVertex* eFromVertexp = (CdcEitherVertex*)edgep->fromp();
	    dumpAsyncRecurse(eFromVertexp, blank+" +--", blank+" |  ");
	}
    }

    //----------------------------------------
    // EDGE REPORTS

    void edgeReport() {
	// Make report of all signal names and what clock edges they have
	UINFO(3,__FUNCTION__<<": "<<endl);

	// Trace all sources and sinks
	for (int traceDests=0; traceDests<2; ++traceDests) {
	    UINFO(9, " Trace Direction "<<(traceDests?"dst":"src")<<endl);
	    m_graph.userClearVertices();  // user1: bool - was analyzed
	    for (V3GraphVertex* itp = m_graph.verticesBeginp(); itp; itp=itp->verticesNextp()) {
		if (CdcVarVertex* vvertexp = dynamic_cast<CdcVarVertex*>(itp)) {
		    UINFO(9, "   Trace One edge: "<<vvertexp<<endl);
		    edgeDomainRecurse(vvertexp, traceDests, 0);
		}
	    }
	}

	string filename = v3Global.opt.makeDir()+"/"+v3Global.opt.prefix()+"__cdc_edges.txt";
	const auto_ptr<ofstream> ofp (V3File::new_ofstream(filename));
	if (ofp->fail()) v3fatalSrc("Can't write "<<filename);
	*ofp<<"Edge Report for "<<v3Global.opt.prefix()<<endl;

	deque<string> report;  // Sort output by name
	for (V3GraphVertex* itp = m_graph.verticesBeginp(); itp; itp=itp->verticesNextp()) {
	    if (CdcVarVertex* vvertexp = dynamic_cast<CdcVarVertex*>(itp)) {
		AstVar* varp = vvertexp->varScp()->varp();
		if (1) {  // varp->isPrimaryIO()
		    const char* whatp = "wire";
		    if (varp->isPrimaryIO()) whatp = (varp->isInout()?"inout":varp->isInput()?"input":"output");

		    ostringstream os;
		    os.setf(ios::left);
		    os<<"  "<<setw(8)<<whatp;
		    os<<"  "<<setw(40)<<vvertexp->varScp()->prettyName();
		    os<<"  SRC=";
		    if (vvertexp->srcDomainp()) V3EmitV::verilogForTree(vvertexp->srcDomainp(), os);
		    os<<"  DST=";
		    if (vvertexp->dstDomainp()) V3EmitV::verilogForTree(vvertexp->dstDomainp(), os);
		    os<<"\n";
		    report.push_back(os.str());
		}
	    }
	}
	sort(report.begin(), report.end());
	for (deque<string>::iterator it = report.begin(); it!=report.end(); ++it) {
	    *ofp << *it;
	}
    }

    void edgeDomainRecurse(CdcEitherVertex* vertexp, bool traceDests, int level) {
	// Scan back to inputs/outputs, flops, and compute clock domain information
	UINFO(8,spaces(level)<<"     Tracein  "<<vertexp<<endl);
	if (vertexp->user()>=m_userGeneration) return;   // Mid-Processed - prevent loop
	vertexp->user(m_userGeneration);

	// Variables from flops already are domained
	if (traceDests ? vertexp->dstDomainSet() : vertexp->srcDomainSet()) return;  // Fully computed

	typedef set<AstSenTree*> SenSet;
	SenSet 	    senouts;   // List of all sensitivities for new signal
	if (CdcLogicVertex* vvertexp = dynamic_cast<CdcLogicVertex*>(vertexp)) {
	    if (vvertexp) {}  // Unused
	}
	else if (CdcVarVertex* vvertexp = dynamic_cast<CdcVarVertex*>(vertexp)) {
	    // If primary I/O, give it domain of the input
	    AstVar* varp = vvertexp->varScp()->varp();
	    if (varp->isPrimaryIO() && varp->isInput() && !traceDests) {
		senouts.insert(new AstSenTree(varp->fileline(), new AstSenItem(varp->fileline(), AstSenItem::Combo())));
	    }
	}

	// Now combine domains of sources/dests
	if (traceDests) {
	    for (V3GraphEdge* edgep = vertexp->outBeginp(); edgep; edgep = edgep->outNextp()) {
		CdcEitherVertex* eToVertexp = (CdcEitherVertex*)edgep->top();
		edgeDomainRecurse(eToVertexp, traceDests, level+1);
		if (eToVertexp->dstDomainp()) senouts.insert(eToVertexp->dstDomainp());
	    }
	} else {
	    for (V3GraphEdge* edgep = vertexp->inBeginp(); edgep; edgep = edgep->inNextp()) {
		CdcEitherVertex* eFromVertexp = (CdcEitherVertex*)edgep->fromp();
		edgeDomainRecurse(eFromVertexp, traceDests, level+1);
		if (eFromVertexp->srcDomainp()) senouts.insert(eFromVertexp->srcDomainp());
	    }
	}

	// Convert list of senses into one sense node
	AstSenTree* senoutp = NULL;
	bool	    senedited = false;
	for (SenSet::iterator it=senouts.begin(); it!=senouts.end(); ++it) {
	    if (!senoutp) senoutp = *it;
	    else {
		if (!senedited) {
		    senedited = true;
		    senoutp = senoutp->cloneTree(true);
		}
		senoutp->addSensesp((*it)->sensesp()->cloneTree(true));
	    }
	}
	// If multiple domains need to do complicated optimizations
	if (senedited) {
	    senoutp = V3Const::constifyExpensiveEdit(senoutp)->castSenTree();
	}
	if (traceDests) {
	    vertexp->dstDomainSet(true);  // Note it's set - domainp may be null, so can't use that
	    vertexp->dstDomainp(senoutp);
	    if (debug()>=9) {
		UINFO(9,spaces(level)+"     Tracedst "<<vertexp);
		if (senoutp) V3EmitV::verilogForTree(senoutp, cout); cout<<endl;
	    }
	} else {
	    vertexp->srcDomainSet(true);  // Note it's set - domainp may be null, so can't use that
	    vertexp->srcDomainp(senoutp);
	    if (debug()>=9) {
		UINFO(9,spaces(level)+"     Tracesrc "<<vertexp);
		if (senoutp) V3EmitV::verilogForTree(senoutp, cout); cout<<endl;
	    }
	}
    }

    // VISITORS
    virtual void visit(AstNodeModule* nodep, AstNUser*) {
	m_modp = nodep;
	nodep->iterateChildren(*this);
	m_modp = NULL;
    }
    virtual void visit(AstScope* nodep, AstNUser*) {
	UINFO(4," SCOPE "<<nodep<<endl);
	m_scopep = nodep;
	m_logicVertexp = NULL;
	nodep->iterateChildren(*this);
	m_scopep = NULL;
    }
    virtual void visit(AstActive* nodep, AstNUser*) {
	// Create required blocks and add to module
	UINFO(4,"  BLOCK  "<<nodep<<endl);
	m_domainp = nodep->sensesp();
	if (!m_domainp || m_domainp->hasCombo() || m_domainp->hasClocked()) {  // IE not hasSettle/hasInitial
	    iterateNewStmt(nodep);
	}
	m_domainp = NULL;
    }
    virtual void visit(AstNodeVarRef* nodep, AstNUser*) {
	if (m_scopep) {
	    if (!m_logicVertexp) nodep->v3fatalSrc("Var ref not under a logic block\n");
	    AstVarScope* varscp = nodep->varScopep();
	    if (!varscp) nodep->v3fatalSrc("Var didn't get varscoped in V3Scope.cpp\n");
	    CdcVarVertex* varvertexp = makeVarVertex(varscp);
	    UINFO(5," VARREF to "<<varscp<<endl);
	    // We use weight of one; if we ref the var more than once, when we simplify,
	    // the weight will increase
	    if (nodep->lvalue()) {
		new V3GraphEdge(&m_graph, m_logicVertexp, varvertexp, 1);
		if (m_inDly) {
		    varvertexp->fromFlop(true);
		    varvertexp->srcDomainp(m_domainp);
		    varvertexp->srcDomainSet(true);
		}
	    } else {
		new V3GraphEdge(&m_graph, varvertexp, m_logicVertexp, 1);
	    }
	}
    }
    virtual void visit(AstAssignDly* nodep, AstNUser*) {
	m_inDly = true;
	nodep->iterateChildren(*this);
	m_inDly = false;
    }
    virtual void visit(AstSenItem* nodep, AstNUser*) {
	// Note we look at only AstSenItems, not AstSenGate's
	// The gating term of a AstSenGate is normal logic
	m_senNumber++;
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstAlways* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstCFunc* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstSenGate* nodep, AstNUser*) {
	// First handle the clock part will be handled in a minute by visit AstSenItem
	// The logic gating term is delt with as logic
	iterateNewStmt(nodep);
    }
    virtual void visit(AstAssignAlias* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstAssignW* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }

    // Math that shouldn't cause us to clear safe
    virtual void visit(AstConst* nodep, AstNUser*) { }
    virtual void visit(AstReplicate* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstConcat* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstNot* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstSel* nodep, AstNUser*) {
	if (!nodep->lsbp()->castConst()) clearNodeSafe(nodep);
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstNodeSel* nodep, AstNUser*) {
	if (!nodep->bitp()->castConst()) clearNodeSafe(nodep);
	nodep->iterateChildren(*this);
    }

    // Ignores
    virtual void visit(AstInitial* nodep, AstNUser*) { }
    virtual void visit(AstTraceInc* nodep, AstNUser*) { }
    virtual void visit(AstCoverToggle* nodep, AstNUser*) { }

    //--------------------
    // Default
    virtual void visit(AstNodeMath* nodep, AstNUser*) {
	clearNodeSafe(nodep);
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstNode* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }

public:
    // CONSTUCTORS
    CdcVisitor(AstNode* nodep) {
	m_logicVertexp = NULL;
	m_scopep = NULL;
	m_modp = NULL;
	m_domainp = NULL;
	m_inDly = false;
	m_senNumber = 0;
	m_userGeneration = 0;

	// Make report of all signal names and what clock edges they have
	string filename = v3Global.opt.makeDir()+"/"+v3Global.opt.prefix()+"__cdc.txt";
	m_ofp = V3File::new_ofstream(filename);
	if (m_ofp->fail()) v3fatalSrc("Can't write "<<filename);
	m_ofFilename = filename;
	*m_ofp<<"CDC Report for "<<v3Global.opt.prefix()<<endl;
	*m_ofp<<"Each dump below traces a variable from a flop back through logic.\n";
	*m_ofp<<"First the variable is listed, then the logic that creates it, then all variables\n";
	*m_ofp<<"feeding that logic, recursively backwards to the sourcing flop(s).\n";
	*m_ofp<<"%% Indicates nodes considered potentially unsafe.\n";

	nodep->accept(*this);
	analyze();
	//edgeReport();  // Not useful at the moment

	if (0) { *m_ofp<<"\nDBG-test-dumper\n"; V3EmitV::verilogPrefixedTree(nodep, *m_ofp, "DBG ",true); *m_ofp<<endl; }
    }
    virtual ~CdcVisitor() {
	if (m_ofp) { delete m_ofp; m_ofp = NULL; }
    }
};

//######################################################################
// Cdc class functions

void V3Cdc::cdcAll(AstNetlist* nodep) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    CdcVisitor visitor (nodep);
}