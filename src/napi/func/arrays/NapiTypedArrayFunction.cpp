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


void handleNapiTypedArrayFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_typedarray" << std::endl;
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
        handleTaintFlow(svfg, pag, valueParam, taintMap, summaryItems, summaryItemResult, ander);
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
                if(paramConstant == ""){
                    paramConstant = "null";
                }
                summaryItemResult.addOperand(paramConstant);
            }
            else{
                NodeID paramNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(param);
                handleTaintFlow(svfg, pag, param, taintMap, summaryItems, summaryItemResult, ander);
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
        handleTaintFlow(svfg, pag, typedarrayParam, taintMap, summaryItems, summaryItemResult, ander);
        int typedarrayID = taintMap.assignNewId(typedarrayParamNodeID);

        for(int i=2; i<7; i++){
            const llvm::Value* param = callInst->getOperand(i);
            if(llvm::isa<llvm::Constant>(param)){
                std::string paramConstant = parseConstant(param);
                if(paramConstant == ""){
                    paramConstant = "null";
                }
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