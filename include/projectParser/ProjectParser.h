#ifndef PROJECT_PARSER_H
#define PROJECT_PARSER_H

#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>

// 定义一个结构体来存储so库的信息
struct LibraryInfo {
    std::string name;              // 库名称
    std::string soName;           // so文件名称
    std::string indexDtsFile;     // 对应的 Index.d.ts 文件路径
    std::string finalLLVMIR;      // 最终的LLVM IR文件（可能是合并后的）
};

class ProjectParser {
public:
    // 构造函数，接收项目路径作为参数
    ProjectParser(const std::string& path);

    // 设置CMake路径
    void setCMakePath(const std::string& path);

    // 获取所有的Index.d.ts文件路径
    std::vector<std::string> getIndexDtsFiles() const;

    // 获取所有的LLVM IR文件路径（.bc文件）
    std::vector<std::string> getLLVMIRFiles() const;

    // 获取项目名称（用于生成输出JSON文件名）
    std::string getProjectName() const;

    // 获取输出JSON文件的路径
    std::string getOutputJsonPath() const;

    // 获取解析到的库信息
    const std::vector<LibraryInfo>& getLibraries() const;
    
    // 使用wllvm编译项目并提取bitcode
    bool compileWithWLLVM();
    
    // 提取所有.so文件的bitcode
    bool extractBitcode();

private:
    std::filesystem::path projectPath;
    std::string projectName;
    std::vector<LibraryInfo> libraries;
    std::filesystem::path buildDir;
    std::string cmakePath;  // CMake可执行文件路径

    // 从路径中提取项目名称
    void extractProjectName();

    // 递归查找特定类型的文件
    std::vector<std::string> findFiles(const std::string& extension) const;

    // 获取CMake项目目录
    std::filesystem::path getCMakeProjectDir() const;
    
    // 设置wllvm所需的环境变量
    void setupWLLVMEnvironment();
    
    // 查找所有生成的.so文件
    std::vector<std::string> findSOFiles() const;
    
    // 为单个.so文件提取bitcode
    bool extractBitcodeForFile(const std::string& soFile);
    
    // 查找对应的Index.d.ts文件
    std::string findIndexDtsFile(const std::string& soName) const;
};

#endif // PROJECT_PARSER_H 