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

std::vector<NodeID> bfsPredecessors(const SVFG* svfg, const SVFVar* startSVFVar, const SVFIR* pag) {
    std::vector<NodeID> results;
    std::set<const VFGNode*> visited;
    if(!svfg->hasDefSVFGNode(startSVFVar)){
        return results;
    }
    const VFGNode* startNode = svfg->getDefSVFGNode(startSVFVar);
    if(!startNode){
        return results;
    }
    std::queue<const VFGNode*> queue;
    queue.push(startNode);
    while(!queue.empty()){
        const VFGNode* current = queue.front();
        queue.pop();
        if(visited.find(current) != visited.end()){
            continue;
        }
        visited.insert(current);
        // 获取当前节点的直接赋值节点
        const auto& inEdges = current->getInEdges();
        for (const auto& edge : inEdges) {
            // 检查边是否为直接值流边
            if (const DirectSVFGEdge* dirEdge = SVFUtil::dyn_cast<DirectSVFGEdge>(edge)) {
                const VFGNode* srcNode = dirEdge->getSrcNode();
                if(const StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(srcNode)){
                    NodeID srcNodeID = stmtNode->getPAGSrcNodeID();
                    NodeID dstNodeID = stmtNode->getPAGDstNodeID();
                    // 如果不存在则添加
                    if(std::find(results.begin(), results.end(), srcNodeID) == results.end()){
                        results.push_back(srcNodeID);
                    }
                    if(std::find(results.begin(), results.end(), dstNodeID) == results.end()){
                        results.push_back(dstNodeID);
                    }
                }
                if (visited.find(srcNode) == visited.end()) {
                    queue.push(srcNode);
                }
            }
        }
    }
    return results;
}


std::vector<NodeID> getTaintmapExistingNodes(std::vector<NodeID>& nodeIDs, TaintMap& taintMap, SVF::Andersen* ander) {
    std::vector<NodeID> results = taintMap.getExistingNodes(nodeIDs, ander);
    return results;
}

std::vector<NodeID> bfsPredecessorsWithImplicitFlow(const SVFG* svfg,const SVFVar* startSVFVar, const SVFIR* pag) {
    std::vector<NodeID> results;
    std::set<const VFGNode*> visited;
    if(!svfg->hasDefSVFGNode(startSVFVar)){
        return results;
    }
    const VFGNode* startNode = svfg->getDefSVFGNode(startSVFVar);
    if(!startNode){
        return results;
    }
    std::queue<const VFGNode*> queue;
    queue.push(startNode);
    while(!queue.empty()){
        const VFGNode* current = queue.front();
        queue.pop();
        if(visited.find(current) != visited.end()){
            continue;
        }
        visited.insert(current);
        for (VFGNode::const_iterator it = current->InEdgeBegin(), eit = current->InEdgeEnd(); it != eit; ++it)
        {
            VFGEdge* edge = *it;
            VFGNode* PreNode = edge->getSrcNode();
            if (visited.find(PreNode) == visited.end())
            {
                queue.push(PreNode);
            }
            NodeID preNodeID = PreNode->getId();
            std::cout << "preNodeID: " << preNodeID << std::endl;
            results.push_back(preNodeID);
        }
    }
    return results;
}



int getTaintmapNewID(NodeID nodeID, TaintMap& taintMap, const SVFG* svfg, const SVFIR* pag, NodeID preNodeID) {
    SVFVar* valueSVFVar = pag->getGNode(nodeID);
    if(taintMap.getNewIds(preNodeID).size() == 1){
        return taintMap.getNewIds(preNodeID)[0];
    } else {
        int size = taintMap.getNewIds(preNodeID).size();
        std::pair<NodeID, int> loadPair = parseLoadVFG(valueSVFVar, svfg, pag);
        if(loadPair.first == -1){
            loadPair = parseGepVFG(svfg, pag, valueSVFVar);
        }
        if(loadPair.first == preNodeID){
            return taintMap.getNewIds(loadPair.first)[loadPair.second];
        }
    }
    return -1;
}

int parseIntValue(const SVFVar* intSVFVar, const SVFG* svfg, const SVFIR* pag) {
    if (!svfg->hasDefSVFGNode(intSVFVar)){
        return -1;
    }
    
    const VFGNode* intVNode = svfg->getDefSVFGNode(intSVFVar);
    if (!intVNode) {
        return -1;
    }
    // 遍历 intSVFVar 的出边
    for (auto it = intVNode->OutEdgeBegin(); it != intVNode->OutEdgeEnd(); ++it) {
        SVFGEdge* edge = *it;
        const VFGNode* dstNode = edge->getDstNode();

        // 检查源节点是否为 StoreVFGNode
        if (const StoreVFGNode* storeNode = SVFUtil::dyn_cast<StoreVFGNode>(dstNode)) {
            // 获取存储的值
            const SVFVar* storedValue = storeNode->getPAGSrcNode();
            if (const ConstIntValVar* constIntVar = SVFUtil::dyn_cast<ConstIntValVar>(storedValue)) {
                // 获取常量整数值（有符号扩展值）
                s64_t intValue = constIntVar->getSExtValue();
                std::cout << "The value of int is: " << intValue << std::endl;
                return intValue;
            }
        }
    }

    std::cout << "Failed to find the value of argc." << std::endl;

    return -1;
}

