#include "taintanalysis/TaintMap.h"

using namespace SVF;

TaintMap::TaintMap() : counter(0) {}

TaintMap::TaintMap(std::vector<std::pair<NodeID, std::string>> paramNodeIDs, SVF::PointerAnalysis* pta) : counter(0), pta(pta) {
    this->pta = pta; // 初始化PointerAnalysis指针
    for (size_t i = 0; i < paramNodeIDs.size(); ++i) {
        const auto& node = paramNodeIDs[i];
        int paramId = assignNewId(node.first);
        
        // 根据索引设置参数名称
        std::string paramName;
        if (i == 0) {
            paramName = "napi_env";
        } else if (i == 1) {
            paramName = "napi_callback_info";
        } else {
            paramName = node.second; // 保持原有的参数名称
        }
        
        paramIds.push_back(ParamInfo(node.first, paramId, paramName));
    }
}

int TaintMap::assignNewId(NodeID node) {
    if (nodeToNewIds.find(node) != nodeToNewIds.end()) {
        if(nodeToNewIds[node].size() >= 1) {
            return nodeToNewIds[node][0];
        } else {
            return -1;
        }
    }

    int newId = counter++;
    if (nodeToNewIds.find(node) == nodeToNewIds.end()) {
        nodeToNewIds[node] = std::vector<int>();
    }
    nodeToNewIds[node].push_back(newId);
    if (newIdToNode.find(newId) == newIdToNode.end()) {
        newIdToNode[newId] = std::vector<SVF::NodeID>();
    }
    newIdToNode[newId].push_back(node);

    // Handle points-to set
    const PointsTo& pts = this->pta->getPts(node);
    for (PointsTo::iterator it = pts.begin(); it != pts.end(); ++it) {
        NodeID objId = *it;
        if (nodeToNewIds.find(objId) == nodeToNewIds.end()) {
            int objNewId = newId;
            nodeToNewIds[objId] = std::vector<int>();
            nodeToNewIds[objId].push_back(objNewId);
            if (newIdToNode.find(objNewId) == newIdToNode.end()) {
                newIdToNode[objNewId] = std::vector<SVF::NodeID>();
            }
            newIdToNode[objNewId].push_back(objId);
        }
    }

    return newId;
}

std::vector<int> TaintMap::assignArrayNewIds(NodeID arrayNode, int elementCount) {
    std::vector<int> newIds;
    for (int i = 0; i < elementCount; ++i) {
        newIds.push_back(counter++);
        if (newIdToNode.find(newIds.back()) == newIdToNode.end()) {
            newIdToNode[newIds.back()] = std::vector<SVF::NodeID>();
        }
        newIdToNode[newIds.back()].push_back(arrayNode);
    }
    nodeToNewIds[arrayNode] = newIds;
    return newIds;
}

void TaintMap::setArrayNewIds(NodeID arrayNode, const std::vector<int>& newIds) {
    nodeToNewIds[arrayNode] = newIds;
}

void TaintMap::recordValueFlow(int dest, const std::vector<int>& sources) {
    valueFlowMap[dest] = sources;
}

const std::vector<int>& TaintMap::getNewIds(NodeID node) const {
    static std::vector<int> empty;
    auto it = nodeToNewIds.find(node);
    return it != nodeToNewIds.end() ? it->second : empty;
}

void TaintMap::setNewID(NodeID node, int newId) {
    if (nodeToNewIds.find(node) == nodeToNewIds.end()) {
        nodeToNewIds[node] = std::vector<int>();
    }
    nodeToNewIds[node].push_back(newId);
    if (newIdToNode.find(newId) == newIdToNode.end()) {
        newIdToNode[newId] = std::vector<SVF::NodeID>();
    }
    newIdToNode[newId].push_back(node);
}

void TaintMap::setNewIDAtFront(NodeID node, int newId) {
    if (nodeToNewIds.find(node) == nodeToNewIds.end()) {
        nodeToNewIds[node] = std::vector<int>();
    }
    nodeToNewIds[node].insert(nodeToNewIds[node].begin(), newId);
    if (newIdToNode.find(newId) == newIdToNode.end()) {
        newIdToNode[newId] = std::vector<SVF::NodeID>();
    }
    newIdToNode[newId].push_back(node);
}

std::vector<SVF::NodeID> TaintMap::getOriginalNodes(int newId) const {
    auto it = newIdToNode.find(newId);
    return it != newIdToNode.end() ? it->second : std::vector<SVF::NodeID>();
}

const std::vector<int>& TaintMap::getValueFlowSources(int node) const {
    static std::vector<int> empty;
    auto it = valueFlowMap.find(node);
    return it != valueFlowMap.end() ? it->second : empty;
}

void TaintMap::addValueFlowSource(int dest, int source) {
    // 先判断dest是否在valueFlowMap中
    if (valueFlowMap.find(dest) == valueFlowMap.end()) {
        valueFlowMap[dest] = std::vector<int>();
    }
    valueFlowMap[dest].push_back(source);
}

std::vector<SVF::NodeID> TaintMap::getExistingNodes(std::vector<SVF::NodeID>& nodes, SVF::Andersen* ander) {
    std::vector<SVF::NodeID> result;
    for (SVF::NodeID node : nodes) {
        bool shouldAdd = false;
        SVF::NodeID aliasNodeId = 0; // 记录找到的别名节点ID
        
        // 检查节点是否直接存在于nodeToNewIds中
        if (nodeToNewIds.find(node) != nodeToNewIds.end()) {
            shouldAdd = true;
        } else {
            // 对每个node与nodeToNewIds中的所有节点进行aliasQuery检查
            for (const auto& existingNode : nodeToNewIds) {
                SVF::NodeID existingNodeId = existingNode.first;
                if (ander->alias(node, existingNodeId) == SVF::AliasResult::MayAlias ||
                    ander->alias(node, existingNodeId) == SVF::AliasResult::MustAlias) {
                    shouldAdd = true;
                    aliasNodeId = existingNodeId; // 记录找到的别名节点
                    break;
                }
            }
        }
        
        // 如果应该添加且result中没有则添加
        if (shouldAdd && std::find(result.begin(), result.end(), node) == result.end()) {
            result.push_back(node);  
            // 如果节点是通过别名关系发现的，且不在nodeToNewIds中，则将其添加到映射中
            if (nodeToNewIds.find(node) == nodeToNewIds.end() && aliasNodeId != 0) {
                // 复制别名节点的所有新ID到当前节点
                const std::vector<int>& aliasNewIds = nodeToNewIds[aliasNodeId];
                nodeToNewIds[node] = aliasNewIds;
                
                // 同时更新newIdToNode映射
                for (int newId : aliasNewIds) {
                    if (newIdToNode.find(newId) == newIdToNode.end()) {
                        newIdToNode[newId] = std::vector<SVF::NodeID>();
                    }
                    newIdToNode[newId].push_back(node);
                }
            }
        }
    }
    return result;
} 

int TaintMap::getParamIdByIndex(size_t index) const {
    if (index < paramIds.size()) {
        return paramIds[index].paramId;
    }
    return -1; // 索引超出范围
}

int TaintMap::getParamIdIfAlias(SVF::NodeID nodeID, SVF::Andersen* ander) const {
    for (const auto& paramInfo : paramIds) {
        // 检查是否可能是别名
        if (ander->alias(nodeID, paramInfo.nodeId) == SVF::AliasResult::MayAlias ||
            ander->alias(nodeID, paramInfo.nodeId) == SVF::AliasResult::MustAlias) {
            return paramInfo.paramId;
        }
    }
    return -1; // 没有找到别名关系
}

