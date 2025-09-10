#include "napi/utils/ParseVFG.h"
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include <iostream>
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"
#include "MemoryModel/AccessPath.h"
#include <queue>
#include <set>
using namespace SVF;

std::pair<NodeID, int> parseLoadVFG(SVFVar* loadSVFVar, const SVFG* svfg, const SVFIR* pag) {
    if(!svfg->hasDefSVFGNode(loadSVFVar)){
        return std::make_pair(-1, -1);
    }
    const VFGNode* loadVNode = svfg->getDefSVFGNode(loadSVFVar);
    std::cout << "[Debug] loadSVFVar ID: " << loadSVFVar->getId() << "\n";
    std::cout << "[Debug] loadVNode: " << (loadVNode ? loadVNode->toString() : "nullptr") << "\n";
    if(!loadVNode){
        return std::make_pair(-1, -1);
    }
    if (const LoadVFGNode* loadVFGNode = SVFUtil::dyn_cast<LoadVFGNode>(loadVNode)){
        const SVFVar* gepVar = loadVFGNode->getPAGSrcNode();
        if (svfg->hasDefSVFGNode(gepVar)){
            const VFGNode* gepVNode = svfg->getDefSVFGNode(gepVar);
            if (const GepVFGNode* gepVFGNode = SVFUtil::dyn_cast<GepVFGNode>(gepVNode)){
                int elementIndex = -1;
                
                if (const GepStmt* gepStmt = SVFUtil::dyn_cast<GepStmt>(gepVFGNode->getPAGEdge())) {
                    // 打印gepstmt
                    std::cout << "gepStmt: " << gepStmt->toString() << std::endl;

                    const AccessPath& ap = gepStmt->getAccessPath();

                    // 检查是否为常量偏移
                    if (ap.isConstantOffset()) {
                        const AccessPath::IdxOperandPairs& offsetVarAndGepTypePairVec = gepStmt->getOffsetVarAndGepTypePairVec();
                        const SVFVar* lastOffsetVar = offsetVarAndGepTypePairVec.back().first;
                        if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(lastOffsetVar)) {
                            // 获取常量值
                            elementIndex = static_cast<int>(constInt->getSExtValue());
                        } 
                        std::cout << "The GepVFGNode points to element index: " << elementIndex << std::endl;
                    } else {
                        std::cout << "The GepVFGNode does not have a constant offset." << std::endl;
                    }
                }
                const SVFVar* ptrVar = gepVFGNode->getPAGSrcNode();
                NodeID ptrNodeID = ptrVar->getId();
                return std::make_pair(ptrNodeID, elementIndex);
            }
        }
        
    }
    return std::make_pair(-1, -1);
}

std::pair<NodeID, int> parseGepVFG(const SVFG* svfg, const SVFIR* pag, const SVFVar* gepSVFVar) {
    if(!svfg->hasDefSVFGNode(gepSVFVar)){
        return std::make_pair(-1, -1);
    }
    const VFGNode* gepVNode = svfg->getDefSVFGNode(gepSVFVar);

    // 打印调试信息
    std::cout << "[Debug] gepSVFVar ID: " << gepSVFVar->getId() << "\n";
    std::cout << "[Debug] gepVNode: " << (gepVNode ? gepVNode->toString() : "nullptr") << "\n";

    if(const GepVFGNode* gepVFGNode = SVFUtil::dyn_cast<GepVFGNode>(gepVNode)){
        int elementIndex = -1;
        if (const GepStmt* gepStmt = SVFUtil::dyn_cast<GepStmt>(gepVFGNode->getPAGEdge())) {
            // 打印gepstmt
            std::cout << "gepStmt: " << gepStmt->toString() << std::endl;

            const AccessPath& ap = gepStmt->getAccessPath();

            // 检查是否为常量偏移
            if (ap.isConstantOffset()) {
                const AccessPath::IdxOperandPairs& offsetVarAndGepTypePairVec = gepStmt->getOffsetVarAndGepTypePairVec();
                const SVFVar* lastOffsetVar = offsetVarAndGepTypePairVec.back().first;
                if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(lastOffsetVar)) {
                    // 获取常量值
                    elementIndex = static_cast<int>(constInt->getSExtValue());
                } 
                std::cout << "The GepVFGNode points to element index: " << elementIndex << std::endl;
            } else {
                std::cout << "The GepVFGNode does not have a constant offset." << std::endl;
            }
        }
        const SVFVar* ptrVar = gepVFGNode->getPAGSrcNode();
        NodeID ptrNodeID = ptrVar->getId();
        return std::make_pair(ptrNodeID, elementIndex);
    }
    return std::make_pair(-1, -1);
}

