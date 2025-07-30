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
 * char *strcpy(char *dest, const char *src)
 * char *strncpy(char *dest, const char *src, size_t n)
 */

void handleStrcpy(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "strcpy/strncpy" << std::endl;
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    // 处理dest参数
    const llvm::Value* destParam = callInst->getOperand(0);
    
    // 处理src参数
    const llvm::Value* srcParam = callInst->getOperand(1);
    NodeID srcParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(srcParam);
    
    // 判断src是否taint
    int srcTaintID = handlePhi(svfg, pag, srcParam, taintMap, summaryItems, ander);
    if(srcTaintID == -1){
        return;
    }
    else{
        // 如果src是taint的，那么dest也会被污染
        NodeID destParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(destParam);
        taintMap.setNewIDAtFront(destParamNodeID, srcTaintID);
    }
    return;
}

namespace {
    struct StrcpyRegister {
        StrcpyRegister() {
            NapiHandler::getInstance().registerHandler("strcpy", handleStrcpy);
            NapiHandler::getInstance().registerHandler("strncpy", handleStrcpy);
        }
    };

    static StrcpyRegister _strcpy_reg;
} 