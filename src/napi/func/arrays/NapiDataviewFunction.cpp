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

//NAPI_EXTERN napi_status napi_create_dataview(napi_env env,
//                                             size_t length,
//                                             napi_value arraybuffer,
//                                             size_t byte_offset,
//                                             napi_value* result);

//NAPI_EXTERN napi_status napi_is_dataview(napi_env env,
//                                         napi_value value,
//                                         bool* result);

//NAPI_EXTERN napi_status napi_get_dataview_info(napi_env env,
//                                               napi_value dataview,
//                                               size_t* bytelength,
//                                               void** data,
//                                               napi_value* arraybuffer,
//                                               size_t* byte_offset);

void handleNapiDataviewFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_dataview" << std::endl;
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

    if(calledFunctionName == "napi_create_dataview") {
        // 获取第二个参数
        const llvm::Value* lengthParam = callInst->getOperand(1);
        if(llvm::isa<llvm::Constant>(lengthParam)){
            std::string lengthConstant = parseConstant(lengthParam);
            if(lengthConstant == ""){
                lengthConstant = "null";
            }
            summaryItemResult.addOperand(lengthConstant);
        }
        else{
            summaryItemResult.addOperand("top");
        }

        // 第三个参数
        const llvm::Value* arraybufferParam = callInst->getOperand(2);
        NodeID arraybufferParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arraybufferParam);
        handleTaintFlow(svfg, pag, arraybufferParam, taintMap, summaryItems, summaryItemResult, ander);
        int arraybufferID = taintMap.assignNewId(arraybufferParamNodeID);

        // 第四个参数
        const llvm::Value* byteOffsetParam = callInst->getOperand(3);
        if(llvm::isa<llvm::Constant>(byteOffsetParam)){
            std::string byteOffsetConstant = parseConstant(byteOffsetParam);
            if(byteOffsetConstant == ""){
                byteOffsetConstant = "null";
            }
            summaryItemResult.addOperand(byteOffsetConstant);
        }
        else{
            summaryItemResult.addOperand("top");
        }

        // 第五个参数
        const llvm::Value* resultParam = callInst->getOperand(4);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 4);
        taintMap.addValueFlowSource(resultID, arraybufferID);
    }
    else if(calledFunctionName == "napi_is_dataview"){
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
    else if(calledFunctionName == "napi_get_dataview_info"){
        // 获取第二个参数
        const llvm::Value* dataviewParam = callInst->getOperand(1);
        NodeID dataviewParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(dataviewParam);
        handleTaintFlow(svfg, pag, dataviewParam, taintMap, summaryItems, summaryItemResult, ander);
        int dataviewID = taintMap.assignNewId(dataviewParamNodeID);
        // 获取第三个参数
        const llvm::Value* byte_lengthParam = callInst->getOperand(2);
        NodeID byte_lengthParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(byte_lengthParam);
        int byte_lengthID = taintMap.assignNewId(byte_lengthParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(byte_lengthID), 2);
        taintMap.addValueFlowSource(byte_lengthID, dataviewID);

        // 获取第四个参数
        const llvm::Value* dataParam = callInst->getOperand(3);
        NodeID dataParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(dataParam);
        int dataID = taintMap.assignNewId(dataParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(dataID), 3);
        taintMap.addValueFlowSource(dataID, dataviewID);

        // 获取第五个参数
        const llvm::Value* arraybufferParam = callInst->getOperand(4);
        NodeID arraybufferParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arraybufferParam);
        int arraybufferID = taintMap.assignNewId(arraybufferParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(arraybufferID), 4);
        taintMap.addValueFlowSource(arraybufferID, dataviewID);

        // 获取第六个参数
        const llvm::Value* byte_offsetParam = callInst->getOperand(5);
        NodeID byte_offsetParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(byte_offsetParam);
        int byte_offsetID = taintMap.assignNewId(byte_offsetParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(byte_offsetID), 5);
        taintMap.addValueFlowSource(byte_offsetID, dataviewID);
    }

    summaryItems.push_back(summaryItemResult);
    return;
}

// 注册处理器
namespace {
    struct NapiDataviewRegister {
        NapiDataviewRegister() {
            NapiHandler::getInstance().registerHandler("napi_create_dataview", handleNapiDataviewFunction);
            NapiHandler::getInstance().registerHandler("napi_is_dataview", handleNapiDataviewFunction);
            NapiHandler::getInstance().registerHandler("napi_get_dataview_info", handleNapiDataviewFunction);
        }
    };

    static NapiDataviewRegister _napi_dataview_reg;
}