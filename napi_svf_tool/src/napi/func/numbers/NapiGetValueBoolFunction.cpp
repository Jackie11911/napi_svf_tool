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

// napi_status napi_get_value_bool(napi_env env, napi_value value, bool* result)

void handleNapiGetValueBoolFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, std::vector<SummaryItem>& summaryItems) {
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    const llvm::Function* calledFunction = callInst->getCalledFunction();
    if (!calledFunction) return;
    std::string calledFunctionName = calledFunction->getName().str();
    SummaryItem summaryItemResult(calledFunctionName, "Call");

    // 获取第一个参数
    const llvm::Value* envParam = callInst->getOperand(0);
    NodeID envParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(envParam);
    int envID = taintMap.assignNewId(envParamNodeID);
    summaryItemResult.addOperand("%"+std::to_string(envID));

    // 获取第二个参数
    const llvm::Value* valueParam = callInst->getOperand(1);
    NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
    const SVFVar* valueParamSVFVar = pag->getGNode(valueParamNodeID);
    handleTaintFlow(svfg, pag, valueParamNodeID, valueParamSVFVar, taintMap, summaryItems, summaryItemResult);
    int valueID = taintMap.assignNewId(valueParamNodeID);

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    // 获取第三个参数
    const llvm::Value* resultParam = callInst->getOperand(2);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    int resultID = taintMap.assignNewId(resultParamNodeID);
    if (resultID != -1 && valueID != -1){
        taintMap.addValueFlowSource(resultID, valueID);
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
    }
    summaryItemResult.addOperand("top");

    summaryItems.push_back(summaryItemResult);
    return;
    
}

namespace {
    struct NapiGetValueBoolRegister {
        NapiGetValueBoolRegister() {
            NapiHandler::getInstance().registerHandler("napi_get_value_bool", handleNapiGetValueBoolFunction);
        }
    };

    static NapiGetValueBoolRegister _napi_get_value_bool_reg;
}