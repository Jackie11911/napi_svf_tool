#include "taintanalysis/TargetUnit.h"
#include "Util/SVFUtil.h"
using namespace SVF;

// 构造函数
TargetUnit::TargetUnit(const std::string& funcName, const std::string& instType)
    : functionName(funcName), instructionType(instType) {}

// 添加新增目标节点
void TargetUnit::addNewTargetNode(unsigned int node) {
    newTargetNodes.insert(node);
}

// 添加原有目标节点
void TargetUnit::addOriginalTargetNode(unsigned int node) {
    originalTargetNodes.insert(node);
}

// 添加指令参数
void TargetUnit::addInstructionArg(unsigned int arg) {
    instructionArgs.push_back(arg);
}

void TargetUnit::setInstructionArgs(const std::vector<unsigned int>& args) {
    instructionArgs = args;
}

void TargetUnit::setOriginalTargetNodes(const std::set<unsigned int>& nodes) {
    originalTargetNodes = nodes;
}

void TargetUnit::setNewTargetNodes(const std::set<unsigned int>& nodes) {
    newTargetNodes = nodes;
}

// 获取函数名
std::string TargetUnit::getFunctionName() const {
    return functionName;
}

// 获取新增目标节点
std::set<unsigned int> TargetUnit::getNewTargetNodes() const {
    return newTargetNodes;
}

// 获取原有目标节点
std::set<unsigned int> TargetUnit::getOriginalTargetNodes() const {
    return originalTargetNodes;
}

// 获取指令参数
std::vector<unsigned int> TargetUnit::getInstructionArgs() const {
    return instructionArgs;
}

// 获取指令类型
std::string TargetUnit::getInstructionType() const {
    return instructionType;
}

// 打印信息
void TargetUnit::print() const {
    SVFUtil::outs() << "Function: " << functionName << "\n"
                  << "Instruction Type: " << instructionType << "\n"
                  << "Original Target Nodes: ";
    for (auto node : originalTargetNodes) {
        SVFUtil::outs() << node << " ";
    }
    SVFUtil::outs() << "\nNew Target Nodes: ";
    for (auto node : newTargetNodes) {
        SVFUtil::outs() << node << " ";
    }
    SVFUtil::outs() << "\nInstruction Args: ";
    for (auto arg : instructionArgs) {
        SVFUtil::outs() << arg << " ";
    }
    SVFUtil::outs() << "\n";
}