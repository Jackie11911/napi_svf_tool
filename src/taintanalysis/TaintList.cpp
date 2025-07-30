#include "taintanalysis/TaintList.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/Options.h"
#include "SourceAndSinks/SourceAndSinks.h"

using namespace llvm;
using namespace SVF;

TaintList::TaintList(SVF::Andersen* ander, const std::vector<TaintUnit>& taintUnits)
    : ander(ander), originalTargetUnits(taintUnits) { 
}

TaintList::TaintList(SVF::Andersen* ander, const std::vector<TaintUnit>& taintUnits, std::string function_Name, std::vector<SVF::NodeID> function_ParamNodeIDs)
    : ander(ander), originalTargetUnits(taintUnits), function_Name(function_Name), function_ParamNodeIDs(function_ParamNodeIDs) {
}

void TaintList::processTargetUnits() {
    SVFUtil::outs() << "Starting processTargetUnits\n";
    SourceAndSinks sourceAndSinks;
    for (const auto& unit : originalTargetUnits) {
        // SVFUtil::outs() << "unit: " << unit.getFunctionName() << " " << unit.getInstructionType() << "\n";
        if (isNapiFunction(unit.getFunctionName())) {        
            processedTargetUnits.push_back(unit);
        }
        else if (unit.getInstructionType() == "Load" || unit.getInstructionType() == "DirectAssignment") {
            processedTargetUnits.push_back(unit);
        }
        else if (sourceAndSinks.is_sink_methods(unit.getFunctionName())) {
            processedTargetUnits.push_back(unit);
        }
    }

    //处理最后一个返回指令
    if (!originalTargetUnits.empty()) {
        const TaintUnit& lastUnit = originalTargetUnits.back();
        if (lastUnit.getInstructionType() == "Return") {
            processedTargetUnits.push_back(lastUnit);
        }
    }
    renumberNodeIDs();
}

bool TaintList::isNapiFunction(const std::string& funcName) const {
    return funcName.rfind("napi_", 0) == 0; // 检查字符串起始位置
}

// 处理DirectAssignment
unsigned int TaintList::handleDirectAssignment(const TaintUnit& unit, unsigned int& newID) {
    // 如果getOriginalTaintedNodes大于等于2，则特殊处理
    if (unit.getOriginalTaintedNodes().size() < 2) {
        handleLoadNodes(unit, newID);
        return 0;
    }
    else{
        // 获取newtaintnode
        NodeID newNode = *unit.getNewTaintedNodes().begin();
        // 获取newtaintnode的映射ID
        unsigned int newNodeID = findOrCreateMapping(newNode, newID);
        return newNodeID;
    }
}

void TaintList::handleLoadNodes(const TaintUnit& unit, unsigned int& newID) {
    for (auto origNode : unit.getOriginalTaintedNodes()) {
        // 查找原始节点已有映射
        auto it = nodeIDMap.find(origNode);
        if (it != nodeIDMap.end()) {
            // 将新增节点强制映射到相同ID
            for (auto newNode : unit.getNewTaintedNodes()) {
                nodeIDMap[newNode] = it->second;
            }
        }
        else {
            // 将origNode加入nodeIDMap
            nodeIDMap[origNode] = ++newID;
            for (auto newNode : unit.getNewTaintedNodes()) {
                nodeIDMap[newNode] = newID;
            }
        }
    }
}

void TaintList::handleNapiGetCbInfo(const TaintUnit& unit, unsigned int& newID) {
    // 获取第四个参数（arraydecay的NodeID）
    if (unit.getInstructionArgs().size() >= 4) {
        SVF::NodeID arrayDecayID = unit.getInstructionArgs()[3];
        
        // 获取对应的LLVM Value
        const SVF::SVFValue* svfVal = ander->getPAG()->getGNode(arrayDecayID);
        const Value* val = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(svfVal);
        
        // 检查是否是GEP指令
        if (const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(val)) {
            // 获取基指针（即%args）
            const Value* basePtr = gep->getPointerOperand()->stripPointerCasts();
            SVF::NodeID basePtrID = SVF::LLVMModuleSet::getLLVMModuleSet()->getValueNode(basePtr);
            unsigned int mappedID = findOrCreateMapping(arrayDecayID, newID);
            
            // 建立基指针到映射ID的关联
            nodeIDMap[basePtrID] = mappedID;
            
            SVFUtil::outs() << "[napi_get_cb_info] Mapped base pointer " 
                          << basePtrID << " -> " << mappedID << "\n";
        }
    }
}

