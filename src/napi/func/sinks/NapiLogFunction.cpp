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

void handleNapiLogFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_log" << std::endl;
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    std::string calledFunctionName = "";

    const llvm::Function* calledFunction = callInst->getCalledFunction();
    if (!calledFunction) {
        std::cerr << "Warning: Could not determine called function for instruction: ";
        callInst->print(llvm::errs());
        llvm::errs() << "\n";
        if (callInst->getCalledOperand()->hasName()) {
            calledFunctionName = callInst->getCalledOperand()->getName().str();
        }
    } else{
        calledFunctionName = calledFunction->getName().str();
    }
    SummaryItem summaryItemResult(calledFunctionName, "Call");

    for (unsigned int i = 0; i < callInst->getNumOperands()-1; i++) {
        const llvm::Value* operand = callInst->getOperand(i);
        // nodeid
        NodeID operandNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(operand);
        if(operandNodeID == 0){
            summaryItemResult.addOperand("null");
            continue;
        }
        if (llvm::isa<llvm::Constant>(operand)){
            std::string operandConstant = parseConstant(operand);
            if(operandConstant == ""){
                operandConstant = "null";
            }
            summaryItemResult.addOperand(operandConstant);
            continue;
        }
        if(taintMap.getNewIds(operandNodeID).size() > 0){
            int newID = taintMap.getNewIds(operandNodeID)[0];
            summaryItemResult.addOperand("%"+std::to_string(newID));
        }
        else{
            handleTaintFlow(svfg, pag, operand, taintMap, summaryItems, summaryItemResult, ander);
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