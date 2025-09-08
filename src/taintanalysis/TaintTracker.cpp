#include "taintanalysis/TaintTracker.h"
#include "taintanalysis/CustomTaintAnalysis.h"
#include "taintanalysis/TaintUnit.h"   
#include "taintanalysis/TaintMap.h"
#include "napi/NapiHandler.h"
#include "SourceAndSinks/SourceAndSinks.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVFIR/SVFIR.h"
#include "MemoryModel/AccessPath.h"
#include "taintanalysis/SummaryItem.h"
#include "napi/utils/ParseVFG.h"
#include "JsonExporter/SummaryExporter.h"
using namespace SVF;
using namespace llvm;

// 正确实现构造函数（使用作用域解析运算符）
TaintTracker::TaintTracker(SVF::PAG* pag, SVF::Andersen* ander, SVF::SVFG* svfg, SVF::VFG* vfg) 
    : pag(pag), ander(ander), svfg(svfg), vfg(vfg) {
}

bool TaintTracker::isTainted(SVF::NodeID id) const{
    return taintedNodes.count(id);
}

void TaintTracker::initializeFunctionArgs(const llvm::Function* func) {
    // 初始化工作队列
    while (!worklist.empty()) {
        worklist.pop();
    }
    taintedNodes.clear();
    SVFUtil::outs() << "Initializing taint for function: " << func->getName().str() << "\n";
    
    // 遍历函数参数
    for (auto arg = func->arg_begin(); arg != func->arg_end(); ++arg) {
        // 第一个参数则continue
        // if (arg == func->arg_begin()) {
        //     continue;
        // }
        
        NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(&*arg);
        
        if (argNodeId != 0) { // 过滤无效节点
            SVFUtil::outs() << "  Argument: " << arg->getName().str() 
                          << " (NodeID: " << argNodeId << ")\n";
            
            // 将参数节点标记为污染源
            taintedNodes.insert(argNodeId);
            // 将参数节点加入工作队列
            worklist.push(argNodeId);
        }
        else {
            SVFUtil::outs() << "  [Warning] Invalid node for argument: " 
                          << arg->getName().str() << "\n";
        }
    }
}





bool TaintTracker::isFunctionReturnTainted(const llvm::Function* func) {
    // 遍历函数中的所有返回指令
    for (const auto& bb : *func) {
        if (const llvm::ReturnInst* retInst = llvm::dyn_cast<llvm::ReturnInst>(bb.getTerminator())) {
            // 获取返回值的SVF表示
            NodeID retNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(retInst->getReturnValue());
            // 检查返回值是否被污染
            if (isTainted(retNodeId)) {
                return true;
            }
        }
    }
    return false;
}

TaintUnit TaintTracker::handleDirectAssignment(const llvm::Instruction* inst) {
    std::string opName = inst->getOpcodeName();
    if (opName.empty()) {
        opName = "UnnamedOp";
    }
    TaintUnit taintUnit(opName, "DirectAssignment");

    // 获取结果的SVF表示
    NodeID resultNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(inst);
    
    // 如果操作数被污染，标记结果为污染
    if (resultNodeId != 0 && isInstructionTainted(inst)) {
        // 记录原有污点节点
        for (const auto& op : inst->operands()) {
            if (llvm::isa<llvm::BasicBlock>(op)) {
                continue;
            }
            NodeID opNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(op);
            if (opNodeId != 0 && isTainted(opNodeId)) {
                taintUnit.addOriginalTaintedNode(opNodeId);
            }
        }

        // 记录指令参数
        for (const auto& op : inst->operands()) {
            if (llvm::isa<llvm::BasicBlock>(op)) {
                continue;
            }
            NodeID opNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(op);
            if (opNodeId != 0) {
                taintUnit.addInstructionArg(opNodeId);
            }
        }

        // 标记结果为污染
        taintedNodes.insert(resultNodeId);
        taintUnit.addNewTaintedNode(resultNodeId);
        SVFUtil::outs() << "Tainted result: " << inst->getName().str() << "\n";
    }

    return taintUnit;
}