std::vector<NodeID> bfsPredecessors(const SVFG* svfg, const SVFIR* pag, const SVFVar* startSVFVar) {
    std::vector<NodeID> results;
    std::set<const VFGNode*> visited;
    std::queue<const VFGNode*> q;

    auto enqueue = [&](const VFGNode* n){ if(n) q.push(n); };

    // 确定起点：优先用 Def 节点；若没有，则从全图中查找与该 Var 直接相关的语句节点
    std::vector<const VFGNode*> startNodes;
    if (svfg->hasDefSVFGNode(startSVFVar)) {
        const VFGNode* startNode = svfg->getDefSVFGNode(startSVFVar);
        if (startNode) startNodes.push_back(startNode);
    }
    if (startNodes.empty()) {
        NodeID targetId = startSVFVar->getId();
        for (auto it = svfg->begin(), ie = svfg->end(); it != ie; ++it) {
            const VFGNode* node = it->second;
            if (const StmtVFGNode* stmt = SVFUtil::dyn_cast<StmtVFGNode>(node)) {
                if (stmt->getPAGDstNodeID() == targetId || stmt->getPAGSrcNodeID() == targetId) {
                    startNodes.push_back(node);
                }
            }
        }
    }
    for (auto* n : startNodes) enqueue(n);
    if (q.empty()) return results;

    // 反向遍历所有进入边，收集可映射的 PAG NodeID，并打印详细调试信息
    while(!q.empty()){
        const VFGNode* current = q.front();
        q.pop();
        if (!visited.insert(current).second) continue;

        std::cout << "[SVFG][BFS] Visit Node #" << current->getId() << "\n";
        std::cout << current->toString() << std::endl;

        for (const auto& edge : current->getInEdges()) {
            const VFGNode* srcNode = edge->getSrcNode();
            std::cout << "  <= from Node #" << srcNode->getId() << "\n";
            std::cout << srcNode->toString() << std::endl;

            if (const StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(srcNode)){
                NodeID srcNodeID = stmtNode->getPAGSrcNodeID();
                NodeID dstNodeID = stmtNode->getPAGDstNodeID();
                if (srcNodeID != 0 && std::find(results.begin(), results.end(), srcNodeID) == results.end())
                    results.push_back(srcNodeID);
                if (dstNodeID != 0 && std::find(results.begin(), results.end(), dstNodeID) == results.end())
                    results.push_back(dstNodeID);
            }

            if (visited.find(srcNode) == visited.end()) {
                q.push(srcNode);
            }
        }
    }
    return results;
}


std::vector<NodeID> getTaintmapExistingNodes(std::vector<NodeID>& nodeIDs, TaintMap& taintMap, SVF::Andersen* ander) {
    std::vector<NodeID> results = taintMap.getExistingNodes(nodeIDs, ander);
    return results;
}


int getTaintmapNewID(NodeID nodeID, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, NodeID preNodeID) {
    std::cout << "[Debug] getTaintmapNewID(nodeID=" << nodeID << ", preNodeID=" << preNodeID << ")\n";
    SVFVar* valueSVFVar = pag->getGNode(preNodeID);
    if (!valueSVFVar) {
        std::cout << "[Debug] pag->getGNode(nodeID) returned null" << "\n";
        return -1;
    }
    std::cout << "[Debug] valueSVFVar ID(from PAG): " << valueSVFVar->getId() << "\n";
    if(taintMap.getNewIds(preNodeID).size() == 1){
        return taintMap.getNewIds(preNodeID)[0];
    } else {
        int size = taintMap.getNewIds(preNodeID).size();
        std::cout << "[Debug] preNodeID has " << size << " newIds in taintMap\n";
        std::pair<NodeID, int> loadPair = parseLoadVFG(valueSVFVar, svfg, pag);
        std::cout << "[Debug] parseLoadVFG => baseNodeID=" << loadPair.first << ", index=" << loadPair.second << "\n";
        if(loadPair.first == -1){
            loadPair = parseGepVFG(svfg, pag, valueSVFVar);
            std::cout << "[Debug] parseGepVFG  => baseNodeID=" << loadPair.first << ", index=" << loadPair.second << "\n";
        }
        if(loadPair.first != -1){
            return taintMap.getNewIds(loadPair.first)[loadPair.second];
        }
    }
    return -1;
}

