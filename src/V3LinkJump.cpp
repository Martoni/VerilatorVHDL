//*************************************************************************
// DESCRIPTION: Verilator: Replace return/continue with jumps
//
// Code available from: http://www.veripool.org/verilator
//
// AUTHORS: Wilson Snyder with Paul Wasson, Duane Gabli
//
//*************************************************************************
//
// Copyright 2003-2012 by Wilson Snyder.  This program is free software; you can
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
// V3LinkJump's Transformations:
//
// Each module:
//	Look for BEGINs
//	    BEGIN(VAR...) -> VAR ... {renamed}
//	FOR -> WHILEs
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <algorithm>
#include <vector>

#include "V3Global.h"
#include "V3LinkJump.h"
#include "V3Ast.h"

//######################################################################

class LinkJumpVisitor : public AstNVisitor {
private:
    // TYPES
    typedef vector<AstBegin*> BeginStack;
	typedef map <string, AstNode*> SenItemList;

    // STATE
    AstNodeModule*	m_modp;		// Current module
    AstNodeFTask* 	m_ftaskp;	// Current function/task
    AstWhile*	 	m_loopp;	// Current loop
    bool		m_loopInc;	// In loop increment
    int			m_repeatNum;	// Repeat counter
    BeginStack		m_beginStack;	// All begin blocks above current node
	SenItemList  m_senItemList; // List of the signals process is sensitive to (VHDL)
	bool 		m_clocked; // VHDL Process is clocked
    // METHODS
    static int debug() {
	static int level = -1;
	if (VL_UNLIKELY(level < 0)) level = v3Global.opt.debugSrcLevel(__FILE__);
	return level;
    }

    AstJumpLabel* findAddLabel(AstNode* nodep, bool endOfIter) {
	// Put label under given node, and if WHILE optionally at end of iteration
	UINFO(4,"Create label for "<<nodep<<endl);
	if (nodep->castJumpLabel()) return nodep->castJumpLabel(); // Done

	AstNode* underp = NULL;
	bool     under_and_next = true;
	if (nodep->castBegin()) underp = nodep->castBegin()->stmtsp();
	else if (nodep->castNodeFTask()) underp = nodep->castNodeFTask()->stmtsp();
	else if (nodep->castWhile()) {
	    if (endOfIter) {
		// Note we jump to end of bodysp; a FOR loop has its increment under incsp() which we don't skip
		underp = nodep->castWhile()->bodysp();
	    } else {
		underp = nodep; under_and_next=false; // IE we skip the entire while
	    }
	}
	else {
	    nodep->v3fatalSrc("Unknown jump point for break/disable/continue");
	    return NULL;
	}
	// Skip over variables as we'll just move them in a momement
	// Also this would otherwise prevent us from using a label twice
	// see t_func_return test.
	while (underp && underp->castVar()) underp = underp->nextp();
	if (underp) UINFO(5,"  Underpoint is "<<underp<<endl);

	if (!underp) {
	    nodep->v3fatalSrc("Break/disable/continue not under expected statement");
	    return NULL;
	} else if (underp->castJumpLabel()) {
	    return underp->castJumpLabel();
	} else { // Move underp stuff to be under a new label
	    AstJumpLabel* labelp = new AstJumpLabel(nodep->fileline(), NULL);

	    AstNRelinker repHandle;
	    if (under_and_next) underp->unlinkFrBackWithNext(&repHandle);
	    else underp->unlinkFrBack(&repHandle);
	    repHandle.relink(labelp);

	    labelp->addStmtsp(underp);
	    // Keep any AstVars under the function not under the new JumpLabel
	    for (AstNode* nextp, *varp=underp; varp; varp = nextp) {
		nextp = varp->nextp();
		if (varp->castVar()) {
		    labelp->addPrev(varp->unlinkFrBack());
		}
	    }
	    return labelp;
	}
    }

