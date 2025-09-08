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

//NAPI_EXTERN napi_status napi_set_element(napi_env env,
//                                         napi_value object,
//                                         uint32_t index,
//                                         napi_value value);

//NAPI_EXTERN napi_status napi_has_element(napi_env env,
//                                         napi_value object,
//                                         uint32_t index,
//                                         bool* result);

//NAPI_EXTERN napi_status napi_get_element(napi_env env,
//                                         napi_value object,
//                                         uint32_t index,
//                                         napi_value* result);

//NAPI_EXTERN napi_status napi_delete_element(napi_env env,
//                                            napi_value object,
//                                            uint32_t index,
//                                            bool* result);

void handleNapiElementFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_element" << std::endl;
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
    const llvm::Value* envParam = inst->getOperand(0);
    NodeID envParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(envParam);
    int envID = taintMap.getParamIdByIndex(0); 
    if (envID == -1) {
        envID = taintMap.assignNewId(envParamNodeID);
    }
    summaryItemResult.addOperand("%"+std::to_string(envID));

    // 处理返回值
    const llvm::Value* returnValue = inst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    // 获取第二个参数
    const llvm::Value* objectParam = inst->getOperand(1);
    NodeID objectParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(objectParam);
    handleTaintFlow(svfg, pag, objectParam, taintMap, summaryItems, summaryItemResult, ander);
    int objectID = taintMap.getNewIds(objectParamNodeID)[0];
    
    // 获取第三个参数
    const llvm::Value* indexParam = inst->getOperand(2);
    if (llvm::isa<llvm::Constant>(indexParam)){
        std::string indexConstant = parseConstant(indexParam);
        if(indexConstant == ""){
            indexConstant = "null";
        }
        std::cout << "indexConstant: " << indexConstant << std::endl;
        summaryItemResult.addOperand(indexConstant);
    }
    else{
        summaryItemResult.addOperand("top");
    }

    // 处理第四个参数
    const llvm::Value* resultParam = inst->getOperand(3);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    if(calledFunctionName == "napi_set_element"){
        handleTaintFlow(svfg, pag, resultParam, taintMap, summaryItems, summaryItemResult, ander);
    }
    else{
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 3);
        taintMap.addValueFlowSource(resultID, objectID);
    }
    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiElementRegister {
        NapiElementRegister() {
            NapiHandler::getInstance().registerHandler("napi_set_element", handleNapiElementFunction);
            NapiHandler::getInstance().registerHandler("napi_get_element", handleNapiElementFunction);
            NapiHandler::getInstance().registerHandler("napi_has_element", handleNapiElementFunction);
            NapiHandler::getInstance().registerHandler("napi_delete_element", handleNapiElementFunction);
        }
    };

    static NapiElementRegister _napi_element_reg;
}