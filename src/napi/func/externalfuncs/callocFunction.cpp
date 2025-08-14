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

/**
 * void *calloc(size_t nmemb, size_t size)
 */

void handleCalloc(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "calloc" << std::endl;
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
    } else {
        calledFunctionName = calledFunction->getName().str();
    }

    SummaryItem summaryItemResult(calledFunctionName, "Call");

    // 处理第一个参数 (nmemb)
    const llvm::Value* nmembParam = callInst->getOperand(0);
    if (llvm::isa<llvm::Constant>(nmembParam)) {
        std::string nmembConstant = parseConstant(nmembParam);
        if(nmembConstant == ""){
            nmembConstant = "null";
        }
        summaryItemResult.addOperand(nmembConstant);
    } else {
        summaryItemResult.addOperand("top");
    }

    // 处理第二个参数 (size)
    const llvm::Value* sizeParam = callInst->getOperand(1);
    if (llvm::isa<llvm::Constant>(sizeParam)) {
        std::string sizeConstant = parseConstant(sizeParam);
        if(sizeConstant == ""){
            sizeConstant = "null";
        }
        summaryItemResult.addOperand(sizeConstant);
    } else {
        summaryItemResult.addOperand("top");
    }

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue) {
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0) {
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    summaryItems.push_back(summaryItemResult);
    return;
}

namespace {
    struct CallocRegister {
        CallocRegister() {
            NapiHandler::getInstance().registerHandler("calloc", handleCalloc);
        }
    };

    static CallocRegister _calloc_reg;
} 