#ifndef ANALYZE_PROPERTIES_H
#define ANALYZE_PROPERTIES_H

#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include <set>
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"
#include "napi/utils/ReadArkts.h"
class NapiPropertiesAnalyzer {
public:
    static std::set<llvm::GlobalVariable*> analyzeNapiProperties(SVF::SVFG* svfg,SVF::SVFIR* pag);
    static std::set<llvm::Function*> analyzeGlobalVars(std::set<llvm::GlobalVariable*> globalVars, ReadArkts& readArkts);
};

#endif // ANALYZE_PROPERTIES_H