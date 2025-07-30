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


TaintUnit TaintTracker::handleLoadInstruction(const llvm::Instruction* inst) {
    const llvm::LoadInst* loadInst = llvm::dyn_cast<llvm::LoadInst>(inst);
    if (!loadInst) return TaintUnit("", "");

    std::string name = loadInst->getPointerOperand()->getName().str();
    if (name.empty()) {
        name = "LoadFrom:" + std::to_string(reinterpret_cast<uintptr_t>(loadInst));
    }
    TaintUnit taintUnit(name, "Load");

    // 获取加载的地址操作数
    const llvm::Value* ptrOperand = loadInst->getPointerOperand();
    
    // 获取地址的PAG节点ID
    NodeID ptrNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(ptrOperand);
    
    // 记录原有污点节点
    if (isTainted(ptrNodeId)) {
        taintUnit.addOriginalTaintedNode(ptrNodeId);
    }

    // 记录指令参数
    taintUnit.addInstructionArg(ptrNodeId);

    // 如果地址被污染
    if (isTainted(ptrNodeId)) {
        NodeID resultNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(loadInst);
        
        // 将加载结果标记为污染
        if (resultNodeId != 0) {
            taintedNodes.insert(resultNodeId);
            taintUnit.addNewTaintedNode(resultNodeId);
            SVFUtil::outs() << "Tainted load result: " << loadInst->getName().str() << "\n";
        }
    }

    return taintUnit;
}

TaintUnit TaintTracker::handleStoreInstruction(const llvm::Instruction* inst) {
    const llvm::StoreInst* storeInst = llvm::dyn_cast<llvm::StoreInst>(inst);
    if (!storeInst) return TaintUnit("", "");

    
    std::string name = storeInst->getPointerOperand()->getName().str();
    if (name.empty()) {
        name = "StoreTo:" + std::to_string(reinterpret_cast<uintptr_t>(storeInst));
    }
    TaintUnit taintUnit(name, "Store");

    // 获取存储的值操作数
    const llvm::Value* valueOperand = storeInst->getValueOperand();

    // 获取值的PAG节点ID
    NodeID valueNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueOperand);
    
    // 记录原有污点节点
    if (isTainted(valueNodeId)) {
        taintUnit.addOriginalTaintedNode(valueNodeId);
    }

    // 记录指令参数
    taintUnit.addInstructionArg(valueNodeId);

    // 如果值被污染
    if (isTainted(valueNodeId)) {
        NodeID ptrNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(storeInst->getPointerOperand());
        
        // 将目标地址标记为污染
        if (ptrNodeId != 0) {
            taintedNodes.insert(ptrNodeId);
            taintUnit.addNewTaintedNode(ptrNodeId);
            SVFUtil::outs() << "Tainted store address: " << storeInst->getPointerOperand()->getName().str() << "\n";
        }
    }

    return taintUnit;
}

std::vector<TaintUnit> TaintTracker::handleInvokeInstruction(const llvm::Instruction* inst) {
    const llvm::InvokeInst* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(inst);
    if (!invokeInst) return std::vector<TaintUnit>();

    // 复用Call指令处理的核心逻辑
    std::vector<TaintUnit> taintUnits = handleCallBase(invokeInst);
    
    return taintUnits;
}

