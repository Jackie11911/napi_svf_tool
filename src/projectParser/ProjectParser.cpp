#include "projectParser/ProjectParser.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <set>

namespace fs = std::filesystem;

ProjectParser::ProjectParser(const std::string& path) : projectPath(path) {
    // 检查路径是否存在
    if (!fs::exists(projectPath)) {
        std::cerr << "项目路径不存在: " << projectPath.string() << std::endl;
        return;
    }
    
    // 设置默认CMake路径
    const char* harmonyOsPath = std::getenv("HARMONY_OS_PATH");
    if (harmonyOsPath != nullptr) {
        cmakePath = std::string(harmonyOsPath) + "/command-line-tools/sdk/default/openharmony/native/build-tools/cmake/bin/cmake";
    } else {
        // 如果环境变量未设置，使用系统默认的cmake
        cmakePath = "cmake";
    }
    
    // 设置wllvm环境
    setupWLLVMEnvironment();
    
    // 获取所有CMake项目目录
    std::vector<fs::path> cmakeDirs = getCMakeProjectDirs();
    
    // 对每个CMake项目目录执行编译和提取操作
    for (const auto& cmakeDir : cmakeDirs) {
        std::cout << "处理CMake项目: " << cmakeDir.string() << std::endl;
        
        // 设置当前处理的build目录
        buildDir = cmakeDir / "build";
        
        // 生成项目名称
        std::string projectName = extractProjectName(cmakeDir);

        // 编译并提取bitcode
        if (compileWithWLLVMForDir(cmakeDir)) {
            extractBitcodeForDir(cmakeDir,projectName);
        } else {
            std::cerr << "编译失败: " << cmakeDir.string() << std::endl;
        }
    }
}

void ProjectParser::setCMakePath(const std::string& path) {
    cmakePath = path;
}

std::string ProjectParser::extractProjectName(const fs::path& cmakeDir) {
    // 获取 projectPath 和 cmakeDir 的相对路径
    fs::path relativePath = fs::relative(cmakeDir, projectPath);

    // 将路径中的分隔符替换为下划线
    std::string projectNameWithUnderscores = projectPath.filename().string();
    for (const auto& part : relativePath) {
        projectNameWithUnderscores += "_" + part.string();
    }

    
    return projectNameWithUnderscores;
}


std::vector<fs::path> ProjectParser::getCMakeProjectDirs() const {
    std::vector<fs::path> cmakeDirs;
    std::set<fs::path> visitedParents;

    // 递归遍历项目路径，查找所有包含 CMakeLists.txt 的文件夹
    for (const auto& entry : fs::recursive_directory_iterator(projectPath)) {
        if (entry.is_regular_file() && entry.path().filename() == "CMakeLists.txt") {
            fs::path parentDir = entry.path().parent_path();

            // 检查是否已经记录过该父文件夹或其祖先
            bool isNewParent = true;
            for (const auto& visited : visitedParents) {
                if (parentDir.string().find(visited.string()) == 0) {
                    isNewParent = false;
                    break;
                }
            }

            if (isNewParent) {
                cmakeDirs.push_back(parentDir);
                visitedParents.insert(parentDir);
                std::cout << "找到CMake项目目录: " << parentDir.string() << std::endl;
            }
        }
    }

    if (cmakeDirs.empty()) {
        std::cerr << "警告: 未找到包含CMakeLists.txt的项目文件夹" << std::endl;
    }

    return cmakeDirs;
}

void ProjectParser::setupWLLVMEnvironment() {
    // 设置wllvm所需的环境变量
    std::system("source ~/venv/bin/activate");
    
    const char* harmonyOsPath = std::getenv("HARMONY_OS_PATH");
    if (harmonyOsPath == nullptr) {
        std::cerr << "错误: 未设置HARMONY_OS_PATH环境变量" << std::endl;
        return;
    }
    
    // 设置环境变量
    std::string llvmPath = std::string(harmonyOsPath) + "/command-line-tools/sdk/default/openharmony/native/llvm/bin";
    std::string cmakeBinPath = std::string(harmonyOsPath) + "/command-line-tools/sdk/default/openharmony/native/build-tools/cmake/bin";
    
    setenv("PATH", (llvmPath + ":" + std::string(getenv("PATH"))).c_str(), 1);
    setenv("PATH", (cmakeBinPath + ":" + std::string(getenv("PATH"))).c_str(), 1);
    setenv("LLVM_COMPILER", "clang", 1);
    setenv("WLLVM_OUTPUT_LEVEL", "DEBUG", 1);
    setenv("HARMONY_LLVM_BIN", llvmPath.c_str(), 1);
    setenv("OHOS_ARCH", "armeabi-v7a", 1);
}

