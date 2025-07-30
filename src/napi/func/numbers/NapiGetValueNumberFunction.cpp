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

// napi_status napi_get_value_double(napi_env env,
//                                  napi_value value,
//                                  double* result)

// napi_status napi_get_value_int32(napi_env env,
//                                 napi_value value,
//                                 int32_t* result)

// napi_status napi_get_value_int64(napi_env env,
//                                 napi_value value,
//                                 int64_t* result)

// napi_status napi_get_value_uint32(napi_env env,
//                                  napi_value value,
//                                  uint32_t* result)

// napi_status napi_get_value_bigint_int64(napi_env env,
//                                         napi_value value,
//                                         int64_t* result,
//                                         bool* lossless);

// napi_status napi_get_value_bigint_uint64(napi_env env,
//                                         napi_value value,
//                                         uint64_t* result,
//                                         bool* lossless);



// 处理napi_get_value_number的函数实现
void handleNapiGetValueNumber(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_get_value_number" << std::endl;
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
    SVFVar* valueSVFVar = pag->getGNode(valueParamNodeID);
    std::pair<NodeID, int> valuePtrPair = parseLoadVFG(valueSVFVar, svfg, pag);
    int valueptrid = -1;
    if (valuePtrPair.first == -1){
        // 寻找phi的可能性?还是创建一个新的？目前看应该要phi
    } else {
        std::vector<int> valueptrids = taintMap.getNewIds(valuePtrPair.first);
        if (valuePtrPair.second != -1){
            valueptrid = valueptrids[valuePtrPair.second];
        }
        summaryItemResult.addOperand("%"+std::to_string(valueptrid));
    }

    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    // 获取第三个参数
    const llvm::Value* resultParam = callInst->getOperand(2);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    int resultID = taintMap.assignNewId(resultParamNodeID);
    if (resultID != -1 && valueptrid != -1){
        taintMap.addValueFlowSource(resultID, valueptrid);
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 2);
    }
    summaryItemResult.addOperand("top");

    if(calledFunctionName == "napi_get_value_bigint_int64" || calledFunctionName =="napi_get_value_bigint_uint64"){
        summaryItemResult.addOperand("top");
        const llvm::Value* losslessParam = callInst->getOperand(3);
        NodeID losslessParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(losslessParam);
        int losslessID = taintMap.assignNewId(losslessParamNodeID);
        if (losslessID != -1 && valueptrid != -1){
            taintMap.addValueFlowSource(losslessID, valueptrid);
        }
        summaryItemResult.addRetValue("%"+std::to_string(losslessID), 3);
    }
    
    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiGetValueNumberRegister {
        NapiGetValueNumberRegister() {
            NapiHandler::getInstance().registerHandler("napi_get_value_double", handleNapiGetValueNumber);
            NapiHandler::getInstance().registerHandler("napi_get_value_int32", handleNapiGetValueNumber);
            NapiHandler::getInstance().registerHandler("napi_get_value_int64", handleNapiGetValueNumber);
            NapiHandler::getInstance().registerHandler("napi_get_value_uint32", handleNapiGetValueNumber);
            NapiHandler::getInstance().registerHandler("napi_get_value_bigint_int64", handleNapiGetValueNumber);
            NapiHandler::getInstance().registerHandler("napi_get_value_bigint_uint64", handleNapiGetValueNumber);
        }
    };

    static NapiGetValueNumberRegister _napi_get_value_number_reg;
}