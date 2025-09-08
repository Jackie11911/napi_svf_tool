#ifndef PROJECT_PARSER_H
#define PROJECT_PARSER_H

#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <fstream>

// 定义一个结构体来存储so库的信息
struct LibraryInfo {
    std::string name;              // 库名称
    std::string soName;           // so文件名称
    std::string finalLLVMIR;      // 最终的LLVM IR文件（可能是合并后的）
    std::string projectName;
};

class ProjectParser {
public:
    // 构造函数，接收项目路径作为参数
    ProjectParser(const std::string& path);

    // 设置CMake路径
    void setCMakePath(const std::string& path);

    // 获取所有的LLVM IR文件路径（.bc文件）
    std::vector<std::string> getLLVMIRFiles() const;


    // 获取解析到的库信息
    const std::vector<LibraryInfo>& getLibraries() const;
    
    // 使用wllvm编译项目并提取bitcode
    bool compileWithWLLVM();
    

private:
    std::filesystem::path projectPath;
    std::vector<LibraryInfo> libraries;
    std::filesystem::path buildDir;
    std::string cmakePath;  // CMake可执行文件路径
    // 日志目录与当前项目日志文件
    std::filesystem::path logDir;
    std::filesystem::path currentLogFile;
    mutable std::ofstream logStream; // 允许在const方法中写日志

    // 从路径中提取项目名称
    std::string extractProjectName(const std::filesystem::path& cmakeDir);

    // 递归查找特定类型的文件
    std::vector<std::string> findFiles(const std::string& extension) const;

    
    // 获取所有CMake项目目录
    std::vector<std::filesystem::path> getCMakeProjectDirs() const;
    
    // 设置wllvm所需的环境变量
    void setupWLLVMEnvironment();
    
    
    // 查找指定目录下的.so文件
    std::vector<std::string> findSOFilesForDir(const std::filesystem::path& cmakeDir) const;
    
    // 为单个.so文件提取bitcode
    bool extractBitcodeForFile(const std::string& soFile, std::string projectName);
    
    // 对指定目录执行编译
    bool compileWithWLLVMForDir(const std::filesystem::path& cmakeDir);
    
    // 对指定目录提取bitcode
    bool extractBitcodeForDir(const std::filesystem::path& cmakeDir, std::string projectName);

    // 日志工具
    std::string makeLogFileNameFromPath(const std::filesystem::path& p) const; // 将路径中的分隔符替换为"_"
    void openProjectLog(const std::filesystem::path& cmakeDir); // 为单个项目打开日志
    void closeProjectLog();
    void logInfo(const std::string& msg) const;
    void logError(const std::string& msg) const;
    // 在命令后追加输出重定向到当前日志文件
    std::string withLogRedirect(const std::string& cmd) const;
};

#endif // PROJECT_PARSER_H