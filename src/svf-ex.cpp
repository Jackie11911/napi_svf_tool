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
#include <signal.h>
#include <cstdlib> // for getenv
#include <chrono>
#include <iomanip>
#include <fstream>
using namespace llvm;
using namespace std;
using namespace SVF;

#include <filesystem>

namespace fs = std::filesystem;

static bool DEBUG_MODE = false;

// 计时工具类
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::string timer_name;
    
public:
    Timer(const std::string& name) : timer_name(name) {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        return duration.count() / 1000.0; // 返回秒数
    }
    
    void printElapsed() const {
        double elapsed_time = elapsed();
        SVFUtil::outs() << timer_name << " 耗时: " << std::fixed << std::setprecision(3) 
                       << elapsed_time << " 秒\n";
    }
};

// 超时管理辅助函数
bool waitProcessWithTimeout(pid_t pid, int timeoutSeconds, const std::string& processName) {
    auto startTime = std::chrono::high_resolution_clock::now();
    int status;
    
    while (true) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        
        if (result > 0) {
            // 进程正常结束
            if (WIFEXITED(status)) {
                SVFUtil::outs() << processName << " 进程 " << pid << " 正常完成\n";
            } else if (WIFSIGNALED(status)) {
                SVFUtil::outs() << processName << " 进程 " << pid << " 被信号终止\n";
            }
            return true;
        } else if (result == 0) {
            // 进程仍在运行，检查超时
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            
            if (elapsed.count() >= timeoutSeconds) {
                // 超时，强制终止
                SVFUtil::errs() << processName << " 进程 " << pid << " 超时，强制终止\n";
                kill(pid, SIGTERM);
                sleep(2);
                if (waitpid(pid, &status, WNOHANG) == 0) {
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                }
                return false;
            }
            sleep(1); // 等待1秒后再检查
        } else {
            SVFUtil::errs() << "等待 " << processName << " 进程 " << pid << " 时出错\n";
            return false;
        }
    }
}

// 时间统计结构
struct TimeStats {
    std::string libraryName;
    double projectParsingTime = 0.0;
    double svfConstructionTime = 0.0;
    double propertyAnalysisTime = 0.0;
    double taintAnalysisTime = 0.0;
    double totalLibraryTime = 0.0;
    