void TaintList::renumberNodeIDs() {
    unsigned int newID = 0;
    FinalTargetUnits.clear();

    // 对函数参数优先进行编号
    for (auto paramNodeID : function_ParamNodeIDs) {
        unsigned int mappedID = findOrCreateMapping(paramNodeID, newID);
        nodeIDMap[paramNodeID] = mappedID;
        function_ParamNodeID_MapID.push_back(mappedID);
    }

    for (const auto& taintUnit : processedTargetUnits) {

        // 打印taintUnit
        SVFUtil::outs() << "taintUnit: " << taintUnit.getFunctionName() << " " << taintUnit.getInstructionType() << "\n";


        if (taintUnit.getInstructionType() == "Load") {
            // 打印taintUnit
            // SVFUtil::outs() << "taintUnit: " << taintUnit.getFunctionName() << " " << taintUnit.getInstructionType() << "\n";
            handleLoadNodes(taintUnit, newID);
            continue; // 跳过后续处理
        }

        if (taintUnit.getInstructionType() == "DirectAssignment") {
            handleLoadNodes(taintUnit, newID);
            // unsigned int newNodeID = handleDirectAssignment(taintUnit, newID);
            // if (newNodeID != 0) {
            //     TargetUnit targetUnit("Phi", "Phi");
            //     targetUnit.addNewTargetNode(newNodeID);
            //     for (auto arg : taintUnit.getOriginalTaintedNodes()) {
            //         unsigned int mappedID = findOrCreateMapping(arg, newID);
            //         targetUnit.addOriginalTargetNode(mappedID);
            //     }
            //     FinalTargetUnits.push_back(targetUnit);
            // }
            continue;
        }

        TargetUnit targetUnit(taintUnit.getFunctionName(), taintUnit.getInstructionType());
        
        // 处理指令参数
        std::vector<unsigned int> newArgs;
        for (auto arg : taintUnit.getInstructionArgs()) {
            unsigned int mappedID = findOrCreateMapping(arg, newID);
            newArgs.push_back(mappedID);
        }
        targetUnit.setInstructionArgs(newArgs);

        // 处理原有污点节点
        std::set<unsigned int> newOriginalNodes;
        for (auto node : taintUnit.getOriginalTaintedNodes()) {
            unsigned int mappedID = findOrCreateMapping(node, newID);
            newOriginalNodes.insert(mappedID);
        }
        targetUnit.setOriginalTargetNodes(newOriginalNodes);

        // 处理新增污点节点
        std::set<unsigned int> newTaintedNodes;
        for (auto node : taintUnit.getNewTaintedNodes()) {
            unsigned int mappedID = findOrCreateMapping(node, newID);
            newTaintedNodes.insert(mappedID);
        }
        targetUnit.setNewTargetNodes(newTaintedNodes);

        // 如果taintUnit是napi_get_cb_info，则需要特殊处理
        if (taintUnit.getFunctionName() == "napi_get_cb_info") {
            handleNapiGetCbInfo(taintUnit, newID);
        }

        FinalTargetUnits.push_back(targetUnit);
    }
}

// 新增辅助函数
unsigned int TaintList::findOrCreateMapping(SVF::NodeID originalID, unsigned int& newID) {
    if (originalID == 0) {
        return 0;
    }

    const SVF::SVFValue* svfVal = ander->getPAG()->getGNode(originalID);
    const Value* val = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(svfVal);
    
    if (const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(val)) {
        const Value* basePtr = gep->getPointerOperand()->stripPointerCasts();
        
        for (auto& entry : nodeIDMap) {
            // 对entry.first也做同样转换
            const SVF::SVFValue* existSVFVal = ander->getPAG()->getGNode(entry.first);
            const Value* existVal = SVF::LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(existSVFVal);
            
            if (const GetElementPtrInst* existGep = SVFUtil::dyn_cast<GetElementPtrInst>(existVal)) {
                if (existGep->getPointerOperand()->stripPointerCasts() == basePtr) {
                    return entry.second;
                }
            }
        }
    }

    // 检查现有映射
    for (const auto& [existingID, mappedID] : nodeIDMap) {
        // // 打印existingID
        // SVFUtil::outs() << "existingID: " << existingID << "\n";
        // // 打印originalID
        // SVFUtil::outs() << "originalID: " << originalID << "\n";
        if (existingID == originalID || isAlias(existingID, originalID)) {
            return mappedID;
        }
    }
    
    // 创建新映射
    unsigned int newMapping = ++newID;
    nodeIDMap[originalID] = newMapping;
    return newMapping;
}

bool TaintList::isAlias(SVF::NodeID id1, SVF::NodeID id2) const {
    return ander->alias(id1, id2) == SVF::AliasResult::MayAlias;
}

std::vector<TargetUnit> TaintList::getFinalTargetUnits() const {  // 修改返回类型
    return FinalTargetUnits;
}

std::vector<unsigned int> TaintList::getFunctionParamNodeID_MapID() const {
    return function_ParamNodeID_MapID;
}

std::string TaintList::getFunctionName() const {
    return function_Name;
}