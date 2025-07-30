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

//NAPI_EXTERN napi_status napi_get_property_names(napi_env env,
//                                                napi_value object,
//                                                napi_value* result);

//NAPI_EXTERN napi_status napi_set_property(napi_env env,
//                                          napi_value object,
//                                          napi_value key,
//                                          napi_value value);

//NAPI_EXTERN napi_status napi_has_property(napi_env env,
//                                          napi_value object,
//                                          napi_value key,
//                                          bool* result);

//NAPI_EXTERN napi_status napi_get_property(napi_env env,
//                                          napi_value object,
//                                          napi_value key,
//                                          napi_value* result);

//NAPI_EXTERN napi_status napi_delete_property(napi_env env,
//                                             napi_value object,
//                                             napi_value key,
//                                             bool* result);

//NAPI_EXTERN napi_status napi_has_own_property(napi_env env,
//                                              napi_value object,
//                                              napi_value key,
//                                              bool* result);

//NAPI_EXTERN napi_status napi_set_named_property(napi_env env,
//                                          napi_value object,
//                                          const char* utf8name,
//                                          napi_value value);

//NAPI_EXTERN napi_status napi_has_named_property(napi_env env,
//                                          napi_value object,
//                                          const char* utf8name,
//                                          bool* result);

//NAPI_EXTERN napi_status napi_get_named_property(napi_env env,
//                                          napi_value object,
//                                          const char* utf8name,
//                                          napi_value* result);

//napi_get_all_property_names(napi_env env,
//                            napi_value object,
//                            napi_key_collection_mode key_mode,
//                            napi_key_filter key_filter,
//                            napi_key_conversion key_conversion,
//                            napi_value* result);

void handleNapiPropertyFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
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

    // 获取第二个参数
    const llvm::Value* objectParam = callInst->getOperand(1);
    NodeID objectParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(objectParam);
    handleTaintFlow(svfg, pag, objectParam, taintMap, summaryItems, summaryItemResult, ander);
    int objectID = taintMap.assignNewId(objectParamNodeID);

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }
    
    if (calledFunctionName == "napi_get_property_names"){
        // 获取第三个参数
        const llvm::Value* resultParam = callInst->getOperand(2);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        if (resultID != -1 && objectID != -1){
            taintMap.addValueFlowSource(resultID, objectID);
            summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
        }
        summaryItemResult.addOperand("top");
    }

    if(calledFunctionName == "napi_set_property" || calledFunctionName == "napi_set_named_property"){
        for(int i = 2; i < callInst->getNumOperands()-1; i++){
            const llvm::Value* param = callInst->getOperand(i);
            if(llvm::isa<llvm::Constant>(param)){
                std::string valueConstant = parseConstant(param);
                if(valueConstant == ""){
                    valueConstant = "null";
                }
                summaryItemResult.addOperand(valueConstant);
            }
            else{
                handleTaintFlow(svfg, pag, param, taintMap, summaryItems, summaryItemResult, ander);
            }
        }
    }

    if(calledFunctionName == "napi_has_property" || calledFunctionName == "napi_get_property" || calledFunctionName == "napi_delete_property" || calledFunctionName == "napi_has_own_property" || calledFunctionName == "napi_has_named_property" || calledFunctionName == "napi_get_named_property"){
        int valueID = -1;
        // 获取第三个参数
        const llvm::Value* valueParam = callInst->getOperand(2);
        if(llvm::isa<llvm::Constant>(valueParam)){
            std::string valueConstant = parseConstant(valueParam);
            if(valueConstant == ""){
                valueConstant = "null";
            }
            summaryItemResult.addOperand(valueConstant);
        }
        else{
            NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
            handleTaintFlow(svfg, pag, valueParam, taintMap, summaryItems, summaryItemResult, ander);
            int valueID = taintMap.assignNewId(valueParamNodeID);
            summaryItemResult.addOperand("%"+std::to_string(valueID));
        }

        // 获取第四个参数
        const llvm::Value* resultParam = callInst->getOperand(3);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
        if (resultID != -1 && valueID != -1){
            taintMap.addValueFlowSource(resultID, valueID);
        }
    }

    if(calledFunctionName == "napi_get_all_property_names"){
        for(int i = 2; i < 5; i++){
            const llvm::Value* param = callInst->getOperand(i);
            if(llvm::isa<llvm::Constant>(param)){
                std::string valueConstant = parseConstant(param);
                if(valueConstant == ""){
                    valueConstant = "null";
                }
                summaryItemResult.addOperand(valueConstant);
            }
            else{
                handleTaintFlow(svfg, pag, param, taintMap, summaryItems, summaryItemResult, ander);
            } 
        }

        // 获取第五个参数
        const llvm::Value* resultParam = callInst->getOperand(5);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 5);
        summaryItemResult.addOperand("top");
    }

    summaryItems.push_back(summaryItemResult);
    return;
}

namespace {
    struct NapiPropertyFunctionsRegister {
        NapiPropertyFunctionsRegister() {
            NapiHandler::getInstance().registerHandler("napi_get_property_names", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_set_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_has_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_get_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_delete_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_has_own_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_set_named_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_has_named_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_get_named_property", handleNapiPropertyFunction);
            NapiHandler::getInstance().registerHandler("napi_get_all_property_names", handleNapiPropertyFunction);
        }
    };

    static NapiPropertyFunctionsRegister _napi_property_functions_reg;
}