#ifndef TAINTLIST_H
#define TAINTLIST_H

#include "taintanalysis/TargetUnit.h"
#include "taintanalysis/TaintUnit.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "WPA/Andersen.h"
#include <vector>
#include <map>
#include <set>

class TaintList {
public:
    // 构造函数
    TaintList(SVF::Andersen* ander, const std::vector<TaintUnit>& taintUnits); 

    TaintList(SVF::Andersen* ander, const std::vector<TaintUnit>& taintUnits, std::string function_Name, std::vector<SVF::NodeID> function_ParamNodeIDs);

    // 处理TaintUnit数组，提取napi相关函数
    void processTargetUnits();  // 修改方法名

    // 获取处理后的结果
    std::vector<TargetUnit> getFinalTargetUnits() const;  
    
    unsigned int findOrCreateMapping(SVF::NodeID originalID, unsigned int& newID);

    void handleLoadNodes(const TaintUnit& unit, unsigned int& newID);

    unsigned int handleDirectAssignment(const TaintUnit& unit, unsigned int& newID);

    void handleNapiGetCbInfo(const TaintUnit& unit, unsigned int& newID);

    // 获取函数参数NodeID的Mapid
    std::vector<unsigned int> getFunctionParamNodeID_MapID() const;

    // 获取函数名称
    std::string getFunctionName() const;

private:
    // 判断是否是napi相关函数
    bool isNapiFunction(const std::string& funcName) const;

    // 对NodeID进行重新编号
    void renumberNodeIDs();

    // 检查两个NodeID是否是alias关系
    bool isAlias(SVF::NodeID id1, SVF::NodeID id2) const;

    // 原始TargetUnit数组
    std::vector<TaintUnit> originalTargetUnits;  

    // 处理后的TargetUnit数组
    std::vector<TaintUnit> processedTargetUnits;  

    std::vector<TargetUnit> FinalTargetUnits; 

    // NodeID映射表
    std::map<SVF::NodeID, unsigned int> nodeIDMap; 

    // Andersen指针分析
    SVF::Andersen* ander;

    // 整体函数名称
    std::string function_Name;

    // 整体函数参数NodeID
    std::vector<SVF::NodeID> function_ParamNodeIDs;

    // 函数参数NodeID的Mapid
    std::vector<unsigned int> function_ParamNodeID_MapID;
};

#endif // TAINTLIST_H