int parseIntValue(const SVFVar* intSVFVar, const SVFG* svfg, const SVFIR* pag, NodeID intNodeId, PointerAnalysis* ander) {
    const PointsTo& pts = ander->getPts(intNodeId);
    if (!pts.empty()) {
        for (PointsTo::iterator it = pts.begin(); it != pts.end(); ++it) {
            NodeID objId = *it; 
            const SVFVar* objVar = pag->getGNode(objId);
            if (!objVar) {
                continue;
            }

            NodeID addrVarId = -1;
            for (auto nit = svfg->begin(), nie = svfg->end(); nit != nie; ++nit) {
                const SVFGNode* node = nit->second;
                const AddrVFGNode* addrNode = SVFUtil::dyn_cast<AddrVFGNode>(node);
                if (!addrNode) continue;
                if (const StmtVFGNode* stmt = SVFUtil::dyn_cast<StmtVFGNode>(addrNode)) {
                    NodeID srcId = stmt->getPAGSrcNodeID();
                    NodeID dstId = stmt->getPAGDstNodeID();
                    if (srcId == objId) {
                        addrVarId = dstId; // VarX
                        break;
                    }
                }
            }

            if (addrVarId != -1) {
                for (auto nit = svfg->begin(), nie = svfg->end(); nit != nie; ++nit) {
                    const SVFGNode* node = nit->second;
                    if (const StoreVFGNode* storeNode = SVFUtil::dyn_cast<StoreVFGNode>(node)) {
                        NodeID dstId = storeNode->getPAGDstNodeID();
                        if (dstId == addrVarId || ander->alias(dstId, addrVarId) != SVF::AliasResult::NoAlias) {
                            const SVFVar* storedValue = storeNode->getPAGSrcNode();
                            if (const ConstIntValVar* constIntVar = SVFUtil::dyn_cast<ConstIntValVar>(storedValue)) {
                                s64_t intValue = constIntVar->getSExtValue();
                                std::cout << "The value of argc is: " << intValue << std::endl;
                                return static_cast<int>(intValue);
                            }
                        }
                    }
                }
            }
        }
    } else {
        std::cerr << "Warning: No points-to information found for argc." << std::endl;
    }

    return -1;
}

void parseArgsOperand(const SVFG* svfg, const SVFIR* pag, NodeID valueParamNodeID, 
                    const SVFVar* valueSVFVar, TaintMap& taintMap, 
                    std::vector<SummaryItem>& summaryItems, SummaryItem& summaryItemResult, int argc, SVF::Andersen* ander) {
    std::vector<NodeID> preNodeIDs = bfsPredecessors(svfg, pag, valueSVFVar);
    std::vector<NodeID> existingNodeIDs = getTaintmapExistingNodes(preNodeIDs, taintMap, ander);
    if(existingNodeIDs.size() == 0){
        return;
    }
    if(existingNodeIDs.size() == 1){
        int newID = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, existingNodeIDs[0]);
        for(int i = 0; i < argc; i++){
            summaryItemResult.addArgsOperand("%"+std::to_string(newID++));
        }
    }
    return;
}

int handlePhi(const SVFG* svfg, const SVFIR* pag, const llvm::Value* valueParam,
                    TaintMap& taintMap, std::vector<SummaryItem>& summaryItems, SVF::Andersen* ander) {
    NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
    const SVFVar* valueSVFVar = pag->getGNode(valueParamNodeID);
    if(LLVMUtil::isNullPtrSym(valueParam)){
        return -1;
    }
    if (llvm::isa<llvm::Constant>(valueParam)){
        return -1;
    }
    std::vector<int> newIDs = taintMap.getNewIds(valueParamNodeID);
    if(newIDs.size() > 0){
        return newIDs[0];
    }
    std::vector<NodeID> preNodeIDs = bfsPredecessors(svfg, pag, valueSVFVar);
    std::vector<NodeID> existingNodeIDs = getTaintmapExistingNodes(preNodeIDs, taintMap, ander);
    if(existingNodeIDs.size() == 0){
        return -1;
    }
    else if(existingNodeIDs.size() == 1){
        return existingNodeIDs[0];
    }
    else {
        // 遍历所有 existingNodeIDs，选择第一个 getTaintmapNewID 返回的非 -1
        for (auto preId : existingNodeIDs) {
            int cand = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, preId);
            if (cand != -1) {
                taintMap.setNewID(valueParamNodeID, cand);
                return cand;
            }
        }
        // 全为 -1，则分配新的 ID
        int valueID = taintMap.assignNewId(valueParamNodeID);
        return valueID;
    }
}


