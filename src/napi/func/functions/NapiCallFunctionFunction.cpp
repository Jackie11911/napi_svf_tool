#include "napi/NapiHandler.h"
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include <iostream>
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"
#include "napi/utils/ParseVFG.h"
#include <sstream>

using namespace SVF;

//NAPI_EXTERN napi_status napi_call_function(napi_env env,
//                                           napi_value recv,
//                                           napi_value func,
//                                           size_t argc,
//                                           const napi_value* argv,
//                                           napi_value* result);

void handleNapiCallFunction(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return;
    inst->print(llvm::outs());

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
    callInst->print(llvm::outs());

    const llvm::Value* envParam = callInst->getArgOperand(0);
    envParam->print(llvm::outs());
    NodeID envParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(envParam);
    int envID = taintMap.getParamIdByIndex(0); 
    if (envID == -1) {
        envID = taintMap.assignNewId(envParamNodeID);
    }
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

    for(int i = 1; i < 4; i++) {
        const llvm::Value* param = callInst->getArgOperand(i);
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
            if(i == 2){
                funcID = taintMap.getNewIds(paramNodeID)[0];
            }
        }
    }
    // 从summaryItemResult获取第四个参数的int值
    std::string param4 = summaryItemResult.getOperand(3);
    int param4Int = 0;
    bool isInt = false;
    if (param4.substr(0, 5) == "long ") {
        std::stringstream ss(param4.substr(5));
        ss >> param4Int;
        isInt = !ss.fail();
    }
    
    // 处理第五个参数
    summaryItemResult.addOperand("top");
    if(isInt){
        const llvm::Value* argvParam = callInst->getArgOperand(4);
        NodeID argvParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argvParam);
        const SVFVar* argvParamSVFVar = pag->getGNode(argvParamNodeID);
        // 如果argv不是null
        if(argvParamNodeID != 0){
            // 打印argvParamNodeID
            std::cout << "argvParamNodeID: " << argvParamNodeID << std::endl;
            parseArgsOperand(svfg, pag, argvParamNodeID, argvParamSVFVar, taintMap, summaryItems, summaryItemResult, param4Int, ander);
        }  
    }
    // 获取第六个参数
    const llvm::Value* resultParam = callInst->getArgOperand(5);
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