#include "taintanalysis/CustomTaintAnalysis.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/Options.h"


using namespace SVF;
using namespace llvm;

bool CustomTaintAnalysis::isNapiFunction(const llvm::Function* callee){
    if (!callee) return false;

    // 获取函数名
    std::string funcName = callee->getName().str();
    
    // 检查是否在rules中
    return NapiPropagationRules::getInstance().hasRule(funcName);
}

bool CustomTaintAnalysis::isTainted(SVF::NodeID id, std::set<SVF::NodeID>& taintedNodes) {
    return taintedNodes.count(id);
}

std::unordered_map<SVF::NodeID, std::vector<SVF::NodeID>> CustomTaintAnalysis::handleNapiCall(const llvm::CallInst* callInst, SVF::PAG* pag, std::set<SVF::NodeID>& taintedNodes) {
    std::unordered_map<SVF::NodeID, std::vector<SVF::NodeID>> taintedMap;
    
    // 获取被调用函数名
    std::string funcName = callInst->getCalledFunction()->getName().str();
    
    
    // 获取传播规则实例
    NapiPropagationRules& rules = NapiPropagationRules::getInstance();
    
    // 检查是否有该函数的传播规则
    if (!rules.hasRule(funcName)) {
        return taintedMap;
    }
    
    // 获取该函数的传播规则
    const NapiPropagationRules::SrcToDstMap& rule = rules.getRule(funcName);
    
    // 遍历传播规则
    for (const auto& [srcIdx, dstIndices] : rule) {
        // 获取源参数
        const llvm::Value* srcArg = callInst->getArgOperand(srcIdx);
        NodeID srcNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(srcArg);
        
        // 如果源参数被污染
        if (isTainted(srcNodeId,taintedNodes)) {
            // 处理napi_get_cb_info
            // if (funcName == "napi_get_cb_info") {
            //     std::vector<SVF::NodeID> taintedArgs2 = handleNapiGetCbInfo(callInst, pag);
            //     for (auto nodeId : taintedArgs2) {
            //         taintedArgs.push_back(nodeId);
            //     }
            // }
            // 遍历目标参数索引
            for (int dstIdx : dstIndices) {
                // 获取目标参数
                const llvm::Value* dstArg = callInst->getArgOperand(dstIdx);
                NodeID dstNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(dstArg);
                
                // 将目标参数加入污染列表
                if (dstNodeId != 0) {
                    if(taintedMap.count(srcNodeId) == 0) {
                        taintedMap[srcNodeId] = std::vector<SVF::NodeID>();
                    }
                    if(std::find(taintedMap[srcNodeId].begin(), taintedMap[srcNodeId].end(), dstNodeId) == taintedMap[srcNodeId].end()) {
                        taintedMap[srcNodeId].push_back(dstNodeId);
                    }
                }
            }
        }
    }
    
    return taintedMap;
}


std::vector<SVF::NodeID> CustomTaintAnalysis::handleNapiGetCbInfo(const llvm::CallInst* callInst, SVF::PAG* pag) {
    std::vector<SVF::NodeID> taintedArgs;
    if (callInst->arg_size() < 4) {
        SVFUtil::outs() << "[Error] Invalid napi_get_cb_info call\n";
        return taintedArgs;
    }

    // 获取argv参数节点
    const llvm::Value* argvArg = callInst->getArgOperand(3);
    NodeID argvNode = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argvArg);
    if (!argvNode) {
        SVFUtil::outs() << "[Warning] Cannot get SVFValue for argv\n";
        return taintedArgs;
    }
    // 调试输出节点信息
    const SVF::PAGNode* node = pag->getGNode(argvNode);
    SVFUtil::outs() << "Processing argv node " << argvNode 
                   << " (Type: " << node->getNodeKind() << ")\n";
    LLVMUtil::dumpValue(argvArg);

    if (const SVF::ValVar* valVar = llvm::dyn_cast<SVF::ValVar>(node)) {
    // 获取对应的LLVM值
        const llvm::Value* llvmVal = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(valVar);
        
        // 调试输出原始值信息
        SVFUtil::outs() << "Tracing arraydecay source for: ";
        // 打印llvmVal
        llvmVal->print(llvm::outs(),true);
        SVFUtil::outs() << "\n";

        
        // 追踪GEP指令的来源
        if (const llvm::GetElementPtrInst* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(llvmVal)) {
            // 获取被操作数
            const llvm::Value* basePointer = gep->getPointerOperand();
            SVFUtil::outs() << "Found GEP base pointer: ";
            basePointer->print(llvm::outs(),true);
            SVFUtil::outs() << "\n";
           
            
            // 查找alloca指令（原始数组分配）
            if (const llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(basePointer)) {
                SVFUtil::outs() << "Found original array allocation: ";
                alloca->print(llvm::outs(),true);
                SVFUtil::outs() << "\n";
                
                // 获取alloca指令对应的SVF节点ID
                NodeID allocaNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(alloca);
                
                // 通过PAG获取内存对象
                if (SVF::PAGNode* pagNode = pag->getGNode(allocaNodeId)) {
                    if (pagNode->hasOutgoingEdges(SVF::PAGEdge::Addr)) {
                        SVFUtil::outs() << "PagNode type: " << pagNode->getNodeKind() << "\n";
                        SVFUtil::outs() << "PagNode toString: " << pagNode->toString() << "\n";

                        if (auto stackObj = llvm::dyn_cast<SVF::StackObjVar>(pagNode)) {
                            SVFUtil::outs() << "Processing stack array: " << stackObj->toString() << "\n";
                            processStackObj(stackObj, callInst, pag, taintedArgs);
                        }
                    }
                }
            }
        }
        else {
            SVFUtil::outs() << "[Warning] Non-GEP pointer in arraydecay handling\n";
        }
    }
    else if (const SVF::GepObjVar* gepObj = llvm::dyn_cast<SVF::GepObjVar>(node)) {
        processGepObj(gepObj, callInst, pag, taintedArgs);
    } 
    else if (const SVF::DummyObjVar* dummyObj = llvm::dyn_cast<SVF::DummyObjVar>(node)) {
        processDummyObj(dummyObj, callInst, pag, taintedArgs);
    }
    else {
        SVFUtil::outs() << "[Warning] Unhandled node type: " << node->getNodeKind() << "\n";
    }
    
    return taintedArgs;
}

