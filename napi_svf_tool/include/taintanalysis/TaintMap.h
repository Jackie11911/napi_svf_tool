#ifndef TAINTMAP_H
#define TAINTMAP_H

#include <unordered_map>
#include <vector>
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"



class TaintMap {
private:
    int counter;
    std::unordered_map<SVF::NodeID, std::vector<int>> nodeToNewIds;
    std::unordered_map<int, std::vector<int>> valueFlowMap;
    std::unordered_map<int, std::vector<SVF::NodeID>> newIdToNode;

public:
    TaintMap();

    explicit TaintMap(std::vector<SVF::NodeID> paramNodeIDs);
    
    // 为单个NodeID分配新编号
    int assignNewId(SVF::NodeID node);
    
    // 为数组类型的NodeID分配多个连续编号
    std::vector<int> assignArrayNewIds(SVF::NodeID arrayNode, int elementCount);

    // 设置数组类型的NodeID的新编号
    void setArrayNewIds(SVF::NodeID arrayNode, const std::vector<int>& newIds);
    
    // 记录传值关系
    void recordValueFlow(int dest, const std::vector<int>& sources);
    
    // 获取NodeID对应的所有新编号
    const std::vector<int>& getNewIds(SVF::NodeID node) const;

    // 设置新编号对应的原始NodeID
    void setNewID(SVF::NodeID node, int newId);
    
    // 获取新编号对应的原始NodeID
    std::vector<SVF::NodeID> getOriginalNodes(int newId) const;
    
    // 获取传值关系
    const std::vector<int>& getValueFlowSources(int node) const;

    void addValueFlowSource(int dest, int source);

    std::vector<SVF::NodeID> getExistingNodes(std::vector<SVF::NodeID>& nodes) const;
};

#endif // TAINTMAP_H