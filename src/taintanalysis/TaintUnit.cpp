#include "taintanalysis/TaintUnit.h"
#include "Util/SVFUtil.h"
using namespace SVF;

// 构造函数
TaintUnit::TaintUnit(const std::string& funcName, const std::string& instType)
    : functionName(funcName), instructionType(instType) {}


// 添加新增污点节点
void TaintUnit::addNewTaintedNode(SVF::NodeID node) {
    newTaintedNodes.insert(node);
}

// 添加原有污点节点
void TaintUnit::addOriginalTaintedNode(SVF::NodeID node) {
    originalTaintedNodes.insert(node);
}

// 添加指令参数
void TaintUnit::addInstructionArg(SVF::NodeID arg) {
    instructionArgs.push_back(arg);
}

// 设置指令类型
void TaintUnit::setInstructionType(const std::string& type) {
    instructionType = type;
}

// 设置函数名
void TaintUnit::setName(const std::string& name) {
    functionName = name;
}

// 获取函数名
std::string TaintUnit::getFunctionName() const {
    return functionName;
}

// 获取新增污点节点
std::set<SVF::NodeID> TaintUnit::getNewTaintedNodes() const {
    return newTaintedNodes;
}

// 获取原有污点节点
std::set<SVF::NodeID> TaintUnit::getOriginalTaintedNodes() const {
    return originalTaintedNodes;
}

// 获取指令参数
std::vector<SVF::NodeID> TaintUnit::getInstructionArgs() const {
    return instructionArgs;
}

// 获取指令类型
std::string TaintUnit::getInstructionType() const {
    return instructionType;
}

// 打印信息
void TaintUnit::print() const {
    SVFUtil::outs() << "Function: " << functionName << "\n"
                  << "Instruction Type: " << instructionType << "\n"
                  << "Original Tainted Nodes: ";
    for (auto node : originalTaintedNodes) {
        SVFUtil::outs() << node << " ";
    }
    SVFUtil::outs() << "\nNew Tainted Nodes: ";
    for (auto node : newTaintedNodes) {
        SVFUtil::outs() << node << " ";
    }
    SVFUtil::outs() << "\nInstruction Args: ";
    for (auto arg : instructionArgs) {
        SVFUtil::outs() << arg << " ";
    }
    SVFUtil::outs() << "\n";
}