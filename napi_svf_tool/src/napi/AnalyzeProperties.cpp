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
    
    for(Function &F : *llvmModule){
        for(BasicBlock &BB : F){
            for(Instruction &I : BB){
                if (CallBase* callInst = SVFUtil::dyn_cast<CallBase>(&I)){
                    const Function* calledFunc = callInst->getCalledFunction();
                    if (!calledFunc || calledFunc->isIntrinsic())
                        continue;
                    if (calledFunc->getName() == "napi_define_properties"){
                        // 获取源代码位置信息
                        const DebugLoc& loc = callInst->getDebugLoc();

                        // 解析参数（根据函数签名）
                        // 参数顺序：env, exports, property_count, properties
                        Value* envArg = callInst->getArgOperand(0);
                        Value* exportsArg = callInst->getArgOperand(1);
                        Value* propCountArg = callInst->getArgOperand(2);
                        Value* propArrayArg = callInst->getArgOperand(3);

                        // 从propArrayArg中回溯数组位置

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
                        }
                    }
                }
            }
        }
    }
    return globalVars;
}


// 对globalVars这个数组进行解析
std::set<Function*> NapiPropertiesAnalyzer::analyzeGlobalVars(std::set<GlobalVariable*> globalVars, ReadArkts& readArkts) {
    // 对比readArkts中的函数名，和globalVars中的第一个参数对比
    std::set<Function*> functions;
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
                            // 对比readArkts中的函数名，和globalVars中的第一个参数对比
                            
                            if (readArkts.hasFunction(propNameStr)) {
                                // 解析第三个字段：回调函数指针
                                Value* funcField = prop->getOperand(2)->stripPointerCasts();
                                if (Function* callbackFunc = dyn_cast<Function>(funcField)) {
                                    SVFUtil::outs() << "  Callback function: " << callbackFunc->getName().str() << "\n";
                                    functions.insert(callbackFunc);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return functions;
}