    void writeToFile(const std::string& outputPath) const {
        nlohmann::json timeJson;
        timeJson["library_name"] = libraryName;
        timeJson["timing_stats"] = {
            {"svf_construction_time_seconds", svfConstructionTime},
            {"property_analysis_time_seconds", propertyAnalysisTime},
            {"taint_analysis_time_seconds", taintAnalysisTime},
            {"total_library_time_seconds", totalLibraryTime}
        };
        
        std::string timeFile = outputPath + ".timing.json";
        std::ofstream file(timeFile);
        if (file.is_open()) {
            file << timeJson.dump(4, ' ', false);
            file.close();
            SVFUtil::outs() << "时间统计写入文件: " << timeFile << "\n";
        }
    }
};

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
TimeStats analyzeSingleLibrary(const LibraryInfo& lib) {
    Timer totalTimer("库 " + lib.name + " 总分析");
    TimeStats stats;
    stats.libraryName = lib.name;

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

    // SVF构造计时开始
    Timer svfTimer("SVF构造");
    SVFUtil::outs() << "开始SVF构造 for " << lib.name << "\n";

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

    stats.svfConstructionTime = svfTimer.elapsed();
    svfTimer.printElapsed();

    // 属性解析计时开始
    Timer propertyTimer("属性解析");
    SVFUtil::outs() << "开始属性解析 for " << lib.name << "\n";

    std::set<llvm::GlobalVariable*> globalVars = NapiPropertiesAnalyzer::analyzeNapiProperties(svfg, pag);
    std::map<std::string, llvm::Function*> llvmfunctions = NapiPropertiesAnalyzer::analyzeGlobalVars(globalVars);

    // 添加对通过napi_set_named_property注册的函数的分析
    std::map<std::string, llvm::Function*> namedFunctions = NapiPropertiesAnalyzer::analyzeNamedProperties(svfg, pag, ander);

    // 合并两种注册方式的分析结果
    llvmfunctions.insert(namedFunctions.begin(), namedFunctions.end());

    stats.propertyAnalysisTime = propertyTimer.elapsed();
    propertyTimer.printElapsed();

    // 污点分析计时开始
    Timer taintTimer("污点分析");
    SVFUtil::outs() << "开始污点分析 for " << lib.name << "\n";

    TaintTracker taintTracker(pag, ander, svfg, vfg);
    nlohmann::json allResults = nlohmann::json::array();

    // 为每个函数创建子进程进行分析
    std::vector<pid_t> function_pids;
    std::vector<std::string> tempFileNames;
    int funcIndex = 0;
    const int TIMEOUT_MINUTES = 10;
    const int TIMEOUT_SECONDS = TIMEOUT_MINUTES * 60;

    for (auto& func : llvmfunctions) {
        std::string funcName = func.first;
        std::string tempFileName = "/tmp/taint_result_" + lib.name + "_" + std::to_string(funcIndex) + "_" + std::to_string(getpid()) + ".json";
        tempFileNames.push_back(tempFileName);
        
        pid_t pid = fork();
        
        if (pid < 0) {
            SVFUtil::errs() << "无法为函数 " << funcName << " 创建子进程\n";
            continue;
        }
        else if (pid == 0) {
            // 子进程：分析单个函数
            taintTracker.initializeFunctionArgs(func.second);
            std::vector<std::pair<NodeID, std::string>> paramNodeIDs;

            for(auto& arg : func.second->args()) {
                Value* argVal = &arg;
                NodeID argNodeID = LLVMModuleSet::getLLVMModuleSet()->getValueNode(argVal);
                SVFVar* svfVar = pag->getGNode(argNodeID);
                std::string argname = svfVar->getValueName();
                paramNodeIDs.push_back(std::make_pair(argNodeID, argname));
            }

            nlohmann::json outputJson = taintTracker.Traceker(func.second, paramNodeIDs, funcName);
            
            // 将结果写入临时文件
            std::ofstream tempFile(tempFileName);
            if (tempFile.is_open()) {
                tempFile << outputJson.dump();
                tempFile.close();
            }
            
            exit(0); // 子进程完成
        }
        else {
            // 父进程：记录子进程PID
            function_pids.push_back(pid);
            SVFUtil::outs() << "启动子进程 " << pid << " 分析函数 " << funcName << "，超时限制: " << TIMEOUT_MINUTES << " 分钟\n";
        }
        
        funcIndex++;
    }

    // 父进程等待所有函数分析子进程完成并收集结果
    for (size_t i = 0; i < function_pids.size(); i++) {
        pid_t child_pid = function_pids[i];
        bool completed = waitProcessWithTimeout(child_pid, TIMEOUT_SECONDS, "函数分析");
        
        // 读取临时文件中的结果
        if (completed) {
            std::ifstream tempFile(tempFileNames[i]);
            if (tempFile.is_open()) {
                std::string jsonStr((std::istreambuf_iterator<char>(tempFile)),
                                   std::istreambuf_iterator<char>());
                tempFile.close();
                
                if (!jsonStr.empty()) {
                    // 简单的JSON解析，不使用异常处理
                    nlohmann::json resultJson = nlohmann::json::parse(jsonStr, nullptr, false);
                    if (!resultJson.is_discarded()) {
                        allResults.push_back(resultJson);
                    }
                }
            }
        } else {
            // 进程被强杀，记录超时结果
            nlohmann::json timeoutResult;
            timeoutResult["function_name"] = "timeout_killed";
            timeoutResult["error"] = "Process killed due to timeout (" + std::to_string(TIMEOUT_MINUTES) + " minutes)";
            timeoutResult["timeout"] = true;
            allResults.push_back(timeoutResult);
        }
        
        // 删除临时文件
        std::remove(tempFileNames[i].c_str());
    }

    // 创建最终的 JSON 对象
    nlohmann::json finalJson;
    finalJson["hap_name"] = lib.name;
    finalJson["so_name"] = lib.soName;
    finalJson["module_name"] = lib.name;
    finalJson["functions"] = allResults;

    JsonOutput::writeToFile(outputfilename, finalJson.dump(4, ' ', false));

    stats.taintAnalysisTime = taintTimer.elapsed();
    taintTimer.printElapsed();

    // 清理内存
    delete vfg;
    vfg = nullptr;
    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();
    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    stats.totalLibraryTime = totalTimer.elapsed();
    totalTimer.printElapsed();

    // 写入单个库的时间统计文件
    stats.writeToFile(outputfilename);

    return stats;
}

