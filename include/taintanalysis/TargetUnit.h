#ifndef TARGETUNIT_H
#define TARGETUNIT_H

#include <string>
#include <set>
#include <vector>

class TargetUnit {
private:
    std::string functionName;  // 指令所在的函数名
    std::set<unsigned int> newTargetNodes;  // 新增的目标节点
    std::set<unsigned int> originalTargetNodes;  // 原有的目标节点
    std::vector<unsigned int> instructionArgs;  // 指令的参数节点
    std::string instructionType;  // 指令类型

public:
    // 构造函数
    TargetUnit(const std::string& funcName, const std::string& instType);
    TargetUnit() = default;

    // 添加新增目标节点
    void addNewTargetNode(unsigned int node);

    // 添加原有目标节点
    void addOriginalTargetNode(unsigned int node);

    // 添加指令参数
    void addInstructionArg(unsigned int arg);

    // 获取函数名
    std::string getFunctionName() const;

    // 获取新增目标节点
    std::set<unsigned int> getNewTargetNodes() const;

    // 获取原有目标节点
    std::set<unsigned int> getOriginalTargetNodes() const;

    // 获取指令参数
    std::vector<unsigned int> getInstructionArgs() const;

    // 获取指令类型
    std::string getInstructionType() const;

    void setInstructionArgs(const std::vector<unsigned int>& args);
    void setOriginalTargetNodes(const std::set<unsigned int>& nodes);
    void setNewTargetNodes(const std::set<unsigned int>& nodes);

    // 打印信息
    void print() const;
};

#endif // TARGETUNIT_H