TaintUnit TaintTracker::handlePhiInstruction(const llvm::Instruction* inst) {
    const llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(inst);
    if (!phi) return TaintUnit("", "");

    std::string name = "PhiIn:" + phi->getParent()->getName().str();
    TaintUnit taintUnit(name, "Phi");

    // 获取结果的SVF表示
    NodeID resultNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(phi);
    if (resultNodeId == 0) return taintUnit;

    // 检查所有来源值
    bool isTaint = false;
    for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
        const llvm::Value* incomingValue = phi->getIncomingValue(i);
        // 获取来源值的SVF表示
        NodeID incomingNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(incomingValue);
        
        // 记录指令参数
        if (incomingNodeId != 0) {
            taintUnit.addInstructionArg(incomingNodeId);
        }

        // 如果任一来源值被污染
        if (incomingNodeId != 0 && isTainted(incomingNodeId)) {
            isTaint = true;
            taintUnit.addOriginalTaintedNode(incomingNodeId);
        }
    }

    // 如果任一来源值被污染，标记结果为污染
    if (isTaint) {
        taintedNodes.insert(resultNodeId);
        taintUnit.addNewTaintedNode(resultNodeId);
        SVFUtil::outs() << "Tainted phi result: " << phi->getName().str() << "\n";
    }

    return taintUnit;
}


// 处理返回指令
TaintUnit TaintTracker::handleReturnInstruction(const llvm::Instruction* inst) {
    const llvm::ReturnInst* retInst = llvm::dyn_cast<llvm::ReturnInst>(inst);
    if (!retInst) return TaintUnit("", "");

    std::string name = "ReturnValue";
    if (retInst->getReturnValue()) {
        name = retInst->getReturnValue()->getName().str();
        if (name.empty()) {
            name = "Ret";
        }
    }
    TaintUnit taintUnit(name, "Return");

    // 处理返回值污染传播
    if (retInst->getReturnValue()) {
        // 获取返回值的SVF表示
        NodeID retNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(retInst->getReturnValue());
        
        // 记录原有污点节点
        if (retNodeId != 0 && isTainted(retNodeId)) {
            taintUnit.addOriginalTaintedNode(retNodeId);
        }

        // 记录指令参数
        if (retNodeId != 0) {
            taintUnit.addInstructionArg(retNodeId);
        }
    }

    return taintUnit;
}






std::vector<const Function *> TaintTracker::getCalledFunctions(const Function *F, std::set<const Function*>& visited)
{
    std::vector<const Function *> calledFunctions;
    if (visited.count(F)) {
        return calledFunctions; // 已处理过，避免循环
    }
    visited.insert(F);

    
    for (const Instruction &I : instructions(F))
    {
        I.print(llvm::outs());
        SVFUtil::outs() << "\n";
        if (const CallBase *callInst = SVFUtil::dyn_cast<CallBase>(&I))
        {
            const Function *calledFunction = callInst->getCalledFunction();
            if (!calledFunction) {
                const Value *calleeV = callInst->getCalledOperand();
                if (calleeV)
                    calleeV = calleeV->stripPointerCasts();
                calledFunction = SVFUtil::dyn_cast<Function>(calleeV);
            }
            if (calledFunction)
            {
                calledFunctions.push_back(calledFunction);
                targetedinst.push_back(&I);
                std::vector<const Function *> nestedCalledFunctions = getCalledFunctions(calledFunction, visited);
                calledFunctions.insert(calledFunctions.end(), nestedCalledFunctions.begin(), nestedCalledFunctions.end());
            }
        }
    }
    return calledFunctions;
}