bool ProjectParser::compileWithWLLVMForDir(const fs::path& cmakeDir) {
    // 获取当前工作目录
    fs::path currentPath = fs::current_path();
    
    if (cmakeDir.empty()) {
        return false;
    }
    
    fs::current_path(cmakeDir);
    
    // 先删除build目录（如果存在）
    std::cout << "删除旧的build目录..." << std::endl;
    if (fs::exists("build")) {
        fs::remove_all("build");
    }
    
    // 创建并切换到build目录
    std::cout << "创建build目录..." << std::endl;
    fs::create_directories("build");
    
    // 获取环境变量
    const char* harmonyOsPath = std::getenv("HARMONY_OS_PATH");
    if (harmonyOsPath == nullptr) {
        std::cerr << "错误: 未设置HARMONY_OS_PATH环境变量" << std::endl;
        fs::current_path(currentPath);
        return false;
    }
    
    // 重新设置WLLVM环境变量，确保在当前进程中可用
    std::string llvmPath = std::string(harmonyOsPath) + "/command-line-tools/sdk/default/openharmony/native/llvm/bin";
    std::string cmakeBinPath = std::string(harmonyOsPath) + "/command-line-tools/sdk/default/openharmony/native/build-tools/cmake/bin";
    
    // 获取WLLVM路径（假设安装在Python虚拟环境中）
    std::string wllvmPath = std::string(getenv("HOME")) + "/venv/bin";
    
    // 设置环境变量
    std::string pathEnv = wllvmPath + ":" + llvmPath + ":" + cmakeBinPath + ":" + std::string(getenv("PATH"));
    setenv("PATH", pathEnv.c_str(), 1);
    setenv("LLVM_COMPILER", "clang", 1);
    setenv("WLLVM_OUTPUT_LEVEL", "DEBUG", 1);
    setenv("HARMONY_LLVM_BIN", llvmPath.c_str(), 1);
    setenv("OHOS_ARCH", "armeabi-v7a", 1);
    
    // 确认wllvm在PATH中
    std::cout << "检查wllvm是否在PATH中..." << std::endl;
    int checkWllvm = std::system("which wllvm");
    if (checkWllvm != 0) {
        std::cerr << "错误: wllvm未找到，请确保已正确安装并添加到PATH中" << std::endl;
        fs::current_path(currentPath);
        return false;
    }
    
    // 执行CMake命令
    std::cout << "执行CMake配置..." << std::endl;
    std::string toolchainPath = std::string(harmonyOsPath) + "/command-line-tools/sdk/default/openharmony/native/build/cmake/ohos.toolchain.svf.cmake";
    std::string cmakeConfigCmd = "cd build && \"" + cmakePath + "\" -DCMAKE_C_COMPILER=wllvm -DCMAKE_CXX_COMPILER=wllvm++ -DOHOS_STL=c++_shared -DOHOS_ARCH=armeabi-v7a -DOHOS_PLATFORM=OHOS -DCMAKE_TOOLCHAIN_FILE=\"" + toolchainPath + "\" ..";
    int configResult = std::system(cmakeConfigCmd.c_str());
    if (configResult != 0) {
        std::cerr << "错误: CMake配置失败" << std::endl;
        fs::current_path(currentPath);
        return false;
    }
    
    // 执行CMake构建命令
    std::cout << "执行CMake构建..." << std::endl;
    std::string cmakeBuildCmd = "cd build && \"" + cmakePath + "\" --build .";
    int buildResult = std::system(cmakeBuildCmd.c_str());
    if (buildResult != 0) {
        std::cerr << "错误: CMake构建失败" << std::endl;
        fs::current_path(currentPath);
        return false;
    }
    
    // 切换回原来的目录
    fs::current_path(currentPath);
    
    return true;
}

