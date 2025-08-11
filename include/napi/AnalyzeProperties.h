#ifndef ANALYZE_PROPERTIES_H
#define ANALYZE_PROPERTIES_H

#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include <set>
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"
class NapiPropertiesAnalyzer {
public:
    static std::set<llvm::GlobalVariable*> analyzeNapiProperties(SVF::SVFG* svfg,SVF::SVFIR* pag);
    static std::map<std::string, llvm::Function*> analyzeGlobalVars(std::set<llvm::GlobalVariable*> globalVars);
    // 处理通过napi_set_named_property注册的函数
    static std::map<std::string, llvm::Function*> analyzeNamedProperties(SVF::SVFG* svfg, SVF::SVFIR* pag, SVF::PointerAnalysis* pta);
    // 检查两个值是否可能是别名（通过分析它们的定义位置和加载来源）
    static bool checkPotentialAlias(SVF::NodeID nodeID1, SVF::NodeID nodeID2, SVF::SVFG* svfg);
    // 从NodeID获取定义的SVFGNode
    static const SVF::SVFGNode* getDefSVFGNodeFromNodeID(SVF::NodeID nodeID, SVF::SVFG* svfg);

private:
    // 查找模块注册函数
    static std::map<llvm::GlobalVariable*, const llvm::Function*> findModuleInitFunctions(llvm::Module* llvmModule);
    
    // 分析napi_set_named_property调用与初始化函数返回值的关系
    static void analyzeSetNamedPropertyCalls(
        llvm::Module* llvmModule, 
        const llvm::Function* initFunc, 
        SVF::NodeID retNodeId,
        SVF::SVFIR* pag, 
        SVF::PointerAnalysis* pta,
        SVF::SVFG* svfg,
        std::map<std::string, llvm::Function*>& functions);
    
    // 查找属性值与napi_create_function结果指针之间的关系
    static llvm::Function* findCallbackForValue(
        llvm::Value* valueVal, 
        SVF::NodeID valueNodeId, 
        const llvm::Function* initFunc, 
        SVF::SVFIR* pag, 
        SVF::PointerAnalysis* pta,
        SVF::SVFG* svfg);
};

#endif // ANALYZE_PROPERTIES_H