std::vector<TaintUnit> TaintTracker::handleCallBase(const llvm::CallBase* callBase) {
    std::vector<TaintUnit> results;
    TaintUnit result;
    
    // 获取被调用函数信息
    const llvm::Function* callee = callBase->getCalledFunction();
    std::string funcName = callee ? callee->getName().str() : "indirect_call";
    result.setInstructionType("Call");
    result.setName(funcName);

    // 记录所有参数节点（公共逻辑）
    for (unsigned i = 0; i < callBase->arg_size(); ++i) {
        const llvm::Value* arg = callBase->getArgOperand(i);
        NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arg);
        if (argNodeId != 0) {
            result.addInstructionArg(argNodeId);
            if (isTainted(argNodeId)) {
                result.addOriginalTaintedNode(argNodeId);
            }
        }
    }

    // 处理间接调用（公共逻辑）
    if (!callee) {
        // handleIndirectCall(callBase); 
        results.push_back(result);
        return results;
    }

    // 实函数/声明函数处理（整合自handleCallInstruction）
    if (callee->isDeclaration() || callee->isIntrinsic()) {
        if (CustomTaintAnalysis::isNapiFunction(callee)) {
            // 将CallBase安全转换为CallInst
            if (const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(callBase)) {
                std::unordered_map<SVF::NodeID, std::vector<SVF::NodeID>> returnNodes_map = 
                    CustomTaintAnalysis::handleNapiCall(callInst, pag, taintedNodes);
                for (auto node_map : returnNodes_map) {
                    for (auto node : node_map.second) {
                        if(nodeIDMap.count(node_map.first) == 0) {
                            nodeIDMap[node_map.first] = std::vector<SVF::NodeID>();
                        }
                        if(std::find(nodeIDMap[node_map.first].begin(), nodeIDMap[node_map.first].end(), node) == nodeIDMap[node_map.first].end()) {
                            nodeIDMap[node_map.first].push_back(node);
                        }
                        if(taintedNodes.count(node) == 0) {
                            taintedNodes.insert(node);
                            worklist.push(node);
                        }
                        result.addNewTaintedNode(node);
                    }
                }
            }
            else {
                SVFUtil::outs() << "[Warning] NAPI function called via invoke instruction: " 
                            << callee->getName().str() << "\n";
            }
        }
        results.push_back(result);
        return results;
    }

    // 参数传播（增强版）
    for (unsigned i = 0; i < callBase->arg_size(); ++i) {
        if (const llvm::Value* arg = callBase->getArgOperand(i)) {
            NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arg);
            if (isTainted(argNodeId)) {
                propagateTaintToCalleeArg(callee, i, argNodeId);
            }
        }
    }

    // 递归传播（公共逻辑）
    std::vector<TaintUnit> taintcalleeUnits = propagateTaint(callee);
    results.insert(results.end(), taintcalleeUnits.begin(), taintcalleeUnits.end());

    // 返回值处理（增强版）
    if (!callBase->getType()->isVoidTy()) {
        if (isFunctionReturnTainted(callee)) {
            NodeID resultNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(callBase);
            if(resultNodeId != 0) {
                taintedNodes.insert(resultNodeId);
                result.addNewTaintedNode(resultNodeId);
                for (unsigned i = 0; i < callBase->arg_size(); ++i) {
                    const llvm::Value* arg = callBase->getArgOperand(i);
                    NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arg);
                    if (isTainted(argNodeId)) {
                        if(nodeIDMap.count(argNodeId) == 0) {
                            nodeIDMap[argNodeId] = std::vector<SVF::NodeID>();
                        }
                        if(std::find(nodeIDMap[argNodeId].begin(), nodeIDMap[argNodeId].end(), resultNodeId) == nodeIDMap[argNodeId].end()) {
                            nodeIDMap[argNodeId].push_back(resultNodeId);
                        }
                    }
                }
                // SVFUtil::outs() << "Tainted call result: " << callBase->getName().str() << "\n";
            }
        }
    }
    results.insert(results.begin(), result);
    return results;
}


// 后续Call和Invoke可以合在一起
std::vector<TaintUnit> TaintTracker::handleCallInstruction(const llvm::Instruction* inst) {
    std::vector<TaintUnit> results;
    const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!callInst) return std::vector<TaintUnit>();

    std::string funcName = "indirect_call";
    if (const llvm::Function* callee = callInst->getCalledFunction()) {
        funcName = callee->getName().str();
    }
    
    TaintUnit taintUnit(funcName, "Call");

    // 获取被调用函数
    const llvm::Function* callee = callInst->getCalledFunction();
    
    // 处理间接调用（函数指针）
    if (!callee) {
        // 处理间接调用情况
        results.push_back(taintUnit);
        return results;
    }

    // 记录所有参数
    for (unsigned i = 0; i < callInst->arg_size(); ++i) {
        const llvm::Value* arg = callInst->getArgOperand(i);
        NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arg);
        taintUnit.addInstructionArg(argNodeId);
        

        // 如果参数被污染，记录原有污点节点
        if (isTainted(argNodeId)) {
            taintUnit.addOriginalTaintedNode(argNodeId);
        }
    }

    // 检查是否是实函数（有函数体）
    if (callee->isDeclaration() || callee->isIntrinsic()) {
        // 判断是不是napi函数
        if (CustomTaintAnalysis::isNapiFunction(callee)) {
            std::unordered_map<SVF::NodeID,  std::vector<SVF::NodeID>> returnNodes_map = CustomTaintAnalysis::handleNapiCall(callInst, pag, taintedNodes);
            
            // 记录新增污点节点
            for (auto node_map : returnNodes_map) {
                for (auto node : node_map.second) {
                    if(nodeIDMap.count(node_map.first) == 0) {
                        nodeIDMap[node_map.first] = std::vector<SVF::NodeID>();
                    }
                    if(std::find(nodeIDMap[node_map.first].begin(), nodeIDMap[node_map.first].end(), node) == nodeIDMap[node_map.first].end()) {
                        nodeIDMap[node_map.first].push_back(node);
                    }
                    if(taintedNodes.count(node) == 0) {
                        taintedNodes.insert(node);
                        worklist.push(node);
                    }
                    taintUnit.addNewTaintedNode(node);
                }
            }
        }
        results.push_back(taintUnit);
        return results;
    }

    // 处理参数传播
    for (unsigned i = 0; i < callInst->arg_size(); ++i) {
        const llvm::Value* arg = callInst->getArgOperand(i);
        NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arg);
        if (isTainted(argNodeId)) {
            // 将污点传播到被调用函数的参数
            propagateTaintToCalleeArg(callee, i, argNodeId);
        }
    }

    // 递归传播污点
    std::vector<TaintUnit> taintcalleeUnits = propagateTaint(callee);
    results.insert(results.end(), taintcalleeUnits.begin(), taintcalleeUnits.end());

    // 处理返回值传播
    if (!callInst->getType()->isVoidTy()) {
        if (isFunctionReturnTainted(callee)) {
            NodeID resultNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(callInst);
            if (resultNodeId != 0) {
                taintedNodes.insert(resultNodeId);
                taintUnit.addNewTaintedNode(resultNodeId);
                for (unsigned i = 0; i < callInst->arg_size(); ++i) {
                    const llvm::Value* arg = callInst->getArgOperand(i);
                    NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(arg);
                    if (isTainted(argNodeId)) {
                        if(nodeIDMap.count(argNodeId) == 0) {
                            nodeIDMap[argNodeId] = std::vector<SVF::NodeID>();
                        }
                        if(std::find(nodeIDMap[argNodeId].begin(), nodeIDMap[argNodeId].end(), resultNodeId) == nodeIDMap[argNodeId].end()) {
                            nodeIDMap[argNodeId].push_back(resultNodeId);
                        }
                    }
                }
                // SVFUtil::outs() << "Tainted call result: " << callInst->getName().str() << "\n";
            }
        }
    }
    // 将taintUnit插入taintcalleeUnits最前面
    results.insert(results.begin(), taintUnit);
    return results;
}

