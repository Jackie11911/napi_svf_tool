#include "JsonExporter/SummaryExporter.h"
#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

std::string SummaryExporter::toJson(const std::vector<SummaryItem>& summaryItems) {
    json j;
    
    for (const auto& item : summaryItems) {
        json itemJson;
        if(item.getInstructionType() == "Call"){
            itemJson["function_name"] = item.getFunctionName();
            itemJson["type"] = item.getInstructionType();
        
            // 处理返回值
            json retsJson;
            for (const auto& [node, value] : item.getRetValues()) {
                retsJson[node] = value;
            }
            itemJson["rets"] = retsJson;
            
            // 处理操作数
            json operandsJson = json::array();
            for (const auto& operand : item.getOperands()) {
                operandsJson.push_back(operand);
            }
            itemJson["operands"] = operandsJson;
        } else if(item.getInstructionType() == "Ret") {
            itemJson["type"] = item.getInstructionType();
            // Ret指令处理
            if(!item.getOperands().empty()) {
                json operandsJson = json::array();
                for (const auto& operand : item.getOperands()) {
                    operandsJson.push_back(operand);
                }
                itemJson["operands"] = operandsJson;
            }
        } else if(item.getInstructionType() == "Phi") {
            itemJson["type"] = item.getInstructionType();
            // Phi指令处理
            // 处理操作数
            json operandsJson = json::array();
            for (const auto& operand : item.getOperands()) {
                operandsJson.push_back(operand);
            }
            itemJson["operands"] = operandsJson;
            
            // 处理返回值
            if(!item.getRetValues().empty()) {
                itemJson["ret"] = item.getRetValues()[0].first;
            }
        }
        
        
        j.push_back(itemJson);
    }
    
    return j.dump(4); // 使用4个空格缩进美化输出
}


void SummaryExporter::exportToFile(const std::vector<SummaryItem>& summaryItems, const std::string& filename) {
    std::string fullPath = "result/" + filename;
    if (fullPath.find(".json") == std::string::npos) {
        fullPath += ".json";
    }
    
    // 创建result目录（不使用异常处理）
    if (!std::filesystem::exists("result")) {
        if (!std::filesystem::create_directory("result")) {
            return;
        }
    }
    
    std::string jsonStr = toJson(summaryItems);
    std::ofstream outFile(fullPath);
    
    if (!outFile.is_open()) {
        return;
    }
    
    outFile << jsonStr;
    outFile.close();
    
    return;
}