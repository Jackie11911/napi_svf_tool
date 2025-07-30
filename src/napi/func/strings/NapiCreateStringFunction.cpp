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

// napi_status napi_create_string_utf8(napi_env env,
//                                    const char* str,
//                                    size_t length,
//                                    napi_value* result);


//NAPI_EXTERN napi_status napi_create_string_latin1(napi_env env,
//                                                  const char* str,
//                                                  size_t length,
//                                                  napi_value* result);


//NAPI_EXTERN napi_status napi_create_string_utf16(napi_env env,
//                                                 const char16_t* str,
//                                                 size_t length,
//                                                 napi_value* result);

void handleNapiCreateStringFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
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


    for (int i = 1; i < 3; i++){
        const llvm::Value* param = callInst->getOperand(i);
        if (llvm::isa<llvm::Constant>(param)){
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

    // 获取第三个参数
    const llvm::Value* resultParam = callInst->getOperand(3);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    int resultID = taintMap.assignNewId(resultParamNodeID);
    summaryItemResult.addRetValue("%"+std::to_string(resultID), 3);
    summaryItemResult.addOperand("top");

    
    summaryItems.push_back(summaryItemResult);
    return;
}

namespace {
    struct NapiCreateStringRegister {
        NapiCreateStringRegister() {
            NapiHandler::getInstance().registerHandler("napi_create_string_utf8", handleNapiCreateStringFunction);
            NapiHandler::getInstance().registerHandler("napi_create_string_latin1", handleNapiCreateStringFunction);
            NapiHandler::getInstance().registerHandler("napi_create_string_utf16", handleNapiCreateStringFunction);
        }
    };

    static NapiCreateStringRegister _napi_create_string_reg;
}