#ifndef NAPI_HANDLER_H
#define NAPI_HANDLER_H

#include <string>
#include <unordered_map>
#include <functional>
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "WPA/Andersen.h"
#include "taintanalysis/TaintMap.h"
#include "taintanalysis/SummaryItem.h"
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"

using namespace SVF;

class NapiHandler {
public:
    using HandlerFunc = std::function<void(const llvm::Instruction*, TaintMap&, const SVFG*, const SVFIR*, SVF::Andersen*, std::vector<SummaryItem>&)>;

    static NapiHandler& getInstance();

    void dispatch(const llvm::Instruction* inst, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, SVF::Andersen* ander, std::vector<SummaryItem>& summaryItems);
    void registerHandler(const std::string& name, HandlerFunc func);

private:
    std::unordered_map<std::string, HandlerFunc> handlerMap;

    NapiHandler() = default; 
    NapiHandler(const NapiHandler&) = delete;
    NapiHandler& operator=(const NapiHandler&) = delete;
};

#endif // NAPI_HANDLER_H
