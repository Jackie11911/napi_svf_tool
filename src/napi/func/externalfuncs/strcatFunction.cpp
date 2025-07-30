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
 * char *strcat(char *dest, const char *src)
 * char *strncat(char *dest, const char *src, size_t n)
 */

void handleStrcat(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "strcat/strncat" << std::endl;
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    // 处理dest参数
    const llvm::Value* destParam = callInst->getOperand(0);
    
    // 处理src参数
    const llvm::Value* srcParam = callInst->getOperand(1);
    NodeID srcParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(srcParam);
    // 判断src是否taint
    int srcTaintID = handlePhi(svfg, pag, srcParam, taintMap, summaryItems, ander);
    if(srcTaintID==-1){
        return;
    }
    else{
        int dstTaintID = handlePhi(svfg, pag, destParam, taintMap, summaryItems, ander);
        NodeID destParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(destParam);
        taintMap.setNewID(destParamNodeID, srcTaintID);
        if(dstTaintID == -1){
            taintMap.setNewID(destParamNodeID,dstTaintID);
        }
        else{
            SummaryItem phiItem1("", "Phi");
            phiItem1.addOperand("%"+std::to_string(srcTaintID));
            phiItem1.addOperand("%"+std::to_string(dstTaintID));
            phiItem1.addRetValue("%"+std::to_string(dstTaintID), -1);
            summaryItems.push_back(phiItem1);
        }
    }
    return;
}

namespace {
    struct StrcatRegister {
        StrcatRegister() {
            NapiHandler::getInstance().registerHandler("strcat", handleStrcat);
            NapiHandler::getInstance().registerHandler("strncat", handleStrcat);
        }
    };

    static StrcatRegister _strcat_reg;
} 