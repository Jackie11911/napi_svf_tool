#ifndef TAINTMAP_H
#define TAINTMAP_H

#include <unordered_map>
#include <vector>
#include "SVFIR/SVFIR.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "WPA/Andersen.h"

// 参数信息结构体
struct ParamInfo {
    SVF::NodeID nodeId;  // 原始节点ID
    int paramId;         // 分配的参数ID
    std::string paramName; // 参数名称
    
    ParamInfo(SVF::NodeID nid, int pid, const std::string& name) 
        : nodeId(nid), paramId(pid), paramName(name) {}
};

class TaintMap {
private:
    int counter;
    std::unordered_map<SVF::NodeID, std::vector<int>> nodeToNewIds;
    std::unordered_map<int, std::vector<int>> valueFlowMap;
    std::unordered_map<int, std::vector<SVF::NodeID>> newIdToNode;
    std::vector<ParamInfo> paramIds;
    SVF::PointerAnalysis* pta;

public:
    TaintMap();

    explicit TaintMap(std::vector<std::pair<SVF::NodeID, std::string>> paramNodeIDs, SVF::PointerAnalysis* pta);
    
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
    void setNewIDAtFront(SVF::NodeID node, int newId);
    
    // 获取新编号对应的原始NodeID
    std::vector<SVF::NodeID> getOriginalNodes(int newId) const;
    
    // 获取传值关系
    const std::vector<int>& getValueFlowSources(int node) const;

    void addValueFlowSource(int dest, int source);

    std::vector<SVF::NodeID> getExistingNodes(std::vector<SVF::NodeID>& nodes, SVF::Andersen* ander);
    
    // 获取参数信息
    const std::vector<ParamInfo>& getParamIds() const { return paramIds; }
    
    // 根据参数位置获取对应的paramId
    int getParamIdByIndex(size_t index) const;
    
    // 检查nodeID是否与paramIds中的节点可能是别名，如果是则返回对应的paramId
    int getParamIdIfAlias(SVF::NodeID nodeID, SVF::Andersen* ander) const;
};

#endif // TAINTMAP_H