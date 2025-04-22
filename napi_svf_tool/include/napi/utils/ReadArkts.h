// ReadArkts.h
#pragma once

#include <vector>
#include <string>
#include <regex>

struct FunctionParam {
    std::string name;
    std::string type;
};

struct FunctionInfo {
    std::string name;
    std::vector<FunctionParam> params;
    std::string returnType;
};

class ReadArkts {
public:
    // 解析入口
    bool parseFile(const std::string& filename);
    
    // 获取解析结果
    const std::vector<FunctionInfo>& getFunctions() const;

    bool hasFunction(const std::string& funcName) const;

private:
    // 实际解析实现
    void parseLine(const std::string& line);
    
    // 数据存储
    std::vector<FunctionInfo> functions;

    

    const std::regex funcPattern{R"(export\s+const\s+(\w+):\s*\((.*?)\)\s*=>\s*([\w$<>]+)\s*;)"};
    const std::regex paramPattern{R"(\s*([\w$]+)\s*:\s*([\w|]+)\s*)"};

};