    // VISITORS
    virtual void visit(AstNodeModule* nodep, AstNUser*) {
	m_modp = nodep;
	m_repeatNum = 0;
	nodep->iterateChildren(*this);
	m_modp = NULL;
    }
    virtual void visit(AstNodeFTask* nodep, AstNUser*) {
	m_ftaskp = nodep;
	nodep->iterateChildren(*this);
	m_ftaskp = NULL;
    }
    virtual void visit(AstBegin* nodep, AstNUser*) {
	UINFO(8,"  "<<nodep<<endl);
	m_beginStack.push_back(nodep);
	nodep->iterateChildren(*this);
	m_beginStack.pop_back();
    }
    virtual void visit(AstRepeat* nodep, AstNUser*) {
	// So later optimizations don't need to deal with them,
	//    REPEAT(count,body) -> loop=count,WHILE(loop>0) { body, loop-- }
	// Note var can be signed or unsigned based on original number.
	AstNode* countp = nodep->countp()->unlinkFrBackWithNext();
   	string name = string("__Vrepeat")+cvtToStr(m_repeatNum++);
	// Spec says value is integral, if negative is ignored
	AstVar* varp = new AstVar(nodep->fileline(), AstVarType::BLOCKTEMP, name,
				  AstBitPacked(), 32);
	varp->numeric(AstNumeric::SIGNED);
	varp->dtypep()->numeric(AstNumeric::SIGNED);
	varp->usedLoopIdx(true);
	m_modp->addStmtp(varp);
	AstNode* initsp = new AstAssign(nodep->fileline(), new AstVarRef(nodep->fileline(), varp, true),
					countp);
	AstNode* decp = new AstAssign(nodep->fileline(), new AstVarRef(nodep->fileline(), varp, true),
				      new AstSub(nodep->fileline(), new AstVarRef(nodep->fileline(), varp, false),
						 new AstConst(nodep->fileline(), 1)));
	V3Number zero (nodep->fileline(), 32, 0);  zero.isSigned(true);
	AstNode* zerosp = new AstConst(nodep->fileline(), zero);
	AstNode* condp = new AstGtS(nodep->fileline(), new AstVarRef(nodep->fileline(), varp, false),
				    zerosp);
	AstNode* bodysp = nodep->bodysp(); if (bodysp) bodysp->unlinkFrBackWithNext();
	AstNode* newp = new AstWhile(nodep->fileline(),
				     condp,
				     bodysp,
				     decp);
	initsp = initsp->addNext(newp);
	newp = initsp;
	nodep->replaceWith(newp);
	nodep->deleteTree(); nodep=NULL;
    }
    virtual void visit(AstWhile* nodep, AstNUser*) {
	// Don't need to track AstRepeat/AstFor as they have already been converted
	AstWhile* lastLoopp = m_loopp;
	bool lastInc = m_loopInc;
	m_loopp = nodep;
	m_loopInc = false;
	nodep->precondsp()->iterateAndNext(*this);
	nodep->condp()->iterateAndNext(*this);
	nodep->bodysp()->iterateAndNext(*this);
	m_loopInc = true;
	nodep->incsp()->iterateAndNext(*this);
	m_loopInc = lastInc;
	m_loopp = lastLoopp;
    }
    virtual void visit(AstReturn* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
	AstFunc* funcp = m_ftaskp->castFunc();
	if (!m_ftaskp) { nodep->v3error("Return isn't underneath a task or function"); }
	else if (funcp  && !nodep->lhsp()) { nodep->v3error("Return underneath a function should have return value"); }
	else if (!funcp &&  nodep->lhsp()) { nodep->v3error("Return underneath a task shouldn't have return value"); }
	else {
	    if (funcp && nodep->lhsp()) {
		// Set output variable to return value
		nodep->addPrev(new AstAssign(nodep->fileline(),
					     new AstVarRef(nodep->fileline(), funcp->fvarp()->castVar(), true),
					     nodep->lhsp()->unlinkFrBackWithNext()));
	    }
	    // Jump to the end of the function call
	    AstJumpLabel* labelp = findAddLabel(m_ftaskp, false);
	    nodep->addPrev(new AstJumpGo(nodep->fileline(), labelp));
	}
	nodep->unlinkFrBack(); pushDeletep(nodep); nodep=NULL;
    }
    virtual void visit(AstBreak* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
	if (!m_loopp) { nodep->v3error("break isn't underneath a loop"); }
	else {
	    // Jump to the end of the loop
	    AstJumpLabel* labelp = findAddLabel(m_loopp, false);
	    nodep->addNextHere(new AstJumpGo(nodep->fileline(), labelp));
	}
	nodep->unlinkFrBack(); pushDeletep(nodep); nodep=NULL;
    }
    virtual void visit(AstContinue* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
	if (!m_loopp) { nodep->v3error("continue isn't underneath a loop"); }
	else {
	    // Jump to the end of this iteration
	    // If a "for" loop then need to still do the post-loop increment
	    AstJumpLabel* labelp = findAddLabel(m_loopp, true);
	    nodep->addNextHere(new AstJumpGo(nodep->fileline(), labelp));
	}
	nodep->unlinkFrBack(); pushDeletep(nodep); nodep=NULL;
    }
    virtual void visit(AstDisable* nodep, AstNUser*) {
	UINFO(8,"   DISABLE "<<nodep<<endl);
	nodep->iterateChildren(*this);
	AstBegin* beginp = NULL;
	for (BeginStack::reverse_iterator it = m_beginStack.rbegin(); it != m_beginStack.rend(); ++it) {
	    UINFO(9,"    UNDERBLK  "<<*it<<endl);
	    if ((*it)->name() == nodep->name()) {
		beginp = *it;
		break;
	    }
	}
	//if (debug()>=9) { UINFO(0,"\n"); beginp->dumpTree(cout,"  labeli: "); }
	if (!beginp) { nodep->v3error("disable isn't underneath a begin with name: "<<nodep->name()); }
	else {
	    // Jump to the end of the named begin
	    AstJumpLabel* labelp = findAddLabel(beginp, false);
	    nodep->addNextHere(new AstJumpGo(nodep->fileline(), labelp));
	}
	nodep->unlinkFrBack(); pushDeletep(nodep); nodep=NULL;
	//if (debug()>=9) { UINFO(0,"\n"); beginp->dumpTree(cout,"  labelo: "); }
    }
    virtual void visit(AstVarRef* nodep, AstNUser*) {
	if (m_loopInc && nodep->varp()) nodep->varp()->usedLoopIdx(true);
    }