std::vector<std::string> ProjectParser::findSOFilesForDir(const fs::path& cmakeDir) const {
    std::vector<std::string> soFiles;
    
    fs::path buildDir = cmakeDir / "build";
    if (!fs::exists(buildDir)) {
        std::cerr << "错误: build目录不存在: " << buildDir.string() << std::endl;
        return soFiles;
    }
    
    // 递归查找所有.so文件
    for (const auto& entry : fs::recursive_directory_iterator(buildDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".so") {
            soFiles.push_back(entry.path().string());
        }
    }
    
    return soFiles;
}

bool ProjectParser::extractBitcodeForDir(const fs::path& cmakeDir, std::string projectName) {
    bool allSuccess = true;
    
    // 获取指定目录下的所有.so文件
    std::vector<std::string> soFiles = findSOFilesForDir(cmakeDir);
    if (soFiles.empty()) {
        std::cerr << "错误: 在目录 " << cmakeDir.string() << " 中找不到.so文件" << std::endl;
        return false;
    }
    
    // 提取每个.so文件的bitcode
    for (const auto& soFile : soFiles) {
        if (!extractBitcodeForFile(soFile, projectName)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool ProjectParser::extractBitcodeForFile(const std::string& soFile, std::string projectName) {
    // 保存当前工作目录
    fs::path currentPath = fs::current_path();
    
    fs::path soPath(soFile);
    std::string soName = soPath.filename().string();
    std::string libName = soName;
    
    // 从soName中提取库名（去掉lib前缀和.so后缀）
    if (libName.substr(0, 3) == "lib") {
        libName = libName.substr(3); // 去掉"lib"前缀
    }
    size_t soPos = libName.find(".so");
    if (soPos != std::string::npos) {
        libName = libName.substr(0, soPos); // 去掉".so"后缀
    }
    
    // 构建输出bitcode文件路径
    fs::path bcPath = soPath.parent_path() / (soName + ".bc");
    
    // 切换到.so文件所在目录
    fs::path soDir = soPath.parent_path();
    std::cout << "切换到目录: " << soDir.string() << std::endl;
    fs::current_path(soDir);
    
    // 执行extract-bc命令提取bitcode
    std::cout << "提取bitcode: " << soPath.filename().string() << std::endl;
    std::string extractCmd = "extract-bc \"" + soPath.filename().string() + "\" -v"; // 添加-v参数获取更多信息
    int extractResult = std::system(extractCmd.c_str());
    
    // 切换回原来的目录
    fs::current_path(currentPath);
    
    if (extractResult != 0) {
        std::cerr << "错误: 提取bitcode失败: " << soFile << std::endl;
        
        // 尝试手动检查.llvm_bc节
        std::cout << "手动检查.llvm_bc节..." << std::endl;
        std::string checkSectionCmd = "llvm-objdump -h \"" + soFile + "\" | grep llvm_bc";
        int checkResult = std::system(checkSectionCmd.c_str());
        if (checkResult != 0) {
            std::cerr << "错误: 文件中不存在.llvm_bc节" << std::endl;
        }
        
        return false;
    }
    
    // 检查生成的.bc文件是否存在
    if (!fs::exists(bcPath)) {
        std::cerr << "错误: 生成的bitcode文件不存在: " << bcPath.string() << std::endl;
        return false;
    }
    
    // 创建库信息并添加到列表
    LibraryInfo libInfo;
    libInfo.name = libName;
    libInfo.soName = soName;
    libInfo.finalLLVMIR = bcPath.string();
    libInfo.projectName = projectName; // 设置项目名称
    
    libraries.push_back(libInfo);
    
    std::cout << "成功提取bitcode: " << bcPath.string() << std::endl;
    return true;
}

std::vector<std::string> ProjectParser::findFiles(const std::string& extension) const {
    std::vector<std::string> files;
    
    for (const auto& entry : fs::recursive_directory_iterator(projectPath)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.push_back(entry.path().string());
        }
    }
    
    return files;
}


std::vector<std::string> ProjectParser::getLLVMIRFiles() const {
    std::vector<std::string> bcFiles;
    
    // 从已解析的库信息中获取bitcode文件路径
    for (const auto& lib : libraries) {
        if (!lib.finalLLVMIR.empty() && fs::exists(lib.finalLLVMIR)) {
            bcFiles.push_back(lib.finalLLVMIR);
        }
    }
    
    return bcFiles;
}


const std::vector<LibraryInfo>& ProjectParser::getLibraries() const {
    return libraries;
}