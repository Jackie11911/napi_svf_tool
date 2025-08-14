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
 * void *malloc(size_t size) <br>
 * void *xmalloc(size_t size) <br>
 * new <br>
 * new[]
 */

void handleMalloc(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "malloc" << std::endl;
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
    if(calledFunctionName == "_Znwj"){
        calledFunctionName = "operator.new";
    }
    else if(calledFunctionName == "_Znaj"){
        calledFunctionName = "operator.new[]";
    }
    else if(calledFunctionName == "malloc"){
        calledFunctionName = "malloc";
    }
    else if(calledFunctionName == "_Z7xmallocj"){
        calledFunctionName = "xmalloc";
    }
    SummaryItem summaryItemResult(calledFunctionName, "Call");

    // 处理参数
    const llvm::Value* sizeParam = callInst->getOperand(0);
    if (llvm::isa<llvm::Constant>(sizeParam)){
        std::string sizeConstant = parseConstant(sizeParam);
        if (sizeConstant == "")
        {
            sizeConstant = "null";
        }
        
        summaryItemResult.addOperand(sizeConstant);
    }
    else{
        summaryItemResult.addOperand("top");
    }

    // return value
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



namespace {
    struct MallocRegister {
        MallocRegister() {
            NapiHandler::getInstance().registerHandler("malloc", handleMalloc);
            NapiHandler::getInstance().registerHandler("_Z7xmallocj", handleMalloc);
            NapiHandler::getInstance().registerHandler("_Znwj", handleMalloc);
            NapiHandler::getInstance().registerHandler("_Znaj", handleMalloc);
        }
    };

    static MallocRegister _malloc_reg;
}