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

// NAPI_EXTERN napi_status napi_create_buffer(napi_env env,
//                                           size_t length,
//                                           void** data,
//                                           napi_value* result);
 //	创建并获取一个指定大小的JS Buffer。

//NAPI_EXTERN napi_status napi_create_buffer_copy(napi_env env,
//                                                size_t length,
//                                                const void* data,
//                                                void** result_data,
//                                                napi_value* result);
//	创建并获取一个指定大小的JS Buffer，并以给定数据进行初始化。

//NAPI_EXTERN napi_status napi_create_external_buffer(napi_env env,
//                                                    size_t length,
//                                                    void* data,
//                                                    napi_finalize finalize_cb,
//                                                    void* finalize_hint,  // TODO 这个回调怎么处理。。。
//                                                    napi_value* result);
//	创建并获取一个指定大小的JS Buffer，并以给定数据进行初始化，该接口可为Buffer附带额外数据。

//NAPI_EXTERN napi_status napi_get_buffer_info(napi_env env,
//                                             napi_value value,
//                                             void** data,
//                                             size_t* length);
 //	获取JS Buffer底层data及其长度。

// NAPI_EXTERN napi_status napi_is_buffer(napi_env env,
//                                       napi_value value,
//                                       bool* result);
//	判断给定JS value是否为Buffer对象。


void handleNapiCreateBufferFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    std::cout << "napi_create_buffer" << std::endl;
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

    if(calledFunctionName == "napi_create_buffer"){
        // 获取第二个参数
        const llvm::Value* lengthParam = callInst->getOperand(1);
        if (llvm::isa<llvm::Constant>(lengthParam)){
            std::string lengthConstant = parseConstant(lengthParam);
            if(lengthConstant == ""){
                lengthConstant = "null";
            }
            summaryItemResult.addOperand(lengthConstant);
        }
        else{
           summaryItemResult.addOperand("top");
        }

        // 获取第三个参数
        const llvm::Value* dataParam = callInst->getOperand(2);
        NodeID dataParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(dataParam);
        int dataID = taintMap.assignNewId(dataParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(dataID), 2);

        // 获取第四个参数
        const llvm::Value* resultParam = callInst->getOperand(3);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 3);
    }
    else if(calledFunctionName == "napi_create_buffer_copy"){
        // 获取第二个参数
        const llvm::Value* lengthParam = callInst->getOperand(1);
        if (llvm::isa<llvm::Constant>(lengthParam)){
            std::string lengthConstant = parseConstant(lengthParam);
            if(lengthConstant == ""){
                lengthConstant = "null";
            }
            summaryItemResult.addOperand(lengthConstant);
        }
        else{
            summaryItemResult.addOperand("top");
        }

        // 获取第三个参数
        const llvm::Value* dataParam = callInst->getOperand(2);
        NodeID dataParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(dataParam);
        handleTaintFlow(svfg, pag, dataParam, taintMap, summaryItems, summaryItemResult, ander);
        int dataID = taintMap.assignNewId(dataParamNodeID);

        // 获取第四个参数
        const llvm::Value* resultParam = callInst->getOperand(3);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 3);
        taintMap.addValueFlowSource(resultID, dataID);

        // 获取第五个参数
        const llvm::Value* resultdataParam = callInst->getOperand(4);
        NodeID resultdataParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultdataParam);
        int resultdataID = taintMap.assignNewId(resultdataParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultdataID), 4);
        taintMap.addValueFlowSource(resultdataID, dataID);    
    }
    else if(calledFunctionName == "napi_create_external_buffer"){
        int dataParamID = 0;
        for(int i = 1; i< 5; i++){
            const llvm::Value* param = callInst->getOperand(i);
            if (llvm::isa<llvm::Constant>(param)){
                std::string paramConstant = parseConstant(param);
                if(paramConstant == ""){
                    paramConstant = "null";
                }
                summaryItemResult.addOperand(paramConstant);
            }
            else{
                NodeID paramNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(param);
                handleTaintFlow(svfg, pag, param, taintMap, summaryItems, summaryItemResult, ander);
                if(i == 2){
                    dataParamID = taintMap.assignNewId(paramNodeID);
                }
            }
        }
        // 获取第五个参数
        const llvm::Value* resultParam = callInst->getOperand(5);
        NodeID resultParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultParam);
        int resultID = taintMap.assignNewId(resultParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(resultID), 5);
        taintMap.addValueFlowSource(resultID, dataParamID);
    }
    else if(calledFunctionName == "napi_get_buffer_info"){
        // 获取第二个参数
        const llvm::Value* bufferParam = callInst->getOperand(1);
        NodeID bufferParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(bufferParam);
        handleTaintFlow(svfg, pag, bufferParam, taintMap, summaryItems, summaryItemResult, ander);
        int bufferID = taintMap.assignNewId(bufferParamNodeID);

        // 获取第三个参数
        const llvm::Value* dataParam = callInst->getOperand(2);
        NodeID dataParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(dataParam);
        int dataID = taintMap.assignNewId(dataParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(dataID), 2);
        taintMap.addValueFlowSource(dataID, bufferID);
        // 获取第四个参数
        const llvm::Value* lengthParam = callInst->getOperand(3);
        NodeID lengthParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(lengthParam);
        int lengthID = taintMap.assignNewId(lengthParamNodeID);
        summaryItemResult.addOperand("top");
        summaryItemResult.addRetValue("%"+std::to_string(lengthID), 3);
        taintMap.addValueFlowSource(lengthID, bufferID);
    }
    else if(calledFunctionName == "napi_is_buffer"){
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
    summaryItems.push_back(summaryItemResult);
    return;
}


namespace {
    struct NapiBufferRegister {
        NapiBufferRegister() {
            NapiHandler::getInstance().registerHandler("napi_create_buffer", handleNapiCreateBufferFunction);
            NapiHandler::getInstance().registerHandler("napi_create_buffer_copy", handleNapiCreateBufferFunction);
            NapiHandler::getInstance().registerHandler("napi_create_external_buffer", handleNapiCreateBufferFunction);
            NapiHandler::getInstance().registerHandler("napi_get_buffer_info", handleNapiCreateBufferFunction);
            NapiHandler::getInstance().registerHandler("napi_is_buffer", handleNapiCreateBufferFunction);
        }
    };

    static NapiBufferRegister _napi_buffer_reg;
}