    virtual void visit(AstConst* nodep, AstNUser*) {}

    virtual void visit(AstAlways* nodep, AstNUser*) {
		m_clocked = false;
		nodep->iterateChildren(*this);
		m_senItemList.clear(); // After iterating children in this process, clear the map
		m_clocked = false;
    }

    virtual void visit(AstIf* nodep, AstNUser*) {
		static int levelcnt = 0;
		if (m_clocked == true)
			levelcnt++;
		nodep->iterateChildren(*this);
		levelcnt --;
		if (levelcnt == 0)
			m_clocked = false;
    }

	virtual void visit(AstSenItem* nodep, AstNUser*) {
		AstNodeVarRef* m_varrefp = nodep->varrefp();
		m_senItemList [m_varrefp->name()] = nodep;
		nodep->iterateChildren(*this);
    }

    virtual void visit(AstVhdlQuotedAttribute* nodep, AstNUser*) {
		if (nodep->name() == "event") {
			transformVhdlEvent (nodep);
			m_clocked = true; // mark this part of the process as clocked
		}
		else {
			nodep->v3error ("Unsupported or invalid attribute " << nodep->name());
		}
		nodep->iterateChildren(*this);
    }

	virtual void visit(AstVhdlAssignVar* nodep, AstNUser*) {
	AstNode* valuep = nodep->op1p()->unlinkFrBack();
	AstNode* targetp = nodep->op2p()->unlinkFrBack();
	AstNode* newp = new AstAssign (nodep->fileline(), targetp, valuep);
	nodep->replaceWith(newp);
	nodep->iterateChildren(*this);
    }

