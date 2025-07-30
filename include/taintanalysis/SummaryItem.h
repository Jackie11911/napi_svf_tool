#ifndef SUMMARYITEM_H
#define SUMMARYITEM_H

#include "SVFIR/SVFIR.h"  
#include <string>
#include <vector>
#include <utility>

class SummaryItem {
private:
    std::string functionName;  // 指令所在的函数名
    std::vector<std::pair<std::string, int>> retValues;   // 返回值节点
    std::vector<std::string> operands;  // 操作数节点
    std::vector<std::string> argsoperands;
    std::string instructionType;  // 指令类型

public:
    // 构造函数
    SummaryItem(const std::string& funcName, const std::string& instType);
    SummaryItem() = default;

    // 添加返回值节点
    void addRetValue(std::string node, int value);

    // 添加操作数节点
    void addOperand(std::string node);

    // 添加参数操作数节点
    void addArgsOperand(std::string node);

    // 设置指令类型
    void setInstructionType(const std::string& type);

    // 设置函数名
    void setName(const std::string& name);

    // 获取函数名
    std::string getFunctionName() const;

    // 获取返回值节点
    std::vector<std::pair<std::string, int>> getRetValues() const;

    // 获取操作数节点
    std::vector<std::string> getOperands() const;

    // 获取参数操作数节点
    std::vector<std::string> getArgsOperands() const;

    // 获取指定索引的操作数
    std::string getOperand(int index) const;

    // 获取指令类型
    std::string getInstructionType() const;

    // 打印信息
    void print() const;
};

#endif // SUMMARYITEM_H