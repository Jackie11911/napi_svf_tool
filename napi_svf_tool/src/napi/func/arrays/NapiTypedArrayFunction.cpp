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

//NAPI_EXTERN napi_status napi_is_typedarray(napi_env env,
//                                           napi_value value,
//                                           bool* result);

//NAPI_EXTERN napi_status napi_create_typedarray(napi_env env,
//                                               napi_typedarray_type type,
//                                               size_t length,
//                                               napi_value arraybuffer,
//                                               size_t byte_offset,
//                                               napi_value* result);

//NAPI_EXTERN napi_status napi_get_typedarray_info(napi_env env,
//                                                 napi_value typedarray,
//                                                 napi_typedarray_type* type,
//                                                 size_t* length,
//                                                 void** data,
//                                                 napi_value* arraybuffer,
//                                                 size_t* byte_offset);


void handleNapiTypedArrayFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_typedarray" << std::endl;
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
    const llvm::Value* returnValue = inst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    if(calledFunctionName == "napi_is_typedarray"){
        // 获取第二个参数
        const llvm::Value* valueParam = callInst->getOperand(1);
        NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
        const SVFVar* valueSVFVar = pag->getGNode(valueParamNodeID);
        handleTaintFlow(svfg, pag, valueParamNodeID, valueSVFVar, taintMap, summaryItems, summaryItemResult);
        int valueID = taintMap.assignNewId(valueParamNodeID);

        // 获取第三个参数
        const llvm::Value* resultParam = callInst->getOperand(2);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
        taintMap.addValueFlowSource(resultID, valueID);
    }
    else if(calledFunctionName == "napi_create_typedarray"){
        int valueID = 0;
        for(int i = 1; i < 5; i++){
            const llvm::Value* param = callInst->getOperand(i);
            if(llvm::isa<llvm::Constant>(param)){
                std::string paramConstant = parseConstant(param);
                summaryItemResult.addOperand(paramConstant);
            }
            else{
                NodeID paramNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(param);
                const SVFVar* paramSVFVar = pag->getGNode(paramNodeID);
                handleTaintFlow(svfg, pag, paramNodeID, paramSVFVar, taintMap, summaryItems, summaryItemResult);
                if (i==3){
                    valueID = taintMap.getNewIds(paramNodeID)[0];
                }
            } 
        }

        // 第六个参数
        const llvm::Value *resultParam = callInst->getOperand(5);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 5);
        taintMap.addValueFlowSource(resultID, valueID);
    }
    else if (calledFunctionName == "napi_get_typedarray_info"){
        // 获取第二个参数
        const llvm::Value* typedarrayParam = callInst->getOperand(1);
        NodeID typedarrayParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(typedarrayParam);
        const SVFVar* typedarraySVFVar = pag->getGNode(typedarrayParamNodeID);
        handleTaintFlow(svfg, pag, typedarrayParamNodeID, typedarraySVFVar, taintMap, summaryItems, summaryItemResult);
        int typedarrayID = taintMap.assignNewId(typedarrayParamNodeID);

        for(int i=2; i<7; i++){
            const llvm::Value* param = callInst->getOperand(i);
            if(llvm::isa<llvm::Constant>(param)){
                std::string paramConstant = parseConstant(param);
                summaryItemResult.addOperand(paramConstant);
            }
            else{
                NodeID paramNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(param);
                int paramID = taintMap.assignNewId(paramNodeID);
                summaryItemResult.addOperand("top");
                summaryItemResult.addRetValue("%"+std::to_string(paramID), i);
                taintMap.addValueFlowSource(typedarrayID, paramID);
            }
        }
    }

    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiTypedArrayRegister {
        NapiTypedArrayRegister() {
            NapiHandler::getInstance().registerHandler("napi_is_typedarray", handleNapiTypedArrayFunction);
            NapiHandler::getInstance().registerHandler("napi_create_typedarray", handleNapiTypedArrayFunction);
            NapiHandler::getInstance().registerHandler("napi_get_typedarray_info", handleNapiTypedArrayFunction);
        }
    };

    static NapiTypedArrayRegister _napi_typedarray_reg;
}