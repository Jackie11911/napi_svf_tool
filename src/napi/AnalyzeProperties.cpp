#include "napi/AnalyzeProperties.h"

using namespace llvm;
using namespace SVF;

std::set<GlobalVariable*> NapiPropertiesAnalyzer::analyzeNapiProperties(SVFG* svfg,SVFIR* pag) {
    std::set<GlobalVariable*> globalVars;
   
    // 创建指针分析
    Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);

    // 获取调用图
    CallGraph* callGraph = ander->getCallGraph();

    Module* llvmModule = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();
    
    // 存储napi_module_register调用与对应的模块结构体指针
    std::map<GlobalVariable*, const Function*> moduleToInitFunc;

    // 第一步：找到所有napi_module_register调用及其参数
    for(Function &F : *llvmModule){
        for(BasicBlock &BB : F){
            for(Instruction &I : BB){
                if (const CallBase* callInst = SVFUtil::dyn_cast<CallBase>(&I)){
                    const Function* calledFunc = callInst->getCalledFunction();
                    if (!calledFunc || calledFunc->isIntrinsic())
                        continue;
                    
                    if (calledFunc->getName() == "napi_module_register"){
                        SVFUtil::outs() << "Found napi_module_register call in: " << F.getName().str() << "\n";
                        
                        // 获取传递给napi_module_register的参数（napi_module结构体指针）
                        if (callInst->arg_size() > 0) {
                            Value* moduleArg = callInst->getArgOperand(0);
                            
                            // 获取全局变量
                            if (GlobalVariable* moduleGlobal = SVFUtil::dyn_cast<GlobalVariable>(moduleArg)) {
                                SVFUtil::outs() << "  Module global: " << moduleGlobal->getName().str() << "\n";
                                
                                // 解析napi_module结构体
                                if (ConstantStruct* moduleStruct = SVFUtil::dyn_cast<ConstantStruct>(moduleGlobal->getInitializer())) {
                                    // napi_module的第4个字段是注册函数（nm_register_func）
                                    if (moduleStruct->getNumOperands() >= 4) {
                                        Value* regFuncVal = moduleStruct->getOperand(3)->stripPointerCasts();
                                        if (const Function* regFunc = SVFUtil::dyn_cast<Function>(regFuncVal)) {
                                            SVFUtil::outs() << "  Found register function: " << regFunc->getName().str() << "\n";
                                            moduleToInitFunc[moduleGlobal] = regFunc;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 第二步：在找到的初始化函数中查找napi_define_properties调用
    for (auto &pair : moduleToInitFunc) {
        const Function* initFunc = pair.second;
        
        // 分析初始化函数中的napi_define_properties调用
        for (auto &BB : *initFunc) {
            for (auto &I : BB) {
                if (const CallBase* callInst = SVFUtil::dyn_cast<CallBase>(&I)) {
                    const Function* calledFunc = callInst->getCalledFunction();
                    if (!calledFunc || calledFunc->isIntrinsic())
                        continue;
                    
                    if (calledFunc->getName() == "napi_define_properties"){
                        SVFUtil::outs() << "Found napi_define_properties in init function: " << initFunc->getName().str() << "\n";
                        
                        // 获取源代码位置信息
                        const DebugLoc& loc = callInst->getDebugLoc();

                        // 解析参数（根据函数签名）
                        // 参数顺序：env, exports, property_count, properties
                        Value* envArg = callInst->getArgOperand(0);
                        Value* exportsArg = callInst->getArgOperand(1);
                        Value* propCountArg = callInst->getArgOperand(2);
                        Value* propArrayArg = callInst->getArgOperand(3);

                        // 分析属性数组
                        if (ConstantInt* count = SVFUtil::dyn_cast<ConstantInt>(propCountArg)) {
                            uint64_t arraySize = count->getZExtValue();
                            SVFUtil::outs() << "  Property count: " << arraySize << "\n";

                            if (GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(propArrayArg)) {
                                if (AllocaInst* alloca = SVFUtil::dyn_cast<AllocaInst>(gep->getPointerOperand())) {
                                    // 搜索该alloca的所有使用者，找到memcpy指令
                                    for (User* user : alloca->users()) {
                                        if (MemCpyInst* memcpy = SVFUtil::dyn_cast<MemCpyInst>(user)) {
                                            if (Constant* src = SVFUtil::dyn_cast<Constant>(memcpy->getSource())) {
                                                // 剥离可能的bitcast操作
                                                if (GlobalVariable* global = SVFUtil::dyn_cast<GlobalVariable>(src->stripPointerCasts())) {
                                                    SVFUtil::outs() << "Found target global: " << global->getName().str() << "\n";
                                                    globalVars.insert(global);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else{
                                if (AllocaInst* alloca = SVFUtil::dyn_cast<AllocaInst>(propArrayArg)) {
                                    // 搜索该alloca的所有使用者，找到memcpy指令
                                    for (User* user : alloca->users()) {
                                        if (MemCpyInst* memcpy = SVFUtil::dyn_cast<MemCpyInst>(user)) {
                                            if (Constant* src = SVFUtil::dyn_cast<Constant>(memcpy->getSource())) {
                                                // 剥离可能的bitcast操作
                                                if (GlobalVariable* global = SVFUtil::dyn_cast<GlobalVariable>(src->stripPointerCasts())) {
                                                    SVFUtil::outs() << "Found target global: " << global->getName().str() << "\n";
                                                    globalVars.insert(global);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return globalVars;
}


// 对globalVars这个数组进行解析
std::map<std::string, Function*> NapiPropertiesAnalyzer::analyzeGlobalVars(std::set<GlobalVariable*> globalVars) {
    std::map<std::string, Function*> functions;
    for (GlobalVariable* global : globalVars) {
        // 获取全局变量的初始化表达式
        if (ConstantArray* init = dyn_cast<ConstantArray>(global->getInitializer())) {
            SVFUtil::outs() << "\nAnalyzing global array: " << global->getName().str() << "\n";
            
            // 遍历数组中的每个属性描述符
            for (unsigned i = 0; i < init->getNumOperands(); ++i) {
                if (ConstantStruct* prop = dyn_cast<ConstantStruct>(init->getOperand(i))) {
                    // 解析第一个字段：属性名（字符串指针）
                    Value* nameField = prop->getOperand(0)->stripPointerCasts();
                    if (GlobalVariable* strGlobal = dyn_cast<GlobalVariable>(nameField)) {
                        if (ConstantDataArray* strArray = dyn_cast<ConstantDataArray>(strGlobal->getInitializer())) {
                            StringRef propName = strArray->getAsCString();
                            std::string propNameStr = propName.str();
                            SVFUtil::outs() << "Property[" << i << "] name: " << propNameStr << "\n";
                            Value* funcField = prop->getOperand(2)->stripPointerCasts();
                            if (Function* callbackFunc = dyn_cast<Function>(funcField)) {
                                SVFUtil::outs() << "  Callback function: " << callbackFunc->getName().str() << "\n";
                                functions[propNameStr] = callbackFunc;
                            }
                        }
                    }
                }
            }
        }
    }
    return functions;
}

// 查找模块注册函数
std::map<GlobalVariable*, const Function*> NapiPropertiesAnalyzer::findModuleInitFunctions(Module* llvmModule) {
    std::map<GlobalVariable*, const Function*> moduleToInitFunc;
    
    // 查找所有napi_module_register调用及其参数
    for(Function &F : *llvmModule){
        for(BasicBlock &BB : F){
            for(Instruction &I : BB){
                if (const CallBase* callInst = SVFUtil::dyn_cast<CallBase>(&I)){
                    const Function* calledFunc = callInst->getCalledFunction();
                    if (!calledFunc || calledFunc->isIntrinsic())
                        continue;
                    
                    if (calledFunc->getName() == "napi_module_register"){
                        // 获取传递给napi_module_register的参数（napi_module结构体指针）
                        if (callInst->arg_size() > 0) {
                            Value* moduleArg = callInst->getArgOperand(0);
                            
                            // 获取全局变量
                            if (GlobalVariable* moduleGlobal = SVFUtil::dyn_cast<GlobalVariable>(moduleArg)) {
                                // 解析napi_module结构体
                                if (ConstantStruct* moduleStruct = SVFUtil::dyn_cast<ConstantStruct>(moduleGlobal->getInitializer())) {
                                    // napi_module的第4个字段是注册函数（nm_register_func）
                                    if (moduleStruct->getNumOperands() >= 4) {
                                        Value* regFuncVal = moduleStruct->getOperand(3)->stripPointerCasts();
                                        if (const Function* regFunc = SVFUtil::dyn_cast<Function>(regFuncVal)) {
                                            moduleToInitFunc[moduleGlobal] = regFunc;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return moduleToInitFunc;
}



// 查找属性值与napi_create_function结果指针之间的关系
Function* NapiPropertiesAnalyzer::findCallbackForValue(
    Value* valueVal, 
    NodeID valueNodeId, 
    const Function* initFunc, 
    SVFIR* pag, 
    PointerAnalysis* pta,
    SVFG* svfg) {
    
    for (auto &BB : *initFunc) {
        for (auto &I : BB) {
            // 使用LLVMUtil检查是否是函数调用
            if (LLVMUtil::isCallSite(&I)) {
                const CallBase* createFuncCall = LLVMUtil::getLLVMCallSite(&I);
                const Function* calledFunction = LLVMUtil::getCallee(createFuncCall);
                
                if (!calledFunction)
                    continue;
                
                // 检查是否是napi_create_function调用
                if (calledFunction->getName().str() == "napi_create_function") {
                    SVFUtil::outs() << "Found napi_create_function call\n";
                    
                    // 参数顺序：env, name, length, cb, data, result
                    if (createFuncCall->arg_size() >= 6) {
                        // 获取回调函数（第4个参数）
                        Value* cbFunc = createFuncCall->getArgOperand(3);
                        // 获取结果函数指针（第6个参数）
                        Value* resultPtr = createFuncCall->getArgOperand(5);
                        
                        if (Function* callback = SVFUtil::dyn_cast<Function>(cbFunc->stripPointerCasts())) {
                            // 使用指针分析检查valueVal是否指向resultPtr
                            NodeID resultNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(resultPtr);
                            
                            // 使用checkPotentialAlias检查valueVal和resultPtr是否可能是别名
                            if (checkPotentialAlias(valueNodeId, resultNodeId, svfg)) {
                                SVFUtil::outs() << "  Found callback through potential alias check between valueVal and resultPtr\n";
                                return callback;
                            }
                            
                            // 如果是加载指令，检查加载源
                            if (LoadInst* loadInst = SVFUtil::dyn_cast<LoadInst>(valueVal)) {
                                Value* loadSrc = loadInst->getPointerOperand();
                                NodeID loadSrcId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(loadSrc);
                                
                                // 使用checkPotentialAlias检查loadSrc和resultPtr是否可能是别名
                                if (checkPotentialAlias(loadSrcId, resultNodeId, svfg)) {
                                    SVFUtil::outs() << "  Found callback through potential alias check between loadSrc and resultPtr\n";
                                    return callback;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return nullptr;
}

// 分析napi_set_named_property调用与初始化函数返回值的关系
void NapiPropertiesAnalyzer::analyzeSetNamedPropertyCalls(
    Module* llvmModule, 
    const Function* initFunc, 
    NodeID retNodeId,
    SVFIR* pag, 
    PointerAnalysis* pta,
    SVFG* svfg,
    std::map<std::string, Function*>& functions) {
    
    // 查找所有napi_set_named_property调用
    for (Function &F : *llvmModule) {
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (const CallBase* callInst = SVFUtil::dyn_cast<CallBase>(&I)) {
                    const Function* calledFunc = callInst->getCalledFunction();
                    if (!calledFunc || calledFunc->isIntrinsic())
                        continue;
                    
                    if (calledFunc->getName() == "napi_set_named_property") {
                        SVFUtil::outs() << "Found napi_set_named_property call in: " << F.getName().str() << "\n";
                        
                        // 参数顺序：env, object, name, value
                        if (callInst->arg_size() >= 4) {
                            // 获取对象参数（第2个参数）
                            Value* objVal = callInst->getArgOperand(1);
                            NodeID objNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(objVal);
                            
                            // 获取属性名（第3个参数）
                            Value* nameVal = callInst->getArgOperand(2);
                            
                            // 获取属性值（第4个参数）
                            Value* valueVal = callInst->getArgOperand(3);
                            NodeID valueNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(valueVal);
                            
                            // 解析属性名
                            std::string propNameStr;
                            if (GlobalVariable* nameGlobal = SVFUtil::dyn_cast<GlobalVariable>(nameVal->stripPointerCasts())) {
                                if (ConstantDataArray* nameArray = SVFUtil::dyn_cast<ConstantDataArray>(nameGlobal->getInitializer())) {
                                    StringRef propName = nameArray->getAsCString();
                                    propNameStr = propName.str();
                                    SVFUtil::outs() << "  Property name: " << propNameStr << "\n";
                                }
                            }
                            
                            // 直接使用NodeID检查对象是否可能是初始化函数的返回值
                            bool isReturnValue = checkPotentialAlias(objNodeId, retNodeId, svfg);
                            if (isReturnValue) {
                                SVFUtil::outs() << "  Object is potentially an alias of init function return value\n";
                            }

                            if (isReturnValue && !propNameStr.empty()) {
                                SVFUtil::outs() << "  Processing property: " << propNameStr << " for export object\n";
                                
                                // 查找属性值与napi_create_function结果指针之间的关系
                                Function* callback = findCallbackForValue(valueVal, valueNodeId, initFunc, pag, pta, svfg);
                                
                                if (callback) {
                                    SVFUtil::outs() << "  Found callback for property: " << propNameStr << "\n";
                                    functions[propNameStr] = callback;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// 处理通过napi_set_named_property注册的函数
std::map<std::string, Function*> NapiPropertiesAnalyzer::analyzeNamedProperties(SVFG* svfg, SVFIR* pag, PointerAnalysis* pta) {
    std::map<std::string, Function*> functions;
    
    // 获取LLVM模块
    Module* llvmModule = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();
    
    // 第一步：找到所有napi_module_register调用及其参数
    std::map<GlobalVariable*, const Function*> moduleToInitFunc = findModuleInitFunctions(llvmModule);
    
    // 第二步：分析初始化函数的返回值及其与napi_set_named_property的关系
    for (auto &pair : moduleToInitFunc) {
        const Function* initFunc = pair.second;
        SVFUtil::outs() << "Analyzing init function: " << initFunc->getName().str() << "\n";
 
        // 获取函数返回值的NodeID
        NodeID retNodeId = 0;
        for (auto &BB : *initFunc) {
            for (auto &I : BB) {
                if (const ReturnInst* retInst = SVFUtil::dyn_cast<ReturnInst>(&I)) {
                    if (Value* retVal = retInst->getReturnValue()) {
                        retNodeId = LLVMModuleSet::getLLVMModuleSet()->getValueNode(retVal);
                        SVFUtil::outs() << "Found return value in init function, NodeID: " << retNodeId << "\n";
                        break;
                    }
                }
            }
            if (retNodeId != 0) break;
        }
        
        if (retNodeId == 0) {
            SVFUtil::outs() << "Warning: Could not find return value for function " << initFunc->getName().str() << "\n";
        }

        // 分析napi_set_named_property调用与初始化函数返回值的关系
        analyzeSetNamedPropertyCalls(llvmModule, initFunc, retNodeId, pag, pta, svfg, functions);
    }
    
    return functions;
}

// 从NodeID获取定义的SVFGNode
const SVFGNode* NapiPropertiesAnalyzer::getDefSVFGNodeFromNodeID(NodeID nodeID, SVFG* svfg) {
    // 获取SVFIR实例
    SVFIR* pag = SVFIR::getPAG();
    
    // 从SVFIR中获取对应的PAGNode
    const PAGNode* pagNode = pag->getGNode(nodeID);
    
    if (pagNode) {
        // 检查PAGNode是否有定义的SVFGNode
        if (svfg->hasDefSVFGNode(pagNode)) {
            // 获取定义的SVFGNode
            return svfg->getDefSVFGNode(pagNode);
        }
    }
    
    return nullptr;
}

// 检查两个值是否可能是别名（通过分析它们的定义位置和加载来源）
bool NapiPropertiesAnalyzer::checkPotentialAlias(NodeID nodeID1, NodeID nodeID2, SVFG* svfg) {
    SVFUtil::outs() << "Checking potential alias between NodeID " << nodeID1 << " and NodeID " << nodeID2 << "\n";
    
    // 如果NodeID相同，则一定是别名
    if (nodeID1 == nodeID2 && nodeID1 != 0) {
        SVFUtil::outs() << "  Same NodeID - must be aliases\n";
        return true;
    }
    
    // 获取SVFIR实例
    SVFIR* pag = SVFIR::getPAG();
    
    // 使用NodeID获取定义节点
    const SVFGNode* defNode1 = getDefSVFGNodeFromNodeID(nodeID1, svfg);
    const SVFGNode* defNode2 = getDefSVFGNodeFromNodeID(nodeID2, svfg);
    
    // 提取源节点ID
    NodeID src1 = 0;
    NodeID src2 = 0;
    
    // 从defNode1提取源节点ID
    if (defNode1) {
        if (const LoadVFGNode* loadNode = SVFUtil::dyn_cast<LoadVFGNode>(defNode1)) {
            src1 = loadNode->getPAGSrcNodeID();
            SVFUtil::outs() << "  Node1 is a load node, src ID: " << src1 << "\n";
        }
        else if (const StoreVFGNode* storeNode = SVFUtil::dyn_cast<StoreVFGNode>(defNode1)) {
            src1 = storeNode->getPAGSrcNodeID();
            SVFUtil::outs() << "  Node1 is a store node, src ID: " << src1 << "\n";
        }
        else if (const CopyVFGNode* copyNode = SVFUtil::dyn_cast<CopyVFGNode>(defNode1)) {
            src1 = copyNode->getPAGSrcNodeID();
            SVFUtil::outs() << "  Node1 is a copy node, src ID: " << src1 << "\n";
        }
    }
    
    // 从defNode2提取源节点ID
    if (defNode2) {
        if (const LoadVFGNode* loadNode = SVFUtil::dyn_cast<LoadVFGNode>(defNode2)) {
            src2 = loadNode->getPAGSrcNodeID();
            SVFUtil::outs() << "  Node2 is a load node, src ID: " << src2 << "\n";
        }
        else if (const StoreVFGNode* storeNode = SVFUtil::dyn_cast<StoreVFGNode>(defNode2)) {
            src2 = storeNode->getPAGSrcNodeID();
            SVFUtil::outs() << "  Node2 is a store node, src ID: " << src2 << "\n";
        }
        else if (const CopyVFGNode* copyNode = SVFUtil::dyn_cast<CopyVFGNode>(defNode2)) {
            src2 = copyNode->getPAGSrcNodeID();
            SVFUtil::outs() << "  Node2 is a copy node, src ID: " << src2 << "\n";
        }
    }
    
    // 检查nodeID和src之间的任意组合
    if (src1 != 0) {
        // 如果src1与nodeID2相同
        if (src1 == nodeID2) {
            SVFUtil::outs() << "  src1 == nodeID2 - potential alias detected\n";
            return true;
        }
        
        // 如果src1与src2相同
        if (src1 == src2 && src2 != 0) {
            SVFUtil::outs() << "  src1 == src2 - potential alias detected\n";
            return true;
        }
    }
    
    if (src2 != 0) {
        // 如果src2与nodeID1相同
        if (src2 == nodeID1) {
            SVFUtil::outs() << "  src2 == nodeID1 - potential alias detected\n";
            return true;
        }
    }
    
    return false;
}