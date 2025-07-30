#ifndef SOURCEANDSINKS_H
#define SOURCEANDSINKS_H

#include <string>
#include <vector>

class SourceAndSinks {
public:
    // 构造函数
    SourceAndSinks();
    
    // 添加sink函数名称
    void addSink(const std::string& sinkName);
    
    // 获取所有sink函数名称
    const std::vector<std::string>& getSinks() const;
    
    // 检查某个sink是否存在
    bool is_sink_methods(const std::string& sinkName);

private:
    // 存储sink函数名称的数组
    std::vector<std::string> sinks;
};

#endif // SOURCEANDSINKS_H