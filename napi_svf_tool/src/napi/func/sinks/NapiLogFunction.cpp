#include "napi/NapiHandler.h"
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include <iostream>
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"
#include "napi/utils/ParseVFG.h"

using namespace SVF;

void handleNapiLogFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_log" << std::endl;
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    const llvm::Function* calledFunction = callInst->getCalledFunction();
    if (!calledFunction) return;
    std::string calledFunctionName = calledFunction->getName().str();
    SummaryItem summaryItemResult(calledFunctionName, "Call");

    for (unsigned int i = 0; i < callInst->getNumOperands(); i++) {
        const llvm::Value* operand = callInst->getOperand(i);
        // nodeid
        NodeID operandNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(operand);
        if(operandNodeID == 0){
            summaryItemResult.addOperand("top");
            continue;
        }
        if (llvm::isa<llvm::Constant>(operand)){
            std::string operandConstant = parseConstant(operand);
            summaryItemResult.addOperand(operandConstant);
        }
        else{
            const SVFVar* operandSVFVar = pag->getGNode(operandNodeID);
            std::vector<NodeID> preNodeIDs = bfsPredecessors(svfg, operandSVFVar, pag);
            std::vector<NodeID> existingNodeIDs = getTaintmapExistingNodes(preNodeIDs, taintMap);
            if (existingNodeIDs.size() == 0){
                summaryItemResult.addOperand("top");
            }
            else{
                handleTaintFlow(svfg, pag, operandNodeID, operandSVFVar, taintMap, summaryItems, summaryItemResult);
            }
        }
    }
    // add ret value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }
    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiLogRegister {
        NapiLogRegister() {
            NapiHandler::getInstance().registerHandler("OH_LOG_Print", handleNapiLogFunction);
        }
    };

    static NapiLogRegister _napi_log_reg;
}