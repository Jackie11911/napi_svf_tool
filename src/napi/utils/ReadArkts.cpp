// ReadArkts.cpp
#include "napi/utils/ReadArkts.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cerrno>

using namespace std;

bool ReadArkts::parseFile(const std::string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename 
             << " (Error code: " << errno << ")" << endl;  // 添加错误日志
        return false;
    }

    string line;
    while (getline(file, line)) {
        // 删除line最后的/r或/n
        if (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        parseLine(line);
    }
    return true;
}

const vector<FunctionInfo>& ReadArkts::getFunctions() const {
    return functions;
}

bool ReadArkts::hasFunction(const std::string& funcName) const {
    return std::any_of(functions.begin(), functions.end(),
                       [funcName](const FunctionInfo& func) { return func.name == funcName; });
}


void ReadArkts::parseLine(const std::string& line) {
    smatch match;
    if (!regex_match(line, match, funcPattern)) return;

    FunctionInfo func;
    func.name = match[1];
    func.returnType = match[3];

    // 解析参数列表
    istringstream paramStream(match[2]);
    string paramPair;
    while (getline(paramStream, paramPair, ',')) {
        smatch paramMatch;
        if (regex_match(paramPair, paramMatch, paramPattern)) {
            func.params.push_back({paramMatch[1], paramMatch[2]});
        }
    }

    functions.push_back(func);
}