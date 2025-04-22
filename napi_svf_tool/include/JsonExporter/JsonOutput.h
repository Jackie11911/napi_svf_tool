#ifndef JSONOUTPUT_H
#define JSONOUTPUT_H

#include "taintanalysis/TargetUnit.h"
#include <nlohmann/json.hpp>
#include <vector>

class JsonOutput {
public:
    // 将TargetUnit数组转换为JSON字符串
    static std::string generateJson(const std::vector<TargetUnit>& units);
    static nlohmann::json generateJson(const std::vector<TargetUnit>& units, const std::string& functionName, const std::vector<unsigned int>& paramMap);
    
    static bool writeToFile(const std::string& filename, const std::string& jsonContent);
    static bool writeToFile(const std::vector<TargetUnit>& units, const std::string& filename);
    static bool writeToFile(const std::vector<TargetUnit>& units, const std::string& functionName, const std::vector<unsigned int>& paramMap, const std::string& filename);
    
private:
    // 将单个TargetUnit转换为JSON对象
    static nlohmann::json unitToJson(const TargetUnit& unit);
};

#endif // JSONOUTPUT_H