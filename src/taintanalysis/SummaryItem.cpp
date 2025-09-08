#include "taintanalysis/SummaryItem.h"
#include <iostream>

// 构造函数
SummaryItem::SummaryItem(const std::string& funcName, const std::string& instType) 
    : functionName(funcName), instructionType(instType) {}

// 添加返回值节点及其关联值
void SummaryItem::addRetValue(std::string node, int value) {
    retValues.emplace_back(node, value);
}

// 添加操作数节点
void SummaryItem::addOperand(std::string node) {
    operands.push_back(node);
}

// 添加参数操作数节点
void SummaryItem::addArgsOperand(std::string node) {
    argsoperands.push_back(node);
}

// 设置指令类型
void SummaryItem::setInstructionType(const std::string& type) {
    instructionType = type;
}

// 设置函数名
void SummaryItem::setName(const std::string& name) {
    functionName = name;
}

// 获取函数名
std::string SummaryItem::getFunctionName() const {
    return functionName;
}

// 获取返回值节点及其关联值
std::vector<std::pair<std::string, int>> SummaryItem::getRetValues() const {
    return retValues;
}

// 获取操作数节点
std::vector<std::string> SummaryItem::getOperands() const {
    return operands;
}

// 获取参数操作数节点
std::vector<std::string> SummaryItem::getArgsOperands() const {
    return argsoperands;
}

std::string SummaryItem::getOperand(int index) const {
    return operands[index];
}

// 获取指令类型
std::string SummaryItem::getInstructionType() const {
    return instructionType;
}

// 打印信息
void SummaryItem::print() const {
    std::cout << "Function: " << functionName << "\n";
    std::cout << "Instruction Type: " << instructionType << "\n";
    
    std::cout << "Return Values: ";
    for (const auto& [node, value] : retValues) {
        std::cout << "(" << node << ", " << value << ") ";
    }
    std::cout << "\n";
    
    std::cout << "Operands: ";
    for (const auto& node : operands) {
        std::cout << node << " ";
    }
    std::cout << "\n";
}