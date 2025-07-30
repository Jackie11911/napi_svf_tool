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

// napi_status napi_get_value_bigint_words(napi_env env,
//                                         napi_value value,
//                                         int* sign_bit,
//                                         size_t* word_count,
//                                         uint64_t* words);

void handleNapiGetValueBigintWordsFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
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
    const llvm::Value* valueParam = callInst->getOperand(1);
    NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
    handleTaintFlow(svfg, pag, valueParam, taintMap, summaryItems, summaryItemResult, ander);
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
            NodeID paramNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(param);
            int paramID = taintMap.assignNewId(paramNodeID);
            summaryItemResult.addOperand("top");
            summaryItemResult.addRetValue("%"+std::to_string(paramID), i);
            taintMap.addValueFlowSource(paramID, valueID);
        }
    }

    summaryItems.push_back(summaryItemResult);
    return;
    
}

// 注册处理器
namespace {
    struct NapiGetValueBigintWordsRegister {
        NapiGetValueBigintWordsRegister() {
           NapiHandler::getInstance().registerHandler("napi_get_value_bigint_words", handleNapiGetValueBigintWordsFunction);
        }
    };

    static NapiGetValueBigintWordsRegister _napi_get_value_bigint_words_reg;
}