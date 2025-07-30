#include "napi/NapiHandler.h"
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include <iostream>
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"


using namespace SVF;

// napi_status napi_get_cb_info(napi_env env,
//                      napi_callback_info cbinfo,
//                      size_t* argc,
//                      napi_value* argv,
//                      napi_value* thisArg,
//                      void** data)

int parseArgcValue(SVFVar* argcSVFVar, const SVFG* svfg, const SVFIR* pag) {
    if (!svfg->hasDefSVFGNode(argcSVFVar)){
        return -1;
    }
    
    const VFGNode* argcVNode = svfg->getDefSVFGNode(argcSVFVar);
    if (!argcVNode) {
        return -1;
    }
    // 遍历 argcNode 的出边
    for (auto it = argcVNode->OutEdgeBegin(); it != argcVNode->OutEdgeEnd(); ++it) {
        SVFGEdge* edge = *it;
        const VFGNode* dstNode = edge->getDstNode();

        // 检查源节点是否为 StoreVFGNode
        if (const StoreVFGNode* storeNode = SVFUtil::dyn_cast<StoreVFGNode>(dstNode)) {
            // 获取存储的值
            const SVFVar* storedValue = storeNode->getPAGSrcNode();
            if (const ConstIntValVar* constIntVar = SVFUtil::dyn_cast<ConstIntValVar>(storedValue)) {
                // 获取常量整数值（有符号扩展值）
                s64_t intValue = constIntVar->getSExtValue();
                std::cout << "The value of argc is: " << intValue << std::endl;
                return intValue;
            }
        }
    }

    std::cout << "Failed to find the value of argc." << std::endl;

    return -1;
}

NodeID parseArgvValue(SVFVar* argvSVFVar, const SVFG* svfg, const SVFIR* pag) {
    if(!svfg->hasDefSVFGNode(argvSVFVar)){
        return -1;
    }
    const VFGNode* argvVNode = svfg->getDefSVFGNode(argvSVFVar);
    if(!argvVNode){
        return -1;
    }
    if (const GepVFGNode* gepNode = SVFUtil::dyn_cast<GepVFGNode>(argvVNode)){
        const SVFVar* gepVar = gepNode->getPAGSrcNode();
        NodeID gepNodeID = gepVar->getId();
        return gepNodeID;
    }
    return -1;
}

void handleNapiGetCbInfo(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    
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
    int envID = taintMap.getParamIdByIndex(0); // 直接使用第一个参数的paramId
    if (envID == -1) {
        envID = taintMap.assignNewId(envParamNodeID);
    }
    summaryItemResult.addOperand("%"+std::to_string(envID));

    // 获取第二个参数
    const llvm::Value* cbInfoParam = callInst->getOperand(1);
    NodeID cbInfoParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(cbInfoParam);
    int cbInfoID = taintMap.getParamIdByIndex(1); // 直接使用第二个参数的paramId
    if (cbInfoID == -1) {
        cbInfoID = taintMap.assignNewId(cbInfoParamNodeID);
    }
    summaryItemResult.addOperand("%"+std::to_string(cbInfoID));

    // 获取第三个参数 argc，并解析其参数值
    const llvm::Value* argc = callInst->getOperand(2);
    NodeID argcNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argc);
    int argcValue = -1;
    if(argcNodeID != 0){
        SVFVar* argcSVFVar = pag->getGNode(argcNodeID);
        argcValue = parseArgcValue(argcSVFVar, svfg, pag);
        if(argcValue == -1){
            summaryItemResult.addOperand("top");
        }
        else{
            summaryItemResult.addOperand("long "+std::to_string(argcValue));
        }
    }
    for(int i = 0;i < 3;i++){
        summaryItemResult.addOperand("top");
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

    // 获取第四个参数argv，解析其数组内容
    const llvm::Value* argv = callInst->getOperand(3);
    // 获取argv的SVF表示
    NodeID argvNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argv);
    if(argvNodeID != 0){
        SVFVar* argvSVFVar = pag->getGNode(argvNodeID);
        NodeID argvValue = parseArgvValue(argvSVFVar, svfg, pag);
        taintMap.assignArrayNewIds(argvValue, argcValue);
        taintMap.setArrayNewIds(argvNodeID, taintMap.getNewIds(argvValue));
        for(int i = 0;i < taintMap.getNewIds(argvValue).size();i++){
            int newID = taintMap.getNewIds(argvValue)[i];
            taintMap.addValueFlowSource(newID, cbInfoID);
            summaryItemResult.addRetValue("%"+std::to_string(newID), 3);
        }
    }

    // 第五个参数（如果第五个参数为null，则不处理）
    const llvm::Value* thisArg = callInst->getOperand(4);
    if (thisArg){
        NodeID thisArgNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(thisArg);
        if(thisArgNodeID != 0){
            int thisArgID = taintMap.assignNewId(thisArgNodeID);
            taintMap.addValueFlowSource(thisArgID, cbInfoID);
            summaryItemResult.addRetValue("%"+std::to_string(thisArgID), 4);
        }
    }

    // 第六个参数
    const llvm::Value* data = callInst->getOperand(5);
    if (data){
        NodeID dataNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(data);
        if(dataNodeID != 0){
            int dataID = taintMap.assignNewId(dataNodeID);
            taintMap.addValueFlowSource(dataID, cbInfoID);
            summaryItemResult.addRetValue("%"+std::to_string(dataID), 5);
        }
    }
    summaryItems.push_back(summaryItemResult);

    return;
}

// 注册处理器
namespace {
    struct NapiGetCbInfoRegister {
        NapiGetCbInfoRegister() {
            NapiHandler::getInstance().registerHandler("napi_get_cb_info", handleNapiGetCbInfo);
        }
    };

    static NapiGetCbInfoRegister _napi_get_cb_info_reg;
}