// 处理GEP对象（堆/全局数组）
void CustomTaintAnalysis::processGepObj(const SVF::GepObjVar* gepObj, const llvm::CallInst* callInst, 
                                      SVF::PAG* pag, std::vector<SVF::NodeID>& taintedArgs) {
    // 获取argc参数值
    int argc = 0;
    const llvm::Value* argcPtr = callInst->getArgOperand(2);
    // 新的解析逻辑：直接查找对argc的store指令
    if (const llvm::StoreInst* storeInst = llvm::dyn_cast<llvm::StoreInst>(argcPtr)) {
        // 处理直接存储常量值的情况（store i32 2, ptr %argc）
        if (const llvm::ConstantInt* argcConst = 
            llvm::dyn_cast<llvm::ConstantInt>(storeInst->getValueOperand())) {
            argc = argcConst->getSExtValue();
        }
    }
    // 处理通过alloca+store的情况
    else if (const llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(argcPtr)) {
        // 遍历所有对alloca的store指令
        for (const llvm::User* user : alloca->users()) {
            if (const llvm::StoreInst* store = llvm::dyn_cast<llvm::StoreInst>(user)) {
                if (store->getPointerOperand() == alloca) {
                    if (const llvm::ConstantInt* ci = 
                        llvm::dyn_cast<llvm::ConstantInt>(store->getValueOperand())) {
                        argc = ci->getSExtValue();
                        break; // 取第一个store的值
                    }
                }
            }
        }
    }
    
    if (argc == 0) {
        SVFUtil::outs() << "[Warning] Failed to determine argc value\n";
        return;
    }
    
    SVFUtil::outs() << "Processing GEP object with argc=" << argc << "\n";
    
    // 污染数组元素
    for (int i = 0; i < argc; ++i) {
    // 使用 getBaseObj 方法获取基础对象
        if (const SVF::BaseObjVar* baseObj = gepObj->getBaseObj()) {
            NodeID elemNode = pag->getGepObjVar(baseObj->getId(), i);
            if (elemNode != 0) {
                taintedArgs.push_back(elemNode);
                // 使用 baseObj 获取对应的 LLVM 值
                logTaintedElement(i, elemNode, LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(baseObj));
            }
        }
    }
}

