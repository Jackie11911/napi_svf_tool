// // TaintMap.cpp
// #include "taintanalysis/TaintMap.h"
// #include "SVF-LLVM/LLVMUtil.h"  // 用于节点ID转换
// #include "Util/SVFUtil.h"      // 用于调试输出
// using namespace SVF;

// // 静态成员初始化
// const std::set<TaintMap::NodeID> TaintMap::emptyNodeSet_;
// const std::set<TaintMap::NodePair> TaintMap::emptyPairSet_;

// TaintMap::TaintMap(const std::vector<NodeID>& operands) {
//     for (NodeID node : operands) {
//         addOperand(node);
//     }
// }

// void TaintMap::addsummaryItem(SummaryItem item) {
//     summaryItems.push_back(item);
// }

// const std::vector<SummaryItem>& TaintMap::getSummaryItems() const {
//     return summaryItems;
// }


// void TaintMap::addAlias(NodeID n1, NodeID n2) {
//     // 确保不自反
//     if (n1 == n2) return;
    
//     // 双向插入保证无方向性
//     aliasMap_[n1].insert(n2);
//     aliasMap_[n2].insert(n1);
// }

// void TaintMap::addDataFlow(NodeID src, NodeID dst) {
//     // 过滤无效节点
//     if (src == 0 || dst == 0) return;
    
//     // 防止循环引用
//     if (src == dst || hasDataFlow(dst, src)) return;
    
//     dataFlowMap_[src].insert(dst);
// }

// void TaintMap::addPhiFlow(NodeID src1, NodeID src2, NodeID dst) {
//     // 基本参数检查
//     if (src1 == 0 || src2 == 0 || dst == 0) return;
//     if (src1 == dst || src2 == dst) return;
    
//     // 标准化顺序存储（避免重复）
//     if (src1 > src2) std::swap(src1, src2);
//     phiFlowMap_[dst].insert({src1, src2});
// }



// const std::set<TaintMap::NodeID>& TaintMap::getAliases(NodeID node) const {
//     auto it = aliasMap_.find(node);
//     return it != aliasMap_.end() ? it->second : emptyNodeSet_;
// }

// const std::set<TaintMap::NodeID>& TaintMap::getDataFlows(NodeID node) const {
//     auto it = dataFlowMap_.find(node);
//     return it != dataFlowMap_.end() ? it->second : emptyNodeSet_;
// }

// const std::set<TaintMap::NodePair>& TaintMap::getPhiFlows(NodeID node) const {
//     auto it = phiFlowMap_.find(node);
//     return it != phiFlowMap_.end() ? it->second : emptyPairSet_;
// }



// bool TaintMap::isAlias(NodeID n1, NodeID n2) const {
//     if (n1 == n2) return true;
//     auto it = aliasMap_.find(n1);
//     return it != aliasMap_.end() && it->second.count(n2);
// }

// bool TaintMap::hasDataFlow(NodeID src, NodeID dst) const {
//     auto it = dataFlowMap_.find(src);
//     return it != dataFlowMap_.end() && it->second.count(dst);
// }

// bool TaintMap::hasPhiFlow(NodeID src1, NodeID src2, NodeID dst) const {
//     // 标准化查询顺序
//     if (src1 > src2) std::swap(src1, src2);
    
//     auto it = phiFlowMap_.find(dst);
//     return it != phiFlowMap_.end() && it->second.count({src1, src2});
// }

// void TaintMap::addOperand(NodeID node) {
//     // 防止添加无效节点
//     if (node == 0) return;
    
//     // 避免重复添加
//     if (std::find(operands_vector.begin(), operands_vector.end(), node) == operands_vector.end()) {
//         operands_vector.push_back(node);
//     }
// }

// const std::vector<TaintMap::NodeID>& TaintMap::getOperands() const {
//     return operands_vector;
// }

// bool TaintMap::isOperand(NodeID node) const {
//     return std::find(operands_vector.begin(), operands_vector.end(), node) != operands_vector.end();
// }

// // 添加常量值映射
// void TaintMap::addConstantValue(NodeID node, const std::variant<int, float, std::string>& value) {
//     constantMap[node] = value;
// }


// const std::variant<int, float, std::string>& TaintMap::getConstantValue(NodeID node) const {
//     auto it = constantMap.find(node);
//     if(it == constantMap.end()) {
//         static std::variant<int, float, std::string> empty;
//         return empty;  // 返回空值表示错误
//     }
//     return it->second;
// }

// // 检查是否存在常量值映射
// bool TaintMap::hasConstantValue(NodeID node) const {
//     return constantMap.find(node) != constantMap.end();
// }


// void TaintMap::clear() {
//     aliasMap_.clear();
//     dataFlowMap_.clear();
//     phiFlowMap_.clear();
//     operands_vector.clear();
// }

// void TaintMap::dump() const {
//     SVFUtil::outs() << "===== TaintMap Dump =====\n";

//     // 打印别名关系
//     SVFUtil::outs() << "\n[Alias Relations]\n";
//     for (const auto& [node, aliases] : aliasMap_) {
//         SVFUtil::outs() << "  " << node << " <-> ";
//         for (NodeID alias : aliases) {
//             SVFUtil::outs() << alias << " ";
//         }
//         SVFUtil::outs() << "\n";
//     }

//     // 打印数据流关系
//     SVFUtil::outs() << "\n[Data Flow Relations]\n";
//     for (const auto& [src, dsts] : dataFlowMap_) {
//         for (NodeID dst : dsts) {
//             SVFUtil::outs() << "  " << src << " -> " << dst << "\n";
//         }
//     }

//     // 打印Phi流关系
//     SVFUtil::outs() << "\n[Phi Flow Relations]\n";
//     for (const auto& [dst, srcPairs] : phiFlowMap_) {
//         for (const auto& [src1, src2] : srcPairs) {
//             SVFUtil::outs() << "  " << src1 << " + " << src2 << " -> " << dst << "\n";
//         }
//     }

//     // 打印操作数向量（调试用）
//     if (!operands_vector.empty()) {
//         SVFUtil::outs() << "\n[Operands Vector]\n  ";
//         for (NodeID op : operands_vector) {
//             SVFUtil::outs() << op << " ";
//         }
//         SVFUtil::outs() << "\n";
//     }
// }
