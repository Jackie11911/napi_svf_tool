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
#include <nlohmann/json.hpp>
#include "projectParser/ProjectParser.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib> // for getenv
using namespace llvm;
using namespace std;
using namespace SVF;

#include <filesystem>

namespace fs = std::filesystem;

static bool DEBUG_MODE = false;

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


/// 对单个库进行SVF分析并生成JSON输出
void analyzeSingleLibrary(const LibraryInfo& lib) {

    fs::path resultDir = fs::current_path() / "result";
    if (!fs::exists(resultDir)) {
        fs::create_directory(resultDir);
    }

    // 创建项目子目录
    fs::path projectDir = resultDir / lib.projectName;
    if (!fs::exists(projectDir)) {
        fs::create_directory(projectDir);
    }

    // 构建输出文件路径
    std::string outputfilename = (projectDir / (lib.soName + ".ir.json")).string();

    // 设置模块向量，使用所有的LLVM IR文件
    std::vector<std::string> moduleNameVec;
    moduleNameVec.push_back(lib.finalLLVMIR);
    // if (const char* svfDir = std::getenv("SVF_DIR")) {
    //     moduleNameVec.push_back(std::string(svfDir) + "/lib/extapi.bc");
    // } else {
    //     SVFUtil::errs() << "环境变量 SVF_DIR 未设置，无法添加 extapi.bc\n";
    // }

    if (Options::WriteAnder() == "ir_annotator") {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    LLVMModuleSet::buildSVFModule(moduleNameVec);

    /// Build Program Assignment Graph (SVFIR)
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();

    /// Create Andersen's pointer analysis
    Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);

    /// Call Graph
    CallGraph* callgraph = ander->getCallGraph();

    /// ICFG
    ICFG* icfg = pag->getICFG();

    /// Value-Flow Graph (VFG)
    VFG* vfg = new VFG(callgraph);

    /// Sparse value-flow graph (SVFG)
    SVFGBuilder svfBuilder;
    SVFG* svfg = svfBuilder.buildFullSVFG(ander);

    std::set<llvm::GlobalVariable*> globalVars = NapiPropertiesAnalyzer::analyzeNapiProperties(svfg, pag);
    std::map<std::string, llvm::Function*> llvmfunctions = NapiPropertiesAnalyzer::analyzeGlobalVars(globalVars);

    // 添加对通过napi_set_named_property注册的函数的分析
    std::map<std::string, llvm::Function*> namedFunctions = NapiPropertiesAnalyzer::analyzeNamedProperties(svfg, pag, ander);

    // 合并两种注册方式的分析结果
    llvmfunctions.insert(namedFunctions.begin(), namedFunctions.end());

    TaintTracker taintTracker(pag, ander, svfg, vfg);
    nlohmann::json allResults = nlohmann::json::array();

    for (auto& func : llvmfunctions) {
        taintTracker.initializeFunctionArgs(func.second);
        std::string funcName = func.first;
        std::vector<std::pair<NodeID, std::string>> paramNodeIDs;

        for(auto& arg : func.second->args()) {
            Value* argVal = &arg;
            NodeID argNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argVal);
            SVFVar* svfVar = pag->getGNode(argNodeID);
            std::string argname = svfVar->getValueName();
            paramNodeIDs.push_back(std::make_pair(argNodeID, argname));
        }

        nlohmann::json outputJson = taintTracker.Traceker(func.second, paramNodeIDs, funcName);
        allResults.push_back(outputJson);
    }

    // 创建最终的 JSON 对象
    nlohmann::json finalJson;
    finalJson["hap_name"] = lib.name;
    finalJson["so_name"] = lib.soName;
    finalJson["module_name"] = lib.name;
    finalJson["functions"] = allResults;

    JsonOutput::writeToFile(outputfilename, finalJson.dump(4, ' ', false));

    // 清理内存
    delete vfg;
    vfg = nullptr;
    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();
    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
}

int main(int argc, char ** argv)
{
    // 检查命令行参数
    if (argc < 2) {
        SVFUtil::errs() << "用法: " << argv[0] << " <项目路径>\n";
        SVFUtil::errs() << "示例: " << argv[0] << " napi_project/HarmonyXFlowBench/native_array_get\n";
        return 1;
    }

    // 调用ProjectParser解析项目
    ProjectParser projectParser(argv[1]);
    std::vector<LibraryInfo> libraries = projectParser.getLibraries();
    

    if (DEBUG_MODE) {
        // 调试模式：串行处理每个库
        SVFUtil::outs() << "使用调试模式：串行处理库\n";
        for (const LibraryInfo& lib : libraries) {
            SVFUtil::outs() << "开始分析库: " << lib.name << "\n";
            analyzeSingleLibrary(lib);
        }
    } else {
        // 生产模式：使用多进程
        SVFUtil::outs() << "使用生产模式：多进程处理库\n";
        std::vector<pid_t> child_pids;
        
        // 对每个库启动独立进程进行SVF分析
        for (const LibraryInfo& lib : libraries) {
            pid_t pid = fork();
            
            if (pid < 0) {
                // fork失败
                SVFUtil::errs() << "无法为库 " << lib.name << " 创建子进程\n";
                continue;
            } 
            else if (pid == 0) {
                // 子进程
                SVFUtil::outs() << "开始分析库: " << lib.name << " (PID: " << getpid() << ")\n";
                analyzeSingleLibrary(lib);
                exit(0); // 子进程完成后退出
            } 
            else {
                // 父进程，记录子进程PID
                child_pids.push_back(pid);
            }
        }

        // 父进程等待所有子进程完成
        for (pid_t child_pid : child_pids) {
            int status;
            waitpid(child_pid, &status, 0);
            if (WIFEXITED(status)) {
                SVFUtil::outs() << "子进程 " << child_pid << " 已完成，退出状态: " << WEXITSTATUS(status) << "\n";
            } else if (WIFSIGNALED(status)) {
                SVFUtil::errs() << "子进程 " << child_pid << " 被信号 " << WTERMSIG(status) << " 终止\n";
            }
        }
    }

    llvm::llvm_shutdown();
    return 0;
}