// 处理Dummy对象（栈分配数组）
void CustomTaintAnalysis::processDummyObj(const SVF::DummyObjVar* dummyObj, const llvm::CallInst* callInst,
                                        SVF::PAG* pag, std::vector<SVF::NodeID>& taintedArgs) {
    int argc = 0;
    const llvm::Value* argcPtr = callInst->getArgOperand(2);
    // 新的解析逻辑：直接查找对argc的store指令
    if (const llvm::StoreInst* storeInst = llvm::dyn_cast<llvm::StoreInst>(argcPtr)) {
        // 处理直接存储常量值的情况（store i32 2, ptr %argc）
        if (const llvm::ConstantInt* argcConst = 
            llvm::dyn_cast<llvm::ConstantInt>(storeInst->getValueOperand())) {
            argc = argcConst->getSExtValue();
        }
    }
    // 处理通过alloca+store的情况
    else if (const llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(argcPtr)) {
        // 遍历所有对alloca的store指令
        for (const llvm::User* user : alloca->users()) {
            if (const llvm::StoreInst* store = llvm::dyn_cast<llvm::StoreInst>(user)) {
                if (store->getPointerOperand() == alloca) {
                    if (const llvm::ConstantInt* ci = 
                        llvm::dyn_cast<llvm::ConstantInt>(store->getValueOperand())) {
                        argc = ci->getSExtValue();
                        break; // 取第一个store的值
                    }
                }
            }
        }
    }
    
    if (argc == 0) {
        SVFUtil::outs() << "[Warning] Failed to determine argc value\n";
        return;
    }
    
    SVFUtil::outs() << "Processing stack array with argc=" << argc << "\n";
    
    // 获取对应的LLVM值
    const llvm::Value* baseValue = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(dummyObj);

    llvm::Type* elemType = nullptr;
    if (const llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(baseValue)) {
        if (const llvm::ArrayType* arrayType = llvm::dyn_cast<llvm::ArrayType>(alloca->getAllocatedType())) {
            elemType = arrayType->getElementType();
        }
    }
    if (!elemType) {
        SVFUtil::outs() << "[Error] Failed to determine array element type\n";
        return;
    }
    SVFIRBuilder builder;
    
    // 根据实际参数数量污染元素
    for (int i = 0; i < argc; ++i) {
        // 创建正确的GEP指令
        llvm::Value* index[] = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(baseValue->getContext()), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(baseValue->getContext()), i)
        };
        
       if (auto gepInst = llvm::GetElementPtrInst::Create(
                elemType,  // 使用显式指定的元素类型
                const_cast<llvm::Value*>(baseValue), 
                index,
                "",
                (llvm::Instruction*)nullptr)) {
            
            // 转换为SVFValue并获取节点ID
            NodeID elemNode = builder.getValueNode(gepInst);
            
            if (elemNode != 0) {
                taintedArgs.push_back(elemNode);
                logTaintedElement(i, elemNode, baseValue);
            }
            
            delete gepInst; // 清理临时创建的指令
        }
        else {
            SVFUtil::outs() << "[Warning] Failed to create GEP for index " << i << "\n";
        }
    }
}

void CustomTaintAnalysis::processStackObj(const SVF::StackObjVar* stackObj, const llvm::CallInst* callInst,
                                        SVF::PAG* pag, std::vector<SVF::NodeID>& taintedArgs) {
    
    int argc = 0;
    const llvm::Value* argcPtr = callInst->getArgOperand(2);
    // 新的解析逻辑：直接查找对argc的store指令
    if (const llvm::StoreInst* storeInst = llvm::dyn_cast<llvm::StoreInst>(argcPtr)) {
        // 处理直接存储常量值的情况（store i32 2, ptr %argc）
        if (const llvm::ConstantInt* argcConst = 
            llvm::dyn_cast<llvm::ConstantInt>(storeInst->getValueOperand())) {
            argc = argcConst->getSExtValue();
        }
    }
    // 处理通过alloca+store的情况
    else if (const llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(argcPtr)) {
        // 遍历所有对alloca的store指令
        for (const llvm::User* user : alloca->users()) {
            if (const llvm::StoreInst* store = llvm::dyn_cast<llvm::StoreInst>(user)) {
                if (store->getPointerOperand() == alloca) {
                    if (const llvm::ConstantInt* ci = 
                        llvm::dyn_cast<llvm::ConstantInt>(store->getValueOperand())) {
                        argc = ci->getSExtValue();
                        break; // 取第一个store的值
                    }
                }
            }
        }
    }
    
    if (argc == 0) {
        SVFUtil::outs() << "[Warning] Failed to determine argc value\n";
        return;
    }
    
    SVFUtil::outs() << "Processing stack array (StackObjVar) with argc=" << argc << "\n";
    
    // 遍历数组元素（根据argc值）
    for (int i = 0; i < argc; ++i) {
        SVF::NodeID elemNode = pag->getGepObjVar(stackObj->getId(), i);
        
        if (elemNode != 0) {
            taintedArgs.push_back(elemNode);
            SVFUtil::outs() << "Marked argv[" << i << "] (NodeID:" << elemNode << ") as tainted\n";
            
            // 调试输出元素值信息
            if (const SVF::PAGNode* node = pag->getGNode(elemNode)) {
                if (const llvm::Value* val = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(node)) {
                    SVFUtil::outs() << "  Value: ";
                    val->print(llvm::outs());
                    SVFUtil::outs() << "\n";
                }
            }
        }
    }
}

// 调试输出辅助函数
void CustomTaintAnalysis::logTaintedElement(int index, SVF::NodeID nodeId, const llvm::Value* value) {
    // 保持参数类型为LLVM原生类型
    SVFUtil::outs() << "Tainted argv[" << index << "] (NodeID: " << nodeId << ") ";
    if (value) {
        SVFUtil::outs() << "from " << value->getName().str();
    }
    SVFUtil::outs() << "\n";
}