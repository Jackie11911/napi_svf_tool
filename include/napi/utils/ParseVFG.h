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
std::vector<NodeID> bfsPredecessors(const SVFG* svfg,  const SVFIR* pag, const SVFVar* startSVFVar);
std::vector<NodeID> getTaintmapExistingNodes(std::vector<NodeID>& nodes, TaintMap& taintMap, SVF::Andersen* ander);
void parseArgsOperand(const SVFG* svfg, const SVFIR* pag, NodeID valueParamNodeID, const SVFVar* valueSVFVar, TaintMap& taintMap, std::vector<SummaryItem>& summaryItems, SummaryItem& summaryItemResult, int argc, SVF::Andersen* ander);
void handleTaintFlow(const SVFG* svfg, const SVFIR* pag, const llvm::Value* valueParam, TaintMap& taintMap, std::vector<SummaryItem>& summaryItems, SummaryItem& summaryItemResult, SVF::Andersen* ander);
int handlePhi(const SVFG* svfg, const SVFIR* pag, const llvm::Value* valueParam, TaintMap& taintMap, std::vector<SummaryItem>& summaryItems, SVF::Andersen* ander);
std::string parseConstant(const llvm::Value* value);
int parseIntValue(const SVFVar* valueSVFVar, const SVFG* svfg, const SVFIR* pag, NodeID intNodeId, PointerAnalysis* ander);
NodeID parseArgvValue(SVFVar* argvSVFVar, const SVFG* svfg, const SVFIR* pag);

#endif
