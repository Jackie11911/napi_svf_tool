#ifndef TAINTTRACKER_H
#define TAINTTRACKER_H

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVFIR/SVFIR.h"
#include "WPA/Andersen.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFG.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <set>
#include <memory>
#include "taintanalysis/TaintUnit.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include "taintanalysis/TaintMap.h"
#include <nlohmann/json.hpp>
class TaintTracker {

private:
    SVF::PAG* pag;
    SVF::Andersen* ander;
    SVF::SVFG* svfg;
    SVF::VFG* vfg;    
    std::set<SVF::NodeID> taintedNodes;
    std::unordered_map<SVF::NodeID, std::vector<SVF::NodeID>> nodeIDMap;
    std::queue<SVF::NodeID> worklist;
    std::set<SVF::NodeID> visitedNodes;
    std::vector<const llvm::Function*> targetedfunctions;
    std::vector<const llvm::Instruction*> targetedinst;

public:
    
    TaintTracker(SVF::PAG* pag, SVF::Andersen* ander, SVF::SVFG* svfg, SVF::VFG* vfg);
    
    // 检查指定节点是否被污染
    bool isTainted(SVF::NodeID id) const;
    void initializeFunctionArgs(const llvm::Function* func);
    nlohmann::json Traceker(const llvm::Function* func, std::vector<std::pair<SVF::NodeID, std::string>> paramNodeIDs, std::string funcName);

    bool isInstructionTainted(const llvm::Instruction* inst);

    std::vector<const llvm::Function*> getCalledFunctions(const llvm::Function* F, std::set<const llvm::Function*>& visited);
    TaintUnit handleDirectAssignment(const llvm::Instruction* inst);
    TaintUnit handlePhiInstruction(const llvm::Instruction* inst);
    TaintUnit handleReturnInstruction(const llvm::Instruction* inst);
    std::vector<TaintUnit> handleInvokeInstruction(const llvm::Instruction* inst);
    std::vector<TaintUnit> handleCallBase(const llvm::CallBase* callBase);
    bool isFunctionReturnTainted(const llvm::Function* func);
 
    void aliasAnalysis(SVF::NodeID id);
    void trackValueFlow(SVF::NodeID srcNode);
    void printNodeID(SVF::NodeID nodeId);
};

#endif // TAINTTRACKER_H