    virtual void visit(AstVhdlAssignSig* nodep, AstNUser*) {
	AstNode* valuep = nodep->op1p()->unlinkFrBack();
	AstNode* targetp = nodep->op2p()->unlinkFrBack();
	AstNode* newp = NULL;
	if (m_clocked) {
		newp = new AstAssignDly (nodep->fileline(), targetp, valuep);
	}
	else {
		newp = new AstAssign (nodep->fileline(), targetp, valuep);
	}
	nodep->replaceWith(newp);
	nodep->iterateChildren(*this);
    }

    virtual void visit(AstNode* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }

	void transformVhdlEvent (AstNode* nodep) {
		AstAnd* m_prevp = nodep->backp()->castAnd(); // AstAnd over the attribute
		AstNode* m_varref = NULL; // variable referenced in the attribute
		AstNode* m_eqvarref = NULL; // variable reference in the equality
		AstNode* m_senref = NULL; // Corresponding element in the sensitivity list
		AstEq* m_eqref = NULL; // Equality check for edges
		AstConst* m_constref = NULL; // Constant value under the equality

		if ( nodep->castVhdlQuotedAttribute()->idp()) {
			m_varref = nodep->castVhdlQuotedAttribute()->idp()->unlinkFrBack();
			m_senref = m_senItemList[m_varref->name()]; // Corresponding element in the sensitivity list
		}

		if (m_senref) { // if  found in sensitivity list
			if (m_prevp) { // If signal'event and signal=const
					if (m_prevp->lhsp()->castEq()) { // Search for AstEq left and right in AstAnd
						m_eqref = m_prevp->lhsp()->castEq();
					}
					else if (m_prevp->rhsp()->castEq()) {
						m_eqref = m_prevp->rhsp()->castEq();
					}
					else {
						nodep->v3error ("Unable to find a synthesizable construct");
					}

					if (m_eqref->lhsp()->castConst() and m_eqref->rhsp()->castVarRef()) { // Search for const left and right in AstEq
						m_constref = m_eqref->lhsp()->castConst();
						m_eqvarref = m_eqref->rhsp()->castVarRef();
					}
					else if (m_eqref->rhsp()->castConst() and m_eqref->lhsp()->castVarRef()) {
						m_constref = m_eqref->rhsp()->castConst();
						m_eqvarref = m_eqref->lhsp()->castVarRef();
					}
					else {
						nodep->v3error ("Unable to find a synthesizable construct, value is not equal to a constant");
					}

					if (m_varref->same(m_eqvarref)) {
							if (m_constref->toUInt() == 0) { // Falling edge
								m_senref->castSenItem()->edgeType (AstEdgeType::ET_NEGEDGE);
							}
							else if (m_constref->toUInt() == 1) { // Rising edge
								m_senref->castSenItem()->edgeType (AstEdgeType::ET_POSEDGE);
							}
							else {
								nodep->v3error ("Unable to find a synthesizable construct, value is not covered in the constant values");
							}
							m_prevp->replaceWith (m_varref);
					}
					else { // If the value of the attribute is different than the one from the equality
						m_senref->castSenItem()->edgeType (AstEdgeType::ET_BOTHEDGE);
						nodep->replaceWith (m_varref);
					}
				}
				else { // if no check for equality, sensitive to both edges
					m_senref->castSenItem()->edgeType (AstEdgeType::ET_BOTHEDGE);
					nodep->replaceWith (m_varref);
				}
		}
		else {
			nodep->v3error ("Signal " << m_varref->name()  << " is not in sensitivity list");
		}
	}
public:
    // CONSTUCTORS
    LinkJumpVisitor(AstNetlist* nodep) {
	m_modp = NULL;
	m_ftaskp = NULL;
	m_loopp = NULL;
	m_loopInc = false;
	m_repeatNum = 0;
	nodep->accept(*this);
    }
    virtual ~LinkJumpVisitor() {}
};

//######################################################################
// Task class functions

void V3LinkJump::linkJump(AstNetlist* nodep) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    LinkJumpVisitor bvisitor (nodep);
}