void handleTaintFlow(const SVFG* svfg, const SVFIR* pag, const llvm::Value* valueParam,
                    TaintMap& taintMap, std::vector<SummaryItem>& summaryItems, SummaryItem& summaryItemResult, SVF::Andersen* ander) {
    NodeID valueParamNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueParam);
    const SVFVar* valueSVFVar = pag->getGNode(valueParamNodeID);
    if(LLVMUtil::isNullPtrSym(valueParam)){
        summaryItemResult.addOperand("null");
        return;
    }
    const PointsTo& pts = ander->getPts(valueParamNodeID);
    std::vector<NodeID> preNodeIDs;
    for (PointsTo::iterator it = pts.begin(); it != pts.end(); ++it) {
        NodeID objId = *it;  // 指向的对象节点ID
        preNodeIDs.push_back(objId);
        std::cout << "Points to: " << objId << std::endl;
    }
    std::vector<NodeID> predecessors = bfsPredecessors(svfg, pag, valueSVFVar);
    preNodeIDs.insert(preNodeIDs.end(), predecessors.begin(), predecessors.end());
    std::vector<NodeID> existingNodeIDs = getTaintmapExistingNodes(preNodeIDs, taintMap, ander);
    if(existingNodeIDs.size() == 0) {
        bool hasInt = false;
        for(auto preNodeID : preNodeIDs){
            const SVFVar* preSVFVar = pag->getGNode(preNodeID);
            int intValue = parseIntValue(preSVFVar, svfg, pag, preNodeID, ander);
            if(intValue != -1){
                summaryItemResult.addOperand("long "+std::to_string(intValue));
                hasInt = true;
                break;
            }
        } 
        if(!hasInt){
            int valueID = taintMap.assignNewId(valueParamNodeID);
            summaryItemResult.addOperand("%"+std::to_string(valueID));
        }
        
    } else if(existingNodeIDs.size() == 1) {
        int newID = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, existingNodeIDs[0]);
        taintMap.setNewID(valueParamNodeID, newID);
        summaryItemResult.addOperand("%"+std::to_string(newID));
    } else {
        // 遍历所有 existingNodeIDs，逐个尝试获取可用 newID
        for (auto preId : existingNodeIDs) {
            int cand = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, preId);
            if (cand != -1) {
                taintMap.setNewID(valueParamNodeID, cand);
                summaryItemResult.addOperand("%"+std::to_string(cand));
                return;
            }
        }
        // 全为 -1，则分配新的 ID
        int valueID = taintMap.assignNewId(valueParamNodeID);
        summaryItemResult.addOperand("%"+std::to_string(valueID));
    }
}