void TaintTracker::propagateTaintToCalleeArg(const llvm::Function* callee, unsigned argIdx, NodeID srcNodeId) {
    // 获取被调用函数的参数
    auto argIt = callee->arg_begin();
    std::advance(argIt, argIdx);
    
    // 获取参数的SVF表示
    NodeID argNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(&*argIt);
    
    // 标记参数为污染
    if (argNodeId != 0) {
        if(nodeIDMap.count(srcNodeId) == 0) {
            nodeIDMap[srcNodeId] = std::vector<SVF::NodeID>();
        }
        if(std::find(nodeIDMap[srcNodeId].begin(), nodeIDMap[srcNodeId].end(), argNodeId) == nodeIDMap[srcNodeId].end()) {
            nodeIDMap[srcNodeId].push_back(argNodeId);
        }
        if(taintedNodes.count(argNodeId) == 0) {
            taintedNodes.insert(argNodeId);
        }
        // SVFUtil::outs() << "Tainted callee argument: " << argIt->getName().str() << "\n";
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


std::vector<TaintUnit> TaintTracker::handleTaintedInstruction(const llvm::Instruction* inst) {
    // 打印一下指令
    // SVFUtil::outs() << "Handling call instruction: " << inst->getOpcodeName() << "\n";
    TaintUnit taintUnit;
    std::vector<TaintUnit> taintUnits;
    // inst->print(llvm::outs());
    // SVFUtil::outs() << "\n";
    switch (inst->getOpcode()) {
        case llvm::Instruction::Load:
            taintUnit = handleLoadInstruction(inst);
            break;
        case llvm::Instruction::Store:
            taintUnit = handleStoreInstruction(inst);
            break;
        case llvm::Instruction::Call:
            taintUnits = handleCallInstruction(inst);
            break;
        case llvm::Instruction::Invoke:
            taintUnits = handleInvokeInstruction(inst);
            break;
        // 处理phi
        case llvm::Instruction::PHI:
            taintUnit = handlePhiInstruction(inst);
            break;
        // 直接传播污点指令合集：
        case llvm::Instruction::Add:
        case llvm::Instruction::FAdd:
        case llvm::Instruction::Sub:
        case llvm::Instruction::FSub:
        case llvm::Instruction::Mul:
        case llvm::Instruction::FMul:
        case llvm::Instruction::UDiv:
        case llvm::Instruction::SDiv:
        case llvm::Instruction::FDiv:
        case llvm::Instruction::URem:
        case llvm::Instruction::SRem:
        case llvm::Instruction::FRem:
        case llvm::Instruction::Shl:
        case llvm::Instruction::LShr:
        case llvm::Instruction::AShr:
        case llvm::Instruction::And:
        case llvm::Instruction::Or:
        case llvm::Instruction::Xor:
        case llvm::Instruction::BitCast:
        case llvm::Instruction::Trunc:
        case llvm::Instruction::ZExt:
        case llvm::Instruction::SExt:
        case llvm::Instruction::FPToUI:
        case llvm::Instruction::FPToSI:
        case llvm::Instruction::UIToFP:
        case llvm::Instruction::SIToFP:
        case llvm::Instruction::FPTrunc:
        case llvm::Instruction::FPExt:
        case llvm::Instruction::PtrToInt:
        case llvm::Instruction::IntToPtr:
        case llvm::Instruction::GetElementPtr:
            taintUnit = handleDirectAssignment(inst);
            break;
        case llvm::Instruction::Ret:
            taintUnit = handleReturnInstruction(inst);
            break;
        default:
            // handleDefaultInstruction(inst);
            break;
    }
    if (taintUnits.size() == 0) {
        taintUnits.push_back(taintUnit);
    }
    return taintUnits;
}



std::vector<TaintUnit> TaintTracker::propagateTaint(const llvm::Function* func) {
    std::vector<TaintUnit> taintresults;

    // 遍历函数中的每条基本块
    for (const auto& bb : *func) {
        // 遍历基本块中的每条指令
        for (const auto& inst : bb) {
            // 如果工作队列不为空，先执行传播
            inst.print(llvm::outs());
            SVFUtil::outs() << "\n";
            while (!worklist.empty()) {
                // 打印worklist.front()
                NodeID nodeId_trace = worklist.front();
                printNodeID(nodeId_trace);
                trackValueFlow(worklist.front());
                worklist.pop();
            }
            
            if (isInstructionTainted(&inst)) {
                // 根据指令类型执行传播逻辑
                std::vector<TaintUnit> taintUnits = handleTaintedInstruction(&inst);
                taintresults.insert(taintresults.end(), taintUnits.begin(), taintUnits.end());
            }
        }
    }
    return taintresults;
}

void TaintTracker::traverseFunction(const llvm::Function* func) {
    NapiPropagationRules napiRules = NapiPropagationRules();
    SourceAndSinks sourceAndSinks = SourceAndSinks();
    for (const llvm::BasicBlock& bb : *func) {
        for (const llvm::Instruction& inst : bb) {
            // 打印inst
            inst.print(llvm::outs());
            SVFUtil::outs() << "\n";
            if (LLVMUtil::isCallSite(&inst)) {
                const llvm::CallBase* callInst = LLVMUtil::getLLVMCallSite(&inst);
                const llvm::Function* calledFunction = LLVMUtil::getCallee(callInst);

                if (calledFunction) {
                    if (napiRules.is_napi_function(calledFunction->getName().str()) || sourceAndSinks.is_sink_methods(calledFunction->getName().str())) {
                        // 打印被调用函数名
                        SVFUtil::outs() << "Called function: " << calledFunction->getName().str() << "\n";
                        targetedfunctions.push_back(calledFunction);
                        targetedinst.push_back(&inst);
                    }
                    // 递归调用被调用的函数
                    traverseFunction(calledFunction);
                }
            }
            // 处理invoke指令
            else if (llvm::isa<llvm::InvokeInst>(&inst)) {
                const llvm::InvokeInst* invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&inst);
                const llvm::Function* calledFunction = invokeInst->getCalledFunction();
                if (calledFunction && !calledFunction->isDeclaration()) {
                    if (napiRules.is_napi_function(calledFunction->getName().str()) || sourceAndSinks.is_sink_methods(calledFunction->getName().str())) {
                        targetedfunctions.push_back(calledFunction);
                        targetedinst.push_back(&inst);
                    }
                    // 递归调用被调用的函数
                    traverseFunction(calledFunction);
                }
            }
        }
    }
}



nlohmann::json TaintTracker::Traceker(const llvm::Function* func, std::vector<std::pair<NodeID, std::string>> paramNodeIDs, std::string funcName) {
    nlohmann::json result;
    // 初始化，清空
    targetedfunctions.clear();
    targetedinst.clear();
    traverseFunction(func);
    // 打印targetedfunctions
    TaintMap taintMap(paramNodeIDs);
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
        for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
            Value *operand = inst->getOperand(i);
            NodeID nodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(operand); // 使用SVFIRBuilder调用getValueNode
            if (targetidsets.count(nodeId) == 0) {
                targetidsets.insert(nodeId);
            }
            std::cout << "Operand " << i << ": " << operand->getName().str() << ", NodeID: " << nodeId << std::endl;
            // 打印nodeid对应的svfvar的值和在llvm中的名称
            SVFVar* svfVar = pag->getGNode(nodeId);
            if (svfVar) { 
                std::cout << "SVFVar Value: " << svfVar->toString() << std::endl;
                std::cout << "LLVM Name: " << svfVar->getValueName() << std::endl;
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
        std::vector<SVF::NodeID> funRetNodeIDs = bfsPredecessors(svfg, funRet, pag);
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



