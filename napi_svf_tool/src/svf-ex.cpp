

#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/VFG.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFGEdge.h"
#include "SVFIR/SVFIR.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/Options.h"
#include "napi/utils/ReadArkts.h"
#include "napi/AnalyzeProperties.h"
#include "taintanalysis/TaintTracker.h"
#include "taintanalysis/TaintList.h"
#include "JsonExporter/JsonOutput.h"
#include "Util/WorkList.h"

using namespace llvm;
using namespace std;
using namespace SVF;

/*!
 * An example to query alias results of two LLVM values
 */
SVF::AliasResult aliasQuery(PointerAnalysis* pta, const SVFVar* v1, const SVFVar* v2)
{
    return pta->alias(v1->getId(), v2->getId());
}

/*!
 * An example to print points-to set of an LLVM value
 */
std::string printPts(PointerAnalysis* pta, const SVFVar* svfval)
{

    std::string str;
    raw_string_ostream rawstr(str);
    NodeID pNodeId = svfval->getId();

    const PointsTo& pts = pta->getPts(pNodeId);
    for (PointsTo::iterator ii = pts.begin(), ie = pts.end();
            ii != ie; ii++)
    {
        rawstr << " " << *ii << " ";
        PAGNode* targetObj = pta->getPAG()->getGNode(*ii);
        rawstr << "(" << targetObj->toString() << ")\t ";
    }

    return rawstr.str();

}


/*!
 * An example to query/collect all successor nodes from a ICFGNode (iNode) along control-flow graph (ICFG)
 */
void traverseOnICFG(ICFG* icfg, const Instruction* inst)
{
    const ICFGNode* iNode = LLVMModuleSet::getLLVMModuleSet()->getICFGNode(inst);

    FIFOWorkList<const ICFGNode*> worklist;
    Set<const ICFGNode*> visited;
    worklist.push(iNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const ICFGNode* iNode = worklist.pop();
        for (ICFGNode::const_iterator it = iNode->OutEdgeBegin(), eit =
                    iNode->OutEdgeEnd(); it != eit; ++it)
        {
            ICFGEdge* edge = *it;
            ICFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }
}

/*!
 * An example to query/collect all the uses of a definition of a value along value-flow graph (VFG)
 */
void traverseOnVFG(const SVFG* vfg, const SVFVar* svfval, PAG* pag)
{
    if (!vfg->hasDefSVFGNode(svfval)){
        return;
    }
    const VFGNode* vNode = vfg->getDefSVFGNode(svfval);
    FIFOWorkList<const VFGNode*> worklist;
    Set<const VFGNode*> visited;
    worklist.push(vNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const VFGNode* vNode = worklist.pop();
        std::cout << "VFGNode ID: " << vNode->getId() << std::endl;
        std::cout << vNode->toString() << std::endl;
        for (VFGNode::const_iterator it = vNode->OutEdgeBegin(), eit =
                    vNode->OutEdgeEnd(); it != eit; ++it)
        {
            VFGEdge* edge = *it;
            VFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }

    /// Collect all LLVM Values
    for(Set<const VFGNode*>::const_iterator it = visited.begin(), eit = visited.end(); it!=eit; ++it)
    {
        const VFGNode* node = *it;
        // dummyVisit(node);
        /// can only query VFGNode involving top-level pointers (starting with % or @ in LLVM IR)
        /// PAGNode* pNode = vfg->getLHSTopLevPtr(node);
        /// Value* val = pNode->getValue();
        // std::cout << "VFGNode ID: " << node->getId() << std::endl;
        // std::cout << node->toString() << std::endl;
    }
}

int main(int argc, char ** argv)
{

    // 调用ReadArkts读取index.d.ts，获取函数信息
    std::string index_dts_path = "/home/jackie/project/SVF-example/napi_project/HarmonyNativeFlowBench/native_buffer/entry/src/main/cpp/types/libentry/Index.d.ts";
    ReadArkts readArkts;
    readArkts.parseFile(index_dts_path);
    const std::vector<FunctionInfo>& targetfunctions = readArkts.getFunctions();
    for (const FunctionInfo& func : targetfunctions) {
        SVFUtil::outs() << "Function: " << func.name << "\n";
    }

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
            argc, argv, "Whole Program Points-to Analysis", "[options] <input-bitcode...>"
    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    LLVMModuleSet::buildSVFModule(moduleNameVec);

    /// Build Program Assignment Graph (SVFIR)
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();

    /// Create Andersen's pointer analysis
    Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    // 启动flow-sensitive
    // FlowSensitive* flowSensitivePTA = new FlowSensitive(pag);
    // flowSensitivePTA->analyze();

    /// Query aliases
    /// aliasQuery(ander,value1,value2);

    /// Print points-to information
    // printPts(ander, value1);
    /// Call Graph
    CallGraph* callgraph = ander->getCallGraph();

    /// ICFG
    ICFG* icfg = pag->getICFG();

    /// Value-Flow Graph (VFG)
    VFG* vfg = new VFG(callgraph);

    /// Sparse value-flow graph (SVFG)
    SVFGBuilder svfBuilder;
    // svfBuilder.setContextSensitive(true);
    
    SVFG* svfg = svfBuilder.buildFullSVFG(ander);
    std::set<llvm::GlobalVariable*> globalVars = NapiPropertiesAnalyzer::analyzeNapiProperties(svfg,pag);
    for (llvm::GlobalVariable* global : globalVars) {
        SVFUtil::outs() << "Global variable: " << global->getName().str() << "\n";
    }

    std::set<llvm::Function*> llvmfunctions = NapiPropertiesAnalyzer::analyzeGlobalVars(globalVars,readArkts);
    for (llvm::Function* func : llvmfunctions) {
        SVFUtil::outs() << "Function: " << func->getName().str() << "\n";
    }
    LLVMModuleSet* llvmModuleSet = LLVMModuleSet::getLLVMModuleSet();

    TaintTracker taintTracker(pag, ander, svfg, vfg);
    for (llvm::Function* func : llvmfunctions) {
        taintTracker.initializeFunctionArgs(func);
        // 获取func的名称和参数NodeID
        std::string funcName = func->getName().str();
        std::vector<NodeID> paramNodeIDs;
        // 打印
        SVFUtil::outs() << "Function: " << funcName << "\n";
        for(auto& arg : func->args()) {
            SVFUtil::outs() << "Argument: " << arg.getName().str() << "\n";
            Value* argVal = &arg;
            NodeID argNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argVal);
            SVFVar* svfVar = pag->getGNode(argNodeID);
            traverseOnVFG(svfg, svfVar, pag);
            paramNodeIDs.push_back(argNodeID);
        }
        std::vector<TaintUnit> taintUnits = taintTracker.propagateTaint(func);
        taintTracker.Traceker(func, paramNodeIDs);
        TaintList taintList(ander, taintUnits, funcName, paramNodeIDs);
        taintList.processTargetUnits();
        std::vector<TargetUnit> targetUnits = taintList.getFinalTargetUnits();
        JsonOutput::writeToFile(targetUnits, funcName, taintList.getFunctionParamNodeID_MapID(), "result/taint_analysis_result.json");
    }

    /// Collect uses of an LLVM Value
    /// traverseOnVFG(svfg, value);

    /// Collect all successor nodes on ICFG
    /// traverseOnICFG(icfg, value);

    // clean up memory
    delete vfg;
    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();

    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    llvm::llvm_shutdown();
    return 0;
}

