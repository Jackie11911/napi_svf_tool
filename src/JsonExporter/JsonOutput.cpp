#include "JsonExporter/JsonOutput.h"
#include "taintanalysis/TargetUnit.h"
#include <fstream>

using json = nlohmann::json;

std::string JsonOutput::generateJson(const std::vector<TargetUnit>& units) {
    json outputJson = json::array();

    for (const auto& unit : units) {
        outputJson.push_back(unitToJson(unit));
    }

    return outputJson.dump(4, ' ', false); 
}

json JsonOutput::unitToJson(const TargetUnit& unit) {
    // 格式化参数节点
    std::vector<std::string> formattedArgs;
    for (auto arg : unit.getInstructionArgs()) {
        if (arg == 0) {
            formattedArgs.push_back("null");
        } else {
            formattedArgs.push_back("%" + std::to_string(arg));
        }
    }

    // 格式化原有节点
    std::vector<std::string> formattedOriginalNodes;
    for (auto node : unit.getOriginalTargetNodes()) {
        if (node == 0) {
            formattedOriginalNodes.push_back("null");
        } else {
            formattedOriginalNodes.push_back("%" + std::to_string(node));
        }
    }

    // 格式化新增节点
    std::vector<std::string> formattedNewNodes;
    for (auto node : unit.getNewTargetNodes()) {
        if (node == 0) {
            formattedNewNodes.push_back("null");
        } else {
            formattedNewNodes.push_back("%" + std::to_string(node));
        }
    }

    return {
        {"operands", formattedArgs},
        {"target", unit.getFunctionName()},
        {"rets", formattedNewNodes},
        {"original_taints", formattedOriginalNodes},
        {"type", unit.getInstructionType()}
    };
}

bool JsonOutput::writeToFile(const std::string& filename, const std::string& jsonContent) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        return false;
    }
    outFile << jsonContent;
    outFile.close();
    return true;
}

nlohmann::json JsonOutput::generateJson(const std::vector<TargetUnit>& units,
                                      const std::string& functionName,
                                      const std::vector<unsigned int>& paramMap) {
    json outputJson;
    
    // 添加函数名称
    outputJson["function_name"] = functionName;
    
    // 添加参数映射数组
    std::vector<json> formattedParams;
    for (auto param : paramMap) {
        if (param == 1) {
            formattedParams.push_back({"%"+std::to_string(param), "napi_env"});
        } else if (param == 2) {
            formattedParams.push_back({"%"+std::to_string(param), "napi_callback_info"});
        }
    }
    outputJson["params"] = formattedParams;
    
    // 添加原有的单元数据
    json unitsArray = json::array();
    for (const auto& unit : units) {
        unitsArray.push_back(unitToJson(unit));
    }
    outputJson["instructions"] = unitsArray;
    
    return outputJson;
}

bool JsonOutput::writeToFile(const std::vector<TargetUnit>& units,
                           const std::string& functionName,
                           const std::vector<unsigned int>& paramMap,
                           const std::string& filename) {   
    json outputJson = generateJson(units, functionName, paramMap);
    return writeToFile(filename, outputJson.dump(4, ' ', false));
}

bool JsonOutput::writeToFile(const std::vector<TargetUnit>& units, const std::string& filename) {   
    std::string jsonStr = generateJson(units);
    return writeToFile(filename, jsonStr);
}