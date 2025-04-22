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

//NAPI_EXTERN napi_status napi_call_function(napi_env env,
//                                           napi_value recv,
//                                           napi_value func,
//                                           size_t argc,
//                                           const napi_value* argv,
//                                           napi_value* result);

void handleNapiCallFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, std::vector<SummaryItem>& summaryItems) {
    // 解析inst
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;

    // 解析callInst调用了什么函数
    const llvm::Function* calledFunction = callInst->getCalledFunction();
    if (!calledFunction) return;
    SummaryItem summaryItemResult(calledFunction->getName().str(), "Call");

    
    const llvm::Value* envParam = callInst->getOperand(0);
    NodeID envParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(envParam);
    int envID = taintMap.assignNewId(envParamNodeID);
    summaryItemResult.addOperand("%"+std::to_string(envID));
    int funcID = 0;

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    for(int i = 1; i < 5; i++) {
        const llvm::Value* param = callInst->getOperand(i);
        if(llvm::isa<llvm::Constant>(param)){
            std::string paramConstant = parseConstant(param);
            summaryItemResult.addOperand(paramConstant);
        }
        else{
            NodeID paramNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(param);
            const SVFVar* paramSVFVar = pag->getGNode(paramNodeID);
            handleTaintFlow(svfg, pag, paramNodeID, paramSVFVar, taintMap, summaryItems, summaryItemResult);
            if(i == 2){
                funcID = taintMap.getNewIds(paramNodeID)[0];
            }
        }
    }
    
    // 获取第六个参数
    const llvm::Value* resultParam = callInst->getOperand(5);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    int resultID = taintMap.assignNewId(resultParamNodeID);
    summaryItemResult.addOperand("top");
    summaryItemResult.addRetValue("%"+std::to_string(resultID), 5);
    taintMap.addValueFlowSource(resultID, funcID);

    summaryItems.push_back(summaryItemResult);

    return;
}

// 注册处理器
namespace {
    struct NapiCallFunctionRegister {
        NapiCallFunctionRegister() {
            NapiHandler::getInstance().registerHandler("napi_call_function", handleNapiCallFunction);
        }
    };

    static NapiCallFunctionRegister _napi_call_function_reg;
}