int main(int argc, char ** argv)
{
    Timer totalProgramTimer("整个程序");
    
    // 检查命令行参数
    if (argc < 2) {
        SVFUtil::errs() << "用法: " << argv[0] << " <项目路径>\n";
        SVFUtil::errs() << "示例: " << argv[0] << " napi_project/HarmonyXFlowBench/native_array_get\n";
        return 1;
    }

    // 项目解析计时
    Timer parsingTimer("项目解析");
    SVFUtil::outs() << "开始解析项目: " << argv[1] << "\n";
    
    // 调用ProjectParser解析项目
    ProjectParser projectParser(argv[1]);
    std::vector<LibraryInfo> libraries = projectParser.getLibraries();
    
    double projectParsingTime = parsingTimer.elapsed();
    parsingTimer.printElapsed();
    
    SVFUtil::outs() << "共发现 " << libraries.size() << " 个库需要分析\n";
    
    // 存储所有库的时间统计
    std::vector<TimeStats> allLibraryStats;

    if (DEBUG_MODE) {
        // 调试模式：串行处理每个库
        SVFUtil::outs() << "使用调试模式：串行处理库\n";
        for (const LibraryInfo& lib : libraries) {
            SVFUtil::outs() << "开始分析库: " << lib.name << "\n";
            TimeStats stats = analyzeSingleLibrary(lib);
            allLibraryStats.push_back(stats);
        }
    } else {
        // 生产模式：使用多进程
        SVFUtil::outs() << "使用生产模式：多进程处理库\n";
        std::vector<pid_t> child_pids;
        const int LIBRARY_TIMEOUT_MINUTES = 30; // 库级别超时时间：30分钟
        const int LIBRARY_TIMEOUT_SECONDS = LIBRARY_TIMEOUT_MINUTES * 60;
        
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
                SVFUtil::outs() << "启动库分析进程 " << pid << " 分析库 " << lib.name 
                               << "，超时限制: " << LIBRARY_TIMEOUT_MINUTES << " 分钟\n";
            }
        }

        // 父进程等待所有子进程完成（带超时机制）
        for (pid_t child_pid : child_pids) {
            waitProcessWithTimeout(child_pid, LIBRARY_TIMEOUT_SECONDS, "库分析");
        }
        
        // 在多进程模式下，需要从文件中读取各个库的统计信息
        // 因为子进程的统计信息无法直接返回给父进程
        SVFUtil::outs() << "多进程模式下，各库的详细时间统计请查看对应的 .timing.json 文件\n";
    }
    
    double totalProgramTime = totalProgramTimer.elapsed();
    totalProgramTimer.printElapsed();
    
    // 生成全量时间统计文件
    fs::path resultDir = fs::current_path() / "result";
    std::string summaryFile = (resultDir / "overall_timing_summary.json").string();
    
    nlohmann::json summaryJson;
    summaryJson["analysis_summary"] = {
        {"project_path", argv[1]},
        {"total_libraries", libraries.size()},
        {"project_parsing_time_seconds", projectParsingTime},
        {"total_program_time_seconds", totalProgramTime},
        {"execution_mode", DEBUG_MODE ? "debug_serial" : "production_multiprocess"}
    };
    
    if (DEBUG_MODE && !allLibraryStats.empty()) {
        // 在调试模式下，我们有所有库的详细统计信息
        nlohmann::json librariesJson = nlohmann::json::array();
        double totalSvfTime = 0.0, totalPropertyTime = 0.0, totalTaintTime = 0.0, totalLibraryTime = 0.0;
        
        for (const TimeStats& stats : allLibraryStats) {
            nlohmann::json libJson;
            libJson["library_name"] = stats.libraryName;
            libJson["svf_construction_time_seconds"] = stats.svfConstructionTime;
            libJson["property_analysis_time_seconds"] = stats.propertyAnalysisTime;
            libJson["taint_analysis_time_seconds"] = stats.taintAnalysisTime;
            libJson["total_library_time_seconds"] = stats.totalLibraryTime;
            
            librariesJson.push_back(libJson);
            
            totalSvfTime += stats.svfConstructionTime;
            totalPropertyTime += stats.propertyAnalysisTime;
            totalTaintTime += stats.taintAnalysisTime;
            totalLibraryTime += stats.totalLibraryTime;
        }
        
        summaryJson["libraries_detail"] = librariesJson;
        summaryJson["aggregated_stats"] = {
            {"total_svf_construction_time_seconds", totalSvfTime},
            {"total_property_analysis_time_seconds", totalPropertyTime},
            {"total_taint_analysis_time_seconds", totalTaintTime},
            {"total_all_libraries_time_seconds", totalLibraryTime}
        };
    }
    
    std::ofstream summaryOutFile(summaryFile);
    if (summaryOutFile.is_open()) {
        summaryOutFile << summaryJson.dump(4, ' ', false);
        summaryOutFile.close();
        SVFUtil::outs() << "整体时间统计写入文件: " << summaryFile << "\n";
    }

    llvm::llvm_shutdown();
    return 0;
}

