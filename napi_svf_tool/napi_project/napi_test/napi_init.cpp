#include "napi/native_api.h"
#include <string>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

static napi_value Add(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);

    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);

    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    double value0;
    napi_get_value_double(env, args[0], &value0);

    double value1;
    napi_get_value_double(env, args[1], &value1);

    napi_value sum;
    napi_create_double(env, value0 + value1, &sum);

    return sum;

}

// 参考 https://developer.huawei.com/consumer/cn/doc/harmonyos-guides-V5/use-napi-about-string-V5

static napi_value PassDownString(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);// args = Call napi_get_cb_info env, info, 1, args, nullptr, nullptr

    size_t length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &length);// length = Call napi_get_value_string_utf8 env, args[0], nullptr, 0, &length
    char* buffer = (char*)malloc(length + 1);
    napi_get_value_string_utf8(env, args[0], buffer, length + 1, &length);// buffer =  Call napi_get_value_string_utf8 env, args[0], buffer, length + 1, length
    
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);// fd = Call open "output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644
    write(fd, buffer, length);//Call write fd, buffer, length
    close(fd);// Call close fd
    
    napi_value result;
    napi_create_string_utf8(env, buffer, length, &result);// result = Call napi_create_string_utf8 env, buffer, length, &result

    free(buffer);// Call free buffer
    return result;// ret result
}

static napi_value PassDownNumber(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    double value;
    napi_get_value_double(env, args[0], &value);

    napi_value result;
    napi_create_double(env, value, &result);
    return result;
}


static napi_value PassDownBoolean(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    bool value;
    napi_get_value_bool(env, args[0], &value);

    napi_value result;
    napi_get_boolean(env, value, &result);
    return result;
}

static napi_value GetString(napi_env env, napi_callback_info info) {
    const char *str = u8"你好, World!, successes to create UTF-8 string! 111";
    size_t length = strlen(str);                                        
    napi_value result = nullptr;
    napi_status status = napi_create_string_utf8(env, str, length, &result);
    if (status != napi_ok) {
        napi_throw_error(env, nullptr, "Failed to create UTF-8 string");
        return nullptr;
    }
    return result;
}

void writeToFile(const std::string& filename, const std::string& content) {
    std::ofstream outFile(filename, std::ios::out);
    if (!outFile) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return;
    }
    outFile << content;
    outFile.close();
    std::cout << "Result written to file: " << filename << std::endl;
}

static napi_value callCFunction(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    // 获取ArkTS侧入参
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    // 获取全局对象，这里用global是因为napi_call_function的第二个参数是JS函数的this入参。
    napi_value global = nullptr;
    napi_get_global(env, &global);
    // 调用ArkTS方法
    napi_value result = nullptr;
    // 调用napi_call_function时传入的argv的长度必须大于等于argc声明的数量，且被初始化成nullptr
    napi_call_function(env, global, argv[0], argc, argv, &result);
    size_t strSize = 0;
    napi_get_value_string_utf8(env, result, nullptr, 0, &strSize); // 获取字符串长度
    char* buffer = new char[strSize + 1]; // 分配缓冲区
    napi_get_value_string_utf8(env, result, buffer, strSize + 1, nullptr); // 获取字符串内容
    
    std::string resultString(buffer);
    std::string filename = "/data/result.txt";
    writeToFile(filename, resultString);
    
    return result;
}

static napi_value callCFunction_name(napi_env env, napi_callback_info info) {
    
    napi_value global, add_two, arg;
    napi_status status = napi_get_global(env, &global);
    status = napi_get_named_property(env, global, "Add", &add_two);
    status = napi_create_int32(env, 1337, &arg);

    napi_value* argv = &arg;
    size_t argc = 1;
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    
    napi_value return_val;
    status = napi_call_function(env, global, add_two, argc, argv, &return_val);
    
    int32_t result;
    status = napi_get_value_int32(env, return_val, &result);
}

// 拼接两个字符串的函数
static napi_value ConcatStrings(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 确保传入的参数是字符串
    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);
    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    if (valuetype0 != napi_string || valuetype1 != napi_string) {
        napi_throw_type_error(env, nullptr, "Both arguments must be strings.");
        return nullptr;
    }

    // 获取字符串长度
    size_t length1, length2;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &length1);
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &length2);

    // 分配缓冲区以存储拼接后的字符串
    std::string result;
    result.resize(length1 + length2 + 1);

    // 获取字符串数据并拼接
    napi_get_value_string_utf8(env, args[0], &result[0], length1 + 1, &length1);
    napi_get_value_string_utf8(env, args[1], &result[length1], length2 + 1, &length2);

    // 创建返回的字符串对象
    napi_value result_value;
    napi_create_string_utf8(env, result.c_str(), result.length(), &result_value);

    return result_value;
}


EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"add", nullptr, Add, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"passDownString", nullptr, PassDownString, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getString", nullptr, GetString, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"passDownNumber", nullptr, PassDownNumber, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"passDownBoolean", nullptr, PassDownBoolean, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"concatStrings", nullptr, ConcatStrings, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"callFunction", nullptr, callCFunction, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
