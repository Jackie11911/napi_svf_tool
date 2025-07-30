#ifndef NAPI_PROPAGATION_RULES_H
#define NAPI_PROPAGATION_RULES_H

#include <unordered_map>
#include <vector>
#include <string>

class NapiPropagationRules {
public:
    using SrcToDstMap = std::unordered_map<int, std::vector<int>>;

    // 禁用拷贝和赋值
    NapiPropagationRules(const NapiPropagationRules&) = delete;
    void operator=(const NapiPropagationRules&) = delete;

    static NapiPropagationRules& getInstance();
    bool hasRule(const std::string& funcName);
    bool is_napi_function(const std::string& funcName);
    const SrcToDstMap& getRule(const std::string& funcName) const;
    NapiPropagationRules();
    ~NapiPropagationRules() = default;

private:

    std::unordered_map<std::string, SrcToDstMap> rules;
    SrcToDstMap emptyRule;
};

#endif // NAPI_PROPAGATION_RULES_H