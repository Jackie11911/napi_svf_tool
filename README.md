# 源代码分析
# 源代码侧跨语言分析工具

## 开发环境配置
本项目的开发环境需配置在Ubuntu-24.04中
### 鸿蒙项目LLVM IR生成
使用命令行CMake构建NDK工程-构建NDK工程-NDK开发 - 华为HarmonyOS开发者
参考官方教程配置linux中鸿蒙NDK开发环境
生成llvm IR参考指令如下
```plaintext
1.
command-line-tools/sdk/default/openharmony/native/llvm/bin/clang++ --target=arm-linux-ohos --sysroot=command-line-tools/sdk/default/openharmony/native/sysroot -S -c -Xclang -disable-O0-optnone -fno-discard-value-names -emit-llvm target.cpp -o target.ll
​
2.
/home/jackie/harmonyOs/command-line-tools/sdk/default/openharmony/native/llvm/bin/opt -S --mem2reg target.ll -o
target.ll
​
```
第一个指令生成ll文件，保留变量名
第二个指令用opt的mem2reg优化，将栈变量提升为寄存器形式
### SVF解析
#### SVF环境搭建：
参考NAPI_SVF_TOOL/README.md，将env.sh中的内容配置到环境变量中，即可进行编译开发。
源代码于NAPI_SVF_TOOL中
相应文件夹介绍：
- JsonExporter: JsonExporter 模块负责将 summary IR（中间表示）导出为 JSON 格式。该模块主要用于将分析结果序列化，以便于后续的存储、传输或进一步处理
- SourceAndSinks:  SourceAndSinks 模块用于定义在 Native 层中需要特别关注的 Source和 Sink
- taintanalysis：源代码层的污点分析
    - 当前状态：经过内部审查，该值流传播的逻辑存在问题，正在重构，该中期检查版本为未修改过的版本
- napi：NAPI 模块用于识别 NAPI的注册点，并定义自定义的传播规则。
---
## 测试用例说明
本测试用例位于 NAPI_SVF_TOOL/napi_project/napi_test2 目录下，主要用于验证常见的 NAPI 调用，并包含日志记录（log sink）点。
### 执行文件
测试使用的可执行文件为：
```plaintext
NAPI_SVF_TOOL/napi_svf_tool
```
### 运行指令
请按照以下命令运行测试：
```plaintext
./napi_svf_tool napi_project/napi_test2/napi_init.ll
```
其中，napi_init.ll 是预先生成的 LLVM IR 文件。
### 输出结果
执行完成后，分析结果将保存在：
```plaintext
/NAPI_SVF_TOOL/result/taint_analysis_result.json
```