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
 * int atoi(const char *nptr)
 * long atol(const char *nptr)
 * long long atoll(const char *nptr)
 * long long atoq(const char *nptr)
 */

void handleAtoiFunctions(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "atoi/atol/atoll/atoq" << std::endl;
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    // 获取输入字符串参数
    const llvm::Value* strParam = callInst->getOperand(0);
    
    // 获取返回值
    const llvm::Value* retVal = callInst;
    NodeID retValNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(retVal);
    
    // 判断输入字符串是否taint
    int strTaintID = handlePhi(svfg, pag, strParam, taintMap, summaryItems, ander);
    if(strTaintID != -1){
        // 如果输入字符串是taint的，那么返回值也会被污染
        taintMap.setNewIDAtFront(retValNodeID, strTaintID);
    }
    return;
}

namespace {
    struct AtoiFunctionsRegister {
        AtoiFunctionsRegister() {
            NapiHandler::getInstance().registerHandler("atoi", handleAtoiFunctions);
            NapiHandler::getInstance().registerHandler("atol", handleAtoiFunctions);
            NapiHandler::getInstance().registerHandler("atoll", handleAtoiFunctions);
            NapiHandler::getInstance().registerHandler("atoq", handleAtoiFunctions);
        }
    };

    static AtoiFunctionsRegister _atoi_reg;
} 