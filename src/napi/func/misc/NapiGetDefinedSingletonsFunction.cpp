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

//NAPI_EXTERN napi_status napi_get_undefined(napi_env env, napi_value* result);
//NAPI_EXTERN napi_status napi_get_null(napi_env env, napi_value* result);
//NAPI_EXTERN napi_status napi_get_global(napi_env env, napi_value* result);
//NAPI_EXTERN napi_status napi_get_boolean(napi_env env,
//                                         bool value,
//                                         napi_value* result);

void handleGetDefinedSingletonsFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "GetDefinedSingletonsFunction" << std::endl;
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

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }
    
    if(calledFunctionName == "napi_get_boolean"){
        // 获取第二个参数
        const llvm::Value* valueParam = callInst->getOperand(1);
        if(llvm::isa<llvm::Constant>(valueParam)){
            std::string valueConstant = parseConstant(valueParam);
            if(valueConstant == ""){
                valueConstant = "null";
            }
            summaryItemResult.addOperand(valueConstant);
        }
        else{
            NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
            int valueID = taintMap.assignNewId(valueParamNodeID);
            summaryItemResult.addOperand("%"+std::to_string(valueID));
        }

        const llvm::Value* resultParam = callInst->getOperand(2);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
    }
    else{
        summaryItemResult.addOperand("top");
        const llvm::Value* resultParam = callInst->getOperand(1);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 1);
    }

    summaryItems.push_back(summaryItemResult);
    return;
}



namespace {
    struct NapiGetDefinedSingletonsFunctionRegister {
        NapiGetDefinedSingletonsFunctionRegister() {
            NapiHandler::getInstance().registerHandler("napi_get_undefined", handleGetDefinedSingletonsFunction);
            NapiHandler::getInstance().registerHandler("napi_get_null", handleGetDefinedSingletonsFunction);
            NapiHandler::getInstance().registerHandler("napi_get_global", handleGetDefinedSingletonsFunction);
            NapiHandler::getInstance().registerHandler("napi_get_boolean", handleGetDefinedSingletonsFunction);
        }
    };

    static NapiGetDefinedSingletonsFunctionRegister _napi_get_defined_singletons_function_reg;
}