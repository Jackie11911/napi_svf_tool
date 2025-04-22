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
// napi_status napi_create_date(napi_env env, double time, napi_value* result);
// napi_status napi_get_date_value(napi_env env, napi_value value, double* result)
// napi_status napi_is_date(napi_env env, napi_value value, bool* result)

void handleNapiDateFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_date" << std::endl;
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

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    int valueID = 0;

    if(calledFunctionName == "napi_create_date"){
        // 获取第二个参数
        const llvm::Value* timeParam = callInst->getOperand(1);
        if(llvm::isa<llvm::Constant>(timeParam)){
            std::string timeConstant = parseConstant(timeParam);
            summaryItemResult.addOperand(timeConstant);
        }
        else{
            NodeID timeParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(timeParam);
            const SVFVar* timeSVFVar = pag->getGNode(timeParamNodeID);
            handleTaintFlow(svfg, pag, timeParamNodeID, timeSVFVar, taintMap, summaryItems, summaryItemResult);
            valueID = taintMap.getNewIds(timeParamNodeID)[0];
        }
    }
    else{
        // 获取第二个参数
        const llvm::Value* valueParam = callInst->getOperand(1);
        NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
        const SVFVar* valueSVFVar = pag->getGNode(valueParamNodeID);
        handleTaintFlow(svfg, pag, valueParamNodeID, valueSVFVar, taintMap, summaryItems, summaryItemResult);
        valueID = taintMap.getNewIds(valueParamNodeID)[0];
    }

    // 获取第三个参数
    const llvm::Value* resultParam = callInst->getOperand(2);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    int resultID = taintMap.assignNewId(resultParamNodeID);
    summaryItemResult.addOperand("top");
    summaryItemResult.addRetValue("%"+std::to_string(resultID), 1);
    taintMap.addValueFlowSource(resultID, valueID);

    summaryItems.push_back(summaryItemResult);
    return;
}


namespace {
    struct NapiDateFunctionRegister {
        NapiDateFunctionRegister() {
            NapiHandler::getInstance().registerHandler("napi_create_date", handleNapiDateFunction);
            NapiHandler::getInstance().registerHandler("napi_get_date_value", handleNapiDateFunction);
            NapiHandler::getInstance().registerHandler("napi_is_date", handleNapiDateFunction);
        }
    };

    static NapiDateFunctionRegister _napi_date_function_reg;
}