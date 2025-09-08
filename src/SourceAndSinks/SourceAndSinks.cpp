#include "SourceAndSinks/SourceAndSinks.h"
#include <algorithm>

// 构造函数
SourceAndSinks::SourceAndSinks() {
    sinks.push_back("_ZNSt4__n1lsINS_11char_traitsIcEEEERNS_13basic_ostreamIcT_EES6_PKc");
    sinks.push_back("OH_LOG_Print");
    sinks.push_back("malloc");
    sinks.push_back("_Z7xmallocj");
    sinks.push_back("_Znwj");
    sinks.push_back("_Znaj");
    // 添加strcat相关函数
    sinks.push_back("strcat");
    sinks.push_back("strncat");
    // 添加内存分配相关函数
    sinks.push_back("realloc");
    sinks.push_back("calloc");
    sinks.push_back("strcpy");
    sinks.push_back("strncpy");
    sinks.push_back("atoi");
    sinks.push_back("atol");
    sinks.push_back("atoll");
    sinks.push_back("atoq");
    sinks.push_back("memcpy");
}

// 添加sink函数名称
void SourceAndSinks::addSink(const std::string& sinkName) {
    sinks.push_back(sinkName);
}

// 获取所有sink函数名称
const std::vector<std::string>& SourceAndSinks::getSinks() const {
    return sinks;
}

// 检查某个sink是否存在
bool SourceAndSinks::is_sink_methods(const std::string& sinkName) {
    return std::find(sinks.begin(), sinks.end(), sinkName) != sinks.end();
}