nlohmann::json TaintTracker::Traceker(const llvm::Function* func, std::vector<std::pair<NodeID, std::string>> paramNodeIDs, std::string funcName) {
    nlohmann::json result;
    // 初始化，清空
    targetedfunctions.clear();
    targetedinst.clear();
    std::set<const Function*> visitedFunctions;
    targetedfunctions = TaintTracker::getCalledFunctions(func,visitedFunctions);
    // 打印targetedfunctions
    TaintMap taintMap(paramNodeIDs, ander);
    std::vector<SummaryItem> summaryItems;
    
    SVFUtil::outs() << "Targeted functions:\n";
    for (const auto& func : targetedfunctions) {
        SVFUtil::outs() << "  " << func->getName().str() << "\n";
        for (auto &arg : func->args()) {
            const Value* argVal = &arg;
            NodeID nodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argVal);
            SVFVar* svfVar = pag->getGNode(nodeId);
            std::cout << "Argument: " << arg.getName().str() << ", NodeID: " << nodeId << std::endl;
            // 打印nodeid对应的svfvar的值和在llvm中的名称
            if (svfVar) {
                std::cout << "SVFVar Value: " << svfVar->toString() << std::endl;
                std::cout << "LLVM Name: " << svfVar->getValueName() << std::endl;
            }
        }
        SVFUtil::outs() << "\n";
    }
    // 打印targetedinst
    SVFUtil::outs() << "Targeted instructions:\n";
    std::set<NodeID> targetidsets;
    for (const auto& inst : targetedinst) {
        inst->print(llvm::outs());
        SVFUtil::outs() << "\n";
        if (const CallBase* cb = SVFUtil::dyn_cast<CallBase>(inst)) {
            for (unsigned i = 0; i < cb->arg_size(); ++i) {
                const Value *operand = cb->getArgOperand(i);
                NodeID nodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(operand);
                if (targetidsets.count(nodeId) == 0) {
                    targetidsets.insert(nodeId);
                }
                std::cout << "Arg " << i << ": " << operand->getName().str() << ", NodeID: " << nodeId << std::endl;
                SVFVar* svfVar = pag->getGNode(nodeId);
                if (svfVar) {
                    std::cout << "SVFVar Value: " << svfVar->toString() << std::endl;
                    std::cout << "LLVM Name: " << svfVar->getValueName() << std::endl;
                }
            }
        } else {
            for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
                const Value *operand = inst->getOperand(i);
                NodeID nodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(operand);
                if (targetidsets.count(nodeId) == 0) {
                    targetidsets.insert(nodeId);
                }
                std::cout << "Operand " << i << ": " << operand->getName().str() << ", NodeID: " << nodeId << std::endl;
                SVFVar* svfVar = pag->getGNode(nodeId);
                if (svfVar) {
                    std::cout << "SVFVar Value: " << svfVar->toString() << std::endl;
                    std::cout << "LLVM Name: " << svfVar->getValueName() << std::endl;
                }
            }
        }
        SVFUtil::outs() << "\n";
        // 如果inst是call指令且调用函数名称为napi_get_cb_info，则调用NapiGetCallBackInfo
        if (llvm::isa<llvm::CallInst>(inst)) {
            NapiHandler::getInstance().dispatch(inst, taintMap, svfg, pag, ander, summaryItems);
        }
    }
    // 处理返回值
    const FunObjVar* funObjVar = LLVMModuleSet::getLLVMModuleSet()->getFunObjVar(func);
    const SVFVar* funRet = pag->getFunRet(funObjVar);
    // nodeid
    NodeID funRetNodeID = funRet->getId();
    if (funRet) {
        SVFUtil::outs() << "Function return: " << funRet->getValueName() << "\n";
        // 打印funRet的值
        std::cout << "Function return value: " << funRet->toString() << std::endl;
        // 打印funRet的nodeid
        std::cout << "Function return nodeID: " << funRet->getId() << std::endl;
        std::vector<SVF::NodeID> funRetNodeIDs = bfsPredecessors(svfg, pag, funRet);
        std::vector<SVF::NodeID> existingNodeIDs = getTaintmapExistingNodes(funRetNodeIDs, taintMap, ander);
        if(existingNodeIDs.size() == 1) {
            int funRetID = taintMap.getNewIds(existingNodeIDs[0])[0];
            std::cout << "Function return nodeID: " << funRetID << std::endl;
            SummaryItem retItem("", "Ret");
            retItem.addOperand("%"+std::to_string(funRetID));
            summaryItems.push_back(retItem);
        }
        else if(existingNodeIDs.size() > 1) {
            int firstID = taintMap.getNewIds(existingNodeIDs[0])[0];
            int secondID = taintMap.getNewIds(existingNodeIDs[1])[0];
            // 判断secondID是否再taintMap.getValueFlowSources(firstID)中
            std::vector<int> sources = taintMap.getValueFlowSources(firstID);
            if(std::find(sources.begin(), sources.end(), secondID) != sources.end()) {
                std::cout << "Function return nodeID: " << firstID << " and " << secondID << std::endl;
                SummaryItem retItem("", "Ret");
                retItem.addOperand("%"+std::to_string(firstID));
                summaryItems.push_back(retItem);
            }
            else {
                std::cout << "Function return nodeID: " << firstID << " and " << secondID << std::endl;
                // 如果firstID和secondID相同，直接返回这个ID，不需要phi
                if (firstID == secondID) {
                    SummaryItem retItem("", "Ret");
                    retItem.addOperand("%"+std::to_string(firstID));
                    summaryItems.push_back(retItem);
                } else {
                    // 添加一个phi
                    SummaryItem phiItem("", "Phi");
                    phiItem.addOperand("%"+std::to_string(firstID));
                    phiItem.addOperand("%"+std::to_string(secondID));
                    int phiID = taintMap.assignNewId(funRetNodeID);
                    phiItem.addRetValue("%"+std::to_string(phiID), -1);
                    // 添加ret
                    SummaryItem retItem("", "Ret");
                    retItem.addOperand("%"+std::to_string(phiID));
                    summaryItems.push_back(phiItem);
                    summaryItems.push_back(retItem);
                }
            }
        }
        else{
            SummaryItem retItem("", "Ret");
            retItem.addOperand("top");
            summaryItems.push_back(retItem);
        }
    }
    SVFUtil::outs() << "\n";
    result["name"] = funcName;
    nlohmann::json paramsJson;
    const auto& paramInfos = taintMap.getParamIds();
    for (const auto& paramInfo : paramInfos) {
        std::string key = "%" + std::to_string(paramInfo.paramId);
        paramsJson[key] = paramInfo.paramName;
    }
    result["params"] = paramsJson;
    result["instructions"] = SummaryExporter::toJson(summaryItems);

    return result;
}



