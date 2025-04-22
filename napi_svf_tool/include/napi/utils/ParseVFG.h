#ifndef PARSEVFG_H
#define PARSEVFG_H

#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include <iostream>
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"
#include "taintanalysis/TaintMap.h"
#include "taintanalysis/SummaryItem.h"

using namespace SVF;

std::pair<NodeID, int> parseLoadVFG(SVFVar* loadSVFVar, const SVFG* svfg, const SVFIR* pag);
std::vector<NodeID> bfsPredecessors(const SVFG* svfg, const SVFVar* startSVFVar, const SVFIR* pag);
std::vector<NodeID> bfsPredecessorsWithImplicitFlow(const SVFG* svfg,const SVFVar* startSVFVar, const SVFIR* pag);
std::vector<NodeID> getTaintmapExistingNodes(std::vector<NodeID>& nodes, TaintMap& taintMap);
void handleTaintFlow(const SVFG* svfg, const SVFIR* pag, NodeID valueParamNodeID, const SVFVar* valueSVFVar, TaintMap& taintMap, std::vector<SummaryItem>& summaryItems, SummaryItem& summaryItemResult);
std::string parseConstant(const llvm::Value* value);

#endif
