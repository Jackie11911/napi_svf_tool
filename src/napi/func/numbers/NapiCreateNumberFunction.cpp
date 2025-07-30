#include "napi/NapiHandler.h"
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include <iostream>
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"
#include "napi/utils/ParseVFG.h"

// napi_status napi_create_double(napi_env env, double value, napi_value* result)

// napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result)

// napi_status napi_create_uint32(napi_env env, uint32_t value, napi_value* result)

// napi_status napi_create_int64(napi_env env, int64_t value, napi_value* result)

// napi_status napi_create_bigint_int64(napi_env env,
//                              int64_t value,
//                              napi_value* result);

// napi_status napi_create_bigint_uint64(napi_env env,
//                               uint64_t value,
//                               napi_value* result);


using namespace SVF;

void handleNapiCreateNumber(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_create_number" << std::endl;
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
    int valueID = -1;
    if(taintMap.getNewIds(valueParamNodeID).size() == 1 ){
        valueID = taintMap.getNewIds(valueParamNodeID)[0];
        summaryItemResult.addOperand("%"+std::to_string(valueID));
    } else {
        handleTaintFlow(svfg, pag, valueParam, taintMap, summaryItems, summaryItemResult, ander);
    }

    // return value
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    // 第三个参数
    const llvm::Value* resultParam = callInst->getOperand(2);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    int resultID = taintMap.assignNewId(resultParamNodeID);
    summaryItemResult.addOperand("top");
    taintMap.addValueFlowSource(resultID, valueID);
    summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);

    summaryItems.push_back(summaryItemResult);
    return;
}


// 注册处理器
namespace {
    struct NapiCreateNumberRegister {
        NapiCreateNumberRegister() {
           NapiHandler::getInstance().registerHandler("napi_create_double", handleNapiCreateNumber);
           NapiHandler::getInstance().registerHandler("napi_create_int32", handleNapiCreateNumber);
           NapiHandler::getInstance().registerHandler("napi_create_uint32", handleNapiCreateNumber);
           NapiHandler::getInstance().registerHandler("napi_create_int64", handleNapiCreateNumber);
           NapiHandler::getInstance().registerHandler("napi_create_bigint_int64", handleNapiCreateNumber);
           NapiHandler::getInstance().registerHandler("napi_create_bigint_uint64", handleNapiCreateNumber);
        }
    };

    static NapiCreateNumberRegister _napi_create_number_reg;
}