void parseArgsOperand(const SVFG* svfg, const SVFIR* pag, NodeID valueParamNodeID, 
                    const SVFVar* valueSVFVar, TaintMap& taintMap, 
                    std::vector<SummaryItem>& summaryItems, SummaryItem& summaryItemResult, int argc, SVF::Andersen* ander) {
    std::vector<NodeID> preNodeIDs = bfsPredecessors(svfg, valueSVFVar, pag);
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
    std::vector<NodeID> preNodeIDs = bfsPredecessors(svfg, valueSVFVar, pag);
    std::vector<NodeID> existingNodeIDs = getTaintmapExistingNodes(preNodeIDs, taintMap, ander);
    if(existingNodeIDs.size() == 0){
        return -1;
    }
    else if(existingNodeIDs.size() == 1){
        return existingNodeIDs[0];
    }
    else {
        int phiID1 = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, existingNodeIDs[0]);
        int phiID2 = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, existingNodeIDs[1]);
        
        // 处理-1的情况
        if(phiID1 == -1 && phiID2 == -1){
            // 两个都为-1，assignNewId
            int valueID = taintMap.assignNewId(valueParamNodeID);
            return valueID;
        } else if(phiID1 == -1){
            // 只有phiID1为-1，使用phiID2
            taintMap.setNewID(valueParamNodeID, phiID2);
            return phiID2;
        } else if(phiID2 == -1){
            // 只有phiID2为-1，使用phiID1
            taintMap.setNewID(valueParamNodeID, phiID1);
            return phiID1;
        }
        
        // 两个都不为-1的情况
        if(phiID1 == phiID2){
            return phiID1;
        }
        int valueID = taintMap.assignNewId(valueParamNodeID);
        SummaryItem phiItem1("", "Phi");
        phiItem1.addOperand("%"+std::to_string(phiID1));
        phiItem1.addOperand("%"+std::to_string(phiID2));
        phiItem1.addRetValue("%"+std::to_string(valueID), -1);
        summaryItems.push_back(phiItem1);
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
    std::vector<NodeID> preNodeIDs = bfsPredecessors(svfg, valueSVFVar, pag);
    std::vector<NodeID> existingNodeIDs = getTaintmapExistingNodes(preNodeIDs, taintMap, ander);
    if(existingNodeIDs.size() == 0) {
        bool hasInt = false;
        for(auto preNodeID : preNodeIDs){
            const SVFVar* preSVFVar = pag->getGNode(preNodeID);
            int intValue = parseIntValue(preSVFVar, svfg, pag);
            if(intValue != -1){
                summaryItemResult.addOperand("long "+std::to_string(intValue));
                hasInt = true;
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
        int phiID1 = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, existingNodeIDs[0]);
        int phiID2 = getTaintmapNewID(valueParamNodeID, taintMap, svfg, pag, existingNodeIDs[1]);
        
        // 处理-1的情况
        if(phiID1 == -1 && phiID2 == -1){
            // 两个都为-1，assignNewId
            int valueID = taintMap.assignNewId(valueParamNodeID);
            summaryItemResult.addOperand("%"+std::to_string(valueID));
        } else if(phiID1 == -1){
            // 只有phiID1为-1，使用phiID2
            taintMap.setNewID(valueParamNodeID, phiID2);
            summaryItemResult.addOperand("%"+std::to_string(phiID2));
        } else if(phiID2 == -1){
            // 只有phiID2为-1，使用phiID1
            taintMap.setNewID(valueParamNodeID, phiID1);
            summaryItemResult.addOperand("%"+std::to_string(phiID1));
        } else {
            // 两个都不为-1的情况
            if(phiID1 == phiID2){
                taintMap.setNewID(valueParamNodeID, phiID1);
                summaryItemResult.addOperand("%"+std::to_string(phiID1));
            } else {
                int valueID = taintMap.assignNewId(valueParamNodeID);
                taintMap.addValueFlowSource(valueID, phiID1);
                taintMap.addValueFlowSource(valueID, phiID2);
                
                SummaryItem phiItem1("", "Phi");
                phiItem1.addOperand("%"+std::to_string(phiID1));
                phiItem1.addOperand("%"+std::to_string(phiID2));
                phiItem1.addRetValue("%"+std::to_string(valueID), -1);
                summaryItems.push_back(phiItem1);
                summaryItemResult.addOperand("%"+std::to_string(valueID));
            }
        }
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