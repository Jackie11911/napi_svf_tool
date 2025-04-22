// // TaintMap.h
// #ifndef TAINTANALYSIS_TAINTMAP_H
// #define TAINTANALYSIS_TAINTMAP_H

// #include "SVFIR/SVFIR.h"  // 包含SVF基础类型定义
// #include <unordered_map>
// #include <set>
// #include <utility>  // 用于std::pair
// #include <vector>
// #include <string>
// #include "taintanalysis/SummaryItem.h"
// #include <variant>

// using namespace SVF;
// using ConstantValue = std::variant<int, float, std::string>;

// class TaintMap {
// public:
//     using NodeID = SVF::NodeID;
//     using NodePair = std::pair<NodeID, NodeID>;

//     TaintMap() = default; // 默认构造函数
//     explicit TaintMap(const std::vector<NodeID>& operands);

//     // 添加关系类型
//     void addAlias(NodeID n1, NodeID n2);
//     void addDataFlow(NodeID src, NodeID dst);
//     void addPhiFlow(NodeID src1, NodeID src2, NodeID dst);

//     // 查询接口
//     const std::set<NodeID>& getAliases(NodeID node) const;
//     const std::set<NodeID>& getDataFlows(NodeID node) const;
//     const std::set<NodePair>& getPhiFlows(NodeID node) const;

//     // 关系检查
//     bool isAlias(NodeID n1, NodeID n2) const;
//     bool hasDataFlow(NodeID src, NodeID dst) const;
//     bool hasPhiFlow(NodeID src1, NodeID src2, NodeID dst) const;

//     void addOperand(NodeID node);  // 添加操作数
//     const std::vector<NodeID>& getOperands() const;  // 获取所有操作数
//     bool isOperand(NodeID node) const;  // 检查是否为操作数

//     void addsummaryItem(SummaryItem item);

//     const std::vector<SummaryItem>& getSummaryItems() const;

//     void addConstantValue(NodeID node, const std::variant<int, float, std::string>& value);
//     const std::variant<int, float, std::string>& getConstantValue(NodeID node) const;
//     bool hasConstantValue(NodeID node) const;

//     // 可视化输出
//     void dump() const;

//     // 清空所有记录
//     void clear();

// private:
//     // 别名关系存储 (双向映射)
//     std::unordered_map<NodeID, std::set<NodeID>> aliasMap_;

//     // 直接传值关系 (src -> dst)
//     std::unordered_map<NodeID, std::set<NodeID>> dataFlowMap_;

//     // 组合传值关系 (src1 + src2 -> dst)
//     std::unordered_map<NodeID, std::set<NodePair>> phiFlowMap_;

//     // 存值已有的操作数
//     std::vector<NodeID> operands_vector;

//     std::vector<SummaryItem> summaryItems;

//     std::unordered_map<NodeID, ConstantValue> constantMap;

//     // 空集合的引用返回（用于无效查询）
//     static const std::set<NodeID> emptyNodeSet_;
//     static const std::set<NodePair> emptyPairSet_;
// };

// #endif // TAINTANALYSIS_TAINTMAP_H