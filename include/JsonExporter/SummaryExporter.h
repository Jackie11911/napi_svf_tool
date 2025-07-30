#ifndef SUMMARYEXPORTER_H
#define SUMMARYEXPORTER_H

#include "taintanalysis/SummaryItem.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

class SummaryExporter {
public:
    // 将SummaryItem向量转换为JSON字符串
    static nlohmann::json toJson(const std::vector<SummaryItem>& summaryItems);

    static void exportToFile(const std::vector<SummaryItem>& summaryItems, const std::string& filename);
};

#endif // SUMMARYEXPORTER_H