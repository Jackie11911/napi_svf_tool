#include "napi/NapiHandler.h"

using namespace SVF;


NapiHandler& NapiHandler::getInstance() {
    static NapiHandler instance;
    return instance;
}

void NapiHandler::registerHandler(const std::string& name, HandlerFunc func) {
    handlerMap[name] = func;
}

void NapiHandler::dispatch(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems) {
    if (!llvm::isa<llvm::CallInst>(inst)) return;

    const llvm::CallBase* callInst = llvm::dyn_cast<llvm::CallBase>(inst);
    if (!callInst) return;

    const llvm::Function* callee = LLVMUtil::getCallee(callInst);
    if (!callee) return;

    const std::string& calleeName = callee->getName().str();
    auto it = handlerMap.find(calleeName);
    if (it != handlerMap.end()) {
        it->second(inst, taintMap, svfg, pag, ander, summaryItems);  // 调用注册的处理函数
    }
    return;
}