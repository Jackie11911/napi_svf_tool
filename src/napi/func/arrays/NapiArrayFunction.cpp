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

//NAPI_EXTERN napi_status napi_create_array(napi_env env, napi_value* result);
// "napi_create_array",           // 用于在Node-API模块中向ArkTS层创建一个ArkTS数组对象。
//NAPI_EXTERN napi_status napi_create_array_with_length(napi_env env,
//                                                      size_t length,
//                                                      napi_value* result);
// "napi_create_array_with_length",  // 用于在Node-API模块中向ArkTS层创建指定长度的ArkTS数组时。
//NAPI_EXTERN napi_status napi_is_array(napi_env env,
//                                      napi_value value,
//                                      bool* result);
// "napi_is_array",                  // 用于在Node-API模块中判断一个napi_value值是否为数组。
//NAPI_EXTERN napi_status napi_get_array_length(napi_env env,
//                                              napi_value value,
//                                              uint32_t* result);
// "napi_get_array_length"          // 用于在Node-API模块中获取ArkTS数组对象的长度。

void handleNapiArrayFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_array" << std::endl;
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

    // 获取第一个参数
    const llvm::Value* envParam = callInst->getOperand(0);
    NodeID envParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(envParam);
    int envID = taintMap.getParamIdByIndex(0); 
    if (envID == -1) {
        envID = taintMap.assignNewId(envParamNodeID);
    }
    summaryItemResult.addOperand("%"+std::to_string(envID));

    // 添加return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    if(calledFunctionName == "napi_create_array") {
        summaryItemResult.addOperand("top");
        // 获取第二个参数
        const llvm::Value* resultParam = callInst->getOperand(1);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
    }
    else if(calledFunctionName == "napi_create_array_with_length") {
        // 获取第二个参数
        const llvm::Value* lengthParam = callInst->getOperand(1);
        if (llvm::isa<llvm::Constant>(lengthParam)){
            std::string lengthConstant = parseConstant(lengthParam);
            if(lengthConstant == ""){
                lengthConstant = "null";
            }
            summaryItemResult.addOperand(lengthConstant);
        }
        else{
            summaryItemResult.addOperand("top");
        }

        // 获取第三个参数
        const llvm::Value* resultParam = callInst->getOperand(2);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
    }
    else {
        // 获取第二个参数
        const llvm::Value* valueParam = callInst->getOperand(1);
        NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
        handleTaintFlow(svfg, pag, valueParam, taintMap, summaryItems, summaryItemResult, ander);
        int valueID = taintMap.getNewIds(valueParamNodeID)[0];

        // 处理第三个参数
        const llvm::Value* resultParam = callInst->getOperand(2);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
        taintMap.addValueFlowSource(resultID, valueID);
    }
    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiArrayRegister {
        NapiArrayRegister() {
            NapiHandler::getInstance().registerHandler("napi_create_array", handleNapiArrayFunction);
            NapiHandler::getInstance().registerHandler("napi_create_array_with_length", handleNapiArrayFunction);
            NapiHandler::getInstance().registerHandler("napi_is_array", handleNapiArrayFunction);
            NapiHandler::getInstance().registerHandler("napi_get_array_length", handleNapiArrayFunction);
        }
    };

    static NapiArrayRegister _napi_array_reg;
}