#include "taintanalysis/TaintMap.h"

using namespace SVF;

TaintMap::TaintMap() : counter(0) {}

TaintMap::TaintMap(std::vector<SVF::NodeID> paramNodeIDs) : counter(0) {
    for (SVF::NodeID node : paramNodeIDs) {
        assignNewId(node);
    }
}

int TaintMap::assignNewId(NodeID node) {
    if (nodeToNewIds.find(node) != nodeToNewIds.end()) {
        if(nodeToNewIds[node].size()  >= 1) {
            return nodeToNewIds[node][0];
        }
        else {
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

std::vector<SVF::NodeID> TaintMap::getExistingNodes(std::vector<SVF::NodeID>& nodes) const {
    std::vector<SVF::NodeID> result;
    for (SVF::NodeID node : nodes) {
        if (nodeToNewIds.find(node) != nodeToNewIds.end()) {
            // 如果result中没有则添加
            if(std::find(result.begin(), result.end(), node) == result.end()) {
                result.push_back(node);
            }
        }
    }
    return result;
} 