void TaintTracker::aliasAnalysis(SVF::NodeID id) {
    // 获取id的alias
    for (auto node : taintedNodes) {
        if (ander->alias(id, node) == SVF::AliasResult::MayAlias) {
            if(taintedNodes.count(id) == 0) {
                taintedNodes.insert(id);
                worklist.push(id);
                if(nodeIDMap.count(node) == 0) {
                    nodeIDMap[node] = std::vector<SVF::NodeID>();
                }
                if(std::find(nodeIDMap[node].begin(), nodeIDMap[node].end(), id) == nodeIDMap[node].end()) {
                    nodeIDMap[node].push_back(id);
                }
            }
        }
    }
}

void TaintTracker::printNodeID(SVF::NodeID nodeId) {
    SVF::SVFIR* svfir = SVF::SVFIR::getPAG();
    if (!svfir->hasGNode(nodeId)) {
        return;
    }
    SVF::SVFVar* node = svfir->getGNode(nodeId);
    // 检查是否是虚拟节点
    if (node->getNodeKind() == SVF::SVFValue::DummyValNode || 
        node->getNodeKind() == SVF::SVFValue::DummyObjNode) {
        llvm::outs() << "NodeID: " << nodeId << " is a dummy node\n";
        return;
    }
    std::string valName = node->getValueName();
    if (!valName.empty()) {
        llvm::outs() << "NodeID: " << nodeId << " Value: " << valName << "\n";
    }
    else {
        llvm::outs() << "NodeID: " << nodeId << " Value: " << "Unnamed" << "\n";
    }
}

void TaintTracker::trackValueFlow(SVF::NodeID srcNode) {
    // todo：防止重复递归
    if(visitedNodes.count(srcNode) != 0) {
        return;
    }
    visitedNodes.insert(srcNode);
    printNodeID(srcNode);
    // const SVF::PointsTo& pts = ander->getPts(srcNode);

    // for (auto obj : pts) {
    //     // 这里的 obj 是 srcNode 可能指向的对象
    //     trackValueFlow(obj);
    // }
    // 获取源节点的VFG节点
    SVF::VFGNode* srcVFGNode = svfg->getVFGNode(srcNode);
    if (!srcVFGNode) return;

    // 打印getNodekind
    SVFUtil::outs() << "Node kind: " << srcVFGNode->getNodeKind() << "\n";
    // // 先遍历getinEdges，再从inedges出发
    // for (auto& edge : srcVFGNode->getInEdges()) {
    //     SVF::VFGNode* srcVFGNode = edge->getSrcNode();
    //     SVF::NodeID edgesrcNode = srcVFGNode->getId();
    //     trackValueFlow(edgesrcNode);
    // }
    // 遍历所有直接后继节点
    for (auto& edge : srcVFGNode->getOutEdges()) {
        SVF::VFGNode* dstVFGNode = edge->getDstNode();
        SVF::NodeID dstNode = dstVFGNode->getId();
        printNodeID(dstNode);
        
        // 将传播关系记录到nodeIDMap中
        if(nodeIDMap.count(srcNode) == 0) {
            nodeIDMap[srcNode] = std::vector<SVF::NodeID>();
        }
        if(std::find(nodeIDMap[srcNode].begin(), nodeIDMap[srcNode].end(), dstNode) == nodeIDMap[srcNode].end()) {
            nodeIDMap[srcNode].push_back(dstNode);
        }
        if(taintedNodes.count(dstNode) == 0) {
            taintedNodes.insert(dstNode);
            worklist.push(dstNode);
        }
    }
}


bool TaintTracker::isInstructionTainted(const llvm::Instruction* inst) {
    
    // 遍历指令的每个操作数
    for (const auto& op : inst->operands()) {
        // 获取操作数的SVF表示
        // 如果是基本块，则跳过
        if (llvm::isa<llvm::BasicBlock>(op)) {
            continue;
        }
        NodeID opNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(op);
        if (llvm::isa<llvm::Constant>(op)) {
            continue;
        }

        if (opNodeId == 0) {
            continue;
        }
        // aliasAnalysis需要仔细检查
        // aliasAnalysis(opNodeId);
    }
    if (!llvm::isa<llvm::CallInst>(inst) && !llvm::isa<llvm::InvokeInst>(inst) && !llvm::isa<llvm::ReturnInst>(inst)) {
        return false;
    }
    return true;
}



