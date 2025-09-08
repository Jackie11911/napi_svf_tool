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

// napi_status napi_get_value_string_utf8(napi_env env,
//                                       napi_value value,
//                                       char* buf,
//                                       size_t bufsize,
//                                       size_t* result);


//NAPI_EXTERN napi_status napi_get_value_string_latin1(napi_env env,
//                                                     napi_value value,
//                                                     char* buf,
//                                                     size_t bufsize,
//                                                     size_t* result);

//NAPI_EXTERN napi_status napi_get_value_string_utf16(napi_env env,
//                                                    napi_value value,
//                                                    char16_t* buf,
//                                                    size_t bufsize,
//                                                    size_t* result);

void handleNapiGetValueStringFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_get_value_string" << std::endl;
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
    
    // 处理返回值
    const llvm::Value* returnValue = callInst;
    if (returnValue){
        NodeID returnValueNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(returnValue);
        if(returnValueNodeID != 0){
            int returnValueID = taintMap.assignNewId(returnValueNodeID);
            summaryItemResult.addRetValue("%"+std::to_string(returnValueID), -1);
        }
    }

    // 处理第二个参数
    const llvm::Value* valueParam = callInst->getOperand(1);
    NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
    handleTaintFlow(svfg, pag, valueParam, taintMap, summaryItems, summaryItemResult, ander);
    int valueID = taintMap.getNewIds(valueParamNodeID)[0];

    // 处理第三个参数
    const llvm::Value* bufParam = callInst->getOperand(2);
    NodeID bufParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(bufParam);
    const SVFVar* bufSVFVar = pag->getGNode(bufParamNodeID);
    // 如果为常量
    if (llvm::isa<llvm::Constant>(bufParam)){
        std::string bufConstant = parseConstant(bufParam);
        if(bufConstant == ""){
            bufConstant = "null";
        }
        summaryItemResult.addOperand(bufConstant);
    }
    else{
        handleTaintFlow(svfg, pag, bufParam, taintMap, summaryItems, summaryItemResult, ander);
    }
    
    // 处理第四个参数（常量）
    const llvm::Value* bufsizeParam = callInst->getOperand(3);
    NodeID bufsizeParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(bufsizeParam);
    const SVFVar* bufsizeSVFVar = pag->getGNode(bufsizeParamNodeID);
    if (llvm::isa<llvm::Constant>(bufsizeParam)){
        std::string bufsizeConstant = parseConstant(bufsizeParam);
        if(bufsizeConstant == ""){
            bufsizeConstant = "null";
        }
        summaryItemResult.addOperand(bufsizeConstant);
    }
    else{
        handleTaintFlow(svfg, pag, bufsizeParam, taintMap, summaryItems, summaryItemResult, ander);
    }

    // 处理第五个参数
    const llvm::Value* resultParam = callInst->getOperand(4);
    NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
    const SVFVar* resultSVFVar = pag->getGNode(resultParamNodeID);
    summaryItemResult.addOperand("top");
    int resultID = taintMap.assignNewId(resultParamNodeID);
    summaryItemResult.addRetValue("%"+std::to_string(resultID), 4);
    taintMap.addValueFlowSource(resultID, valueID);
    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiGetValueStringRegister {
        NapiGetValueStringRegister() {
            NapiHandler::getInstance().registerHandler("napi_get_value_string_utf8", handleNapiGetValueStringFunction);
            NapiHandler::getInstance().registerHandler("napi_get_value_string_latin1", handleNapiGetValueStringFunction);
            NapiHandler::getInstance().registerHandler("napi_get_value_string_utf16", handleNapiGetValueStringFunction);
        }
    };

    static NapiGetValueStringRegister _napi_get_value_string_reg;
}