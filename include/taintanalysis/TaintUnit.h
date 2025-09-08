#ifndef TAINTUNIT_H
#define TAINTUNIT_H

#include "SVFIR/SVFIR.h"  
#include <string>
#include <set>
#include <vector>

class TaintUnit {
private:
    std::string functionName;  // 指令所在的函数名
    std::set<SVF::NodeID> newTaintedNodes;  // 新增的污点节点
    std::set<SVF::NodeID> originalTaintedNodes;  // 原有的污点节点
    std::vector<SVF::NodeID> instructionArgs;  // 指令的参数节点
    std::string instructionType;  // 指令类型

public:
    // 构造函数
    TaintUnit(const std::string& funcName, const std::string& instType);
    TaintUnit() = default;

    // 添加新增污点节点
    void addNewTaintedNode(SVF::NodeID node);

    // 添加原有污点节点
    void addOriginalTaintedNode(SVF::NodeID node);

    // 添加指令参数
    void addInstructionArg(SVF::NodeID arg);

    // 设置指令类型
    void setInstructionType(const std::string& type);

    // 设置函数名
    void setName(const std::string& name);

    // 获取函数名
    std::string getFunctionName() const;

    // 获取新增污点节点
    std::set<SVF::NodeID> getNewTaintedNodes() const;

    // 获取原有污点节点
    std::set<SVF::NodeID> getOriginalTaintedNodes() const;

    // 获取指令参数
    std::vector<SVF::NodeID> getInstructionArgs() const;

    // 获取指令类型
    std::string getInstructionType() const;

    // 打印信息
    void print() const;
};

#endif // TAINTUNIT_H