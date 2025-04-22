#ifndef CUSTOM_TAINT_ANALYSIS_H
#define CUSTOM_TAINT_ANALYSIS_H

#include "taintanalysis/TaintTracker.h"
#include "napi/NapiPropagationRules.h"
#include <unordered_map>

class CustomTaintAnalysis {
public:

    // 判断callee是否是rules中定义的函数
    static bool isNapiFunction(const llvm::Function* callee);

    // 处理自定义函数的污点传播
    static std::unordered_map<SVF::NodeID, std::vector<SVF::NodeID>> handleNapiCall(const llvm::CallInst* callInst, SVF::PAG* pag, std::set<SVF::NodeID>& taintedNodes);

    // 处理napi_get_cb_info
    static std::vector<SVF::NodeID> handleNapiGetCbInfo(const llvm::CallInst* callInst, SVF::PAG* pag);

    static bool isTainted(SVF::NodeID id, std::set<SVF::NodeID>& taintedNodes);

    // 处理GEP对象（堆/全局数组）
    static void processGepObj(const SVF::GepObjVar* gepObj, const llvm::CallInst* callInst, 
                               SVF::PAG* pag, std::vector<SVF::NodeID>& taintedArgs);

    // 处理DummyObjVar对象
    static void processDummyObj(const SVF::DummyObjVar* dummyObj, const llvm::CallInst* callInst, 
                                SVF::PAG* pag, std::vector<SVF::NodeID>& taintedArgs);  

    // 调试输出辅助函数
    static void logTaintedElement(int index, SVF::NodeID nodeId, const llvm::Value* value);

    static void processStackObj(const SVF::StackObjVar* stackObj, const llvm::CallInst* callInst,
                                SVF::PAG* pag, std::vector<SVF::NodeID>& taintedArgs);


private:
    
};

#endif // CUSTOM_TAINT_ANALYSIS_H