std::string parseConstant(const llvm::Value* value) {
    if (llvm::isa<llvm::Constant>(value)) {
        std::cout << "Value type: " << value->getType()->getTypeID() << std::endl;
        if (const llvm::ConstantInt* intConstant = llvm::dyn_cast<llvm::ConstantInt>(value)) {
            // 处理整数常量
            std::cout << "Constant value: " << intConstant->getSExtValue() << std::endl;
            std::cout << "Type: Integer" << std::endl;
            return "long "+std::to_string(intConstant->getSExtValue());
        } else if (const llvm::ConstantFP* fpConstant = llvm::dyn_cast<llvm::ConstantFP>(value)) {
            // 处理浮点常量
            std::cout << "Constant value: " << fpConstant->getValueAPF().convertToDouble() << std::endl;
            std::cout << "Type: Floating-point" << std::endl;
            return "float "+std::to_string(fpConstant->getValueAPF().convertToDouble());
        } else if (const llvm::ConstantDataArray* dataArray = llvm::dyn_cast<llvm::ConstantDataArray>(value)) {
            // 处理常量数据数组
            std::cout << "Constant value: ";
            for (unsigned i = 0; i < dataArray->getNumElements(); ++i) {
                if (const llvm::ConstantInt* element = llvm::dyn_cast<llvm::ConstantInt>(dataArray->getElementAsConstant(i))) {
                    std::cout << element->getSExtValue() << " ";
                }
            }
            std::cout << std::endl;
            std::cout << "Type: Constant Data Array" << std::endl;
            return "char* \""+std::to_string(dataArray->getNumElements())+"\"";
        } else if (const llvm::ConstantStruct* constantStruct = llvm::dyn_cast<llvm::ConstantStruct>(value)) {
            // 处理常量结构体
            std::cout << "Constant value: Struct with " << constantStruct->getNumOperands() << " elements" << std::endl;
            std::cout << "Type: Constant Struct" << std::endl;
            std::string result = "struct { ";
            for (unsigned i = 0; i < constantStruct->getNumOperands(); ++i) {
                const llvm::Value* operand = constantStruct->getOperand(i);
                std::string operandStr = parseConstant(operand);
                result += operandStr;
                if (i < constantStruct->getNumOperands() - 1) {
                    result += ", ";
                }
            }
            result += " }";
            return result;
        } else if (const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(value)) {
            // 处理常量表达式
            switch (constExpr->getOpcode()) {
                case llvm::Instruction::GetElementPtr: {
                    std::cout << "Constant value: GEP Constant Expression" << std::endl;
                    std::cout << "Type: ConstantExpr (GEP)" << std::endl;
                    // 递归处理操作数
                    const llvm::Value* opnd = constExpr->getOperand(0);
                    std::string opndStr = parseConstant(opnd);
                    return "gep_expr " + opndStr;
                }
                case llvm::Instruction::BitCast: {
                    std::cout << "Constant value: BitCast Constant Expression" << std::endl;
                    std::cout << "Type: ConstantExpr (BitCast)" << std::endl;
                    // 递归处理操作数
                    const llvm::Value* opnd = constExpr->getOperand(0);
                    std::string opndStr = parseConstant(opnd);
                    return "bitcast_expr " + opndStr;
                }
                case llvm::Instruction::Select: {
                    std::cout << "Constant value: Select Constant Expression" << std::endl;
                    std::cout << "Type: ConstantExpr (Select)" << std::endl;
                    // 递归处理操作数
                    const llvm::Value* src1 = constExpr->getOperand(1);
                    const llvm::Value* src2 = constExpr->getOperand(2);
                    std::string src1Str = parseConstant(src1);
                    std::string src2Str = parseConstant(src2);
                    return "select_expr " + src1Str + " " + src2Str;
                }
                case llvm::Instruction::IntToPtr: {
                    std::cout << "Constant value: IntToPtr Constant Expression" << std::endl;
                    std::cout << "Type: ConstantExpr (IntToPtr)" << std::endl;
                    // 递归处理操作数
                    const llvm::Value* opnd = constExpr->getOperand(0);
                    std::string opndStr = parseConstant(opnd);
                    return "int2ptr_expr " + opndStr;
                }
                case llvm::Instruction::PtrToInt: {
                    std::cout << "Constant value: PtrToInt Constant Expression" << std::endl;
                    std::cout << "Type: ConstantExpr (PtrToInt)" << std::endl;
                    // 递归处理操作数
                    const llvm::Value* opnd = constExpr->getOperand(0);
                    std::string opndStr = parseConstant(opnd);
                    return "ptr2int_expr " + opndStr;
                }
                default:
                    std::cout << "Constant value: N/A (unsupported ConstantExpr opcode)" << std::endl;
                    std::cout << "Type: ConstantExpr (Other)" << std::endl;
                    return "";
            }
        } else if (value->getType()->getTypeID() == llvm::Type::TypeID::PointerTyID) {
            if (const GlobalVariable* gv = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
                // 获取 GlobalVariable 的初始值
                if (const Constant* init = gv->getInitializer()) {
                    // 检查初始值是否为 ConstantDataSequential
                    if (const ConstantDataSequential* cds = llvm::dyn_cast<llvm::ConstantDataSequential>(init)) {
                        // 检查是否为字符串
                        if (cds->isString()) {
                            // 获取字符串
                            std::cout << "Constant value: " << cds->getAsString().str() << std::endl;
                            std::string str = cds->getAsString().str();
                            std::string result = "char* \""+str+"\"";
                            return result;
                        }
                    }
                }
            }
        }
        else {
            // 其他类型常量
            std::cout << "Constant value: N/A (unsupported constant type)" << std::endl;
            std::cout << "Type: Other" << std::endl;
            return "top";
        }
    } else {
        std::cout << "The value is not a constant." << std::endl;
    }
    return "";
}

NodeID parseArgvValue(SVFVar* argvSVFVar, const SVFG* svfg, const SVFIR* pag) {
    // 目标：找到 argv 指向的最顶层 NodeID（例如数组基指针），优先使用 BFS 反向回溯得到的上游候选
    if (argvSVFVar == nullptr) return -1;

    // 先尝试使用现有的 BFS 逻辑回溯与该 Var 相关的上游 PAG NodeID 集合
    std::vector<NodeID> preds = bfsPredecessors(svfg, pag, argvSVFVar);

    // 返回 preds 的最后一个元素（如果存在）
    if (!preds.empty()) {
        return preds.back();
    }

    return -1;
}