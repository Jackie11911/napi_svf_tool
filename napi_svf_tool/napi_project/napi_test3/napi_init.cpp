#include "napi/native_api.h"
#include "hilog/log.h"
#include <cstdint>
#include <cstdlib>
#include <fstream>

bool flag;

void WriteSumToFile(int64_t value0, int64_t value1) {
    std::ofstream outFile("result.txt");
    if (outFile.is_open()) {
        outFile << "Sum: " << (value0 + value1) << std::endl;
        outFile.close();
    }
}

// 获取ArkTS侧入参的的参数信息
static napi_value GetCbArgs(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);
    return args[0];
}
// 获取ArkTS侧入参的参数个数
static napi_value GetCbArgQuantity(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[5];
    napi_value result = nullptr;
    napi_get_cb_info(env, info, &argc, nullptr, nullptr, nullptr);
    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);
    napi_create_int32(env, argc, &result);
    return result;
}

static napi_value Add(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);

    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);

    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    int64_t value0;
    napi_get_value_int64(env, args[0], &value0);

    int64_t value1;
    napi_get_value_int64(env, args[1], &value1);

    napi_value sum;
    napi_create_int64(env, value0 + value1, &sum);
//     WriteSumToFile(value0, value1);

    return sum;
}

void WriteToFile(const char* buffer) {
    std::ofstream outFile("output.txt");
    if (outFile.is_open()) {
        outFile << buffer;
        outFile.close();
    }
}



static napi_value PassDownString(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    size_t length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &length);
    char* buffer = (char*)malloc(length + 1);
    napi_get_value_string_utf8(env, args[0], buffer, length + 1, &length);
    
    
    if(flag){
        free(buffer);
    }else{
        strcat(buffer, "_extra");
    }

    napi_value result;
    napi_create_string_utf8(env, buffer, length, &result);
    // WriteToFile(buffer);
    
    free(buffer);
    return result;
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

static napi_value ConstNumber(napi_env env, napi_callback_info info)
{
    napi_value ret;
    napi_create_int64(env, 1037, &ret);
    return ret;
}

static napi_value ConstString(napi_env env, napi_callback_info info)
{
    napi_value ret;
    napi_create_string_utf8(env, "Hello, OpenHarmony!", NAPI_AUTO_LENGTH, &ret);
    return ret;
}

static napi_value PhiTest(napi_env env, napi_callback_info info)
{
    napi_value a1;
    napi_create_int64(env, 1, &a1);
    napi_value a2;
    napi_create_int64(env, 2, &a2);
    int x = 65534;
    if (x*(x+1)%2 == 0){
        return a1;
    }else{
        return a2;
    }
}

static napi_value loadModule(napi_env env, napi_callback_info info) {
    napi_value result;
    // 1. 使用napi_load_module_with_info加载library
    napi_status status = napi_load_module_with_info(env, "library", "com.example.application/entry", &result);

    napi_value testFn;
    // 2. 使用napi_get_named_property获取test函数
    napi_get_named_property(env, result, "test", &testFn);
    // 3. 使用napi_call_function调用函数test
    napi_call_function(env, result, testFn, 0, nullptr, nullptr);

    napi_value value;
    napi_value key;
    napi_create_string_utf8(env, "value", NAPI_AUTO_LENGTH, &key);
    // 4. 使用napi_get_property获取变量value
    napi_get_property(env, result, key, &value);
    return result;
}

int64_t add_2_napi_value(napi_env env, napi_value v1, napi_value v2){
    int64_t value0, value1;
    napi_get_value_int64(env, v1, &value0);
    napi_get_value_int64(env, v2, &value1);
    return value0 + value1;
}

static napi_value otherFunction(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    int64_t ret = add_2_napi_value(env, args[0], args[1]);
    
    napi_value retVal;
    napi_create_int64(env, ret, &retVal);
    
    return retVal;
}


static napi_value NAPI_Global_logLeak(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    char* str = (char*) malloc(256);
    napi_get_value_string_utf8(env, args[0], str, 256, nullptr);
    OH_LOG_INFO(LOG_APP, "Oops! there is %{public}d leak: %{public}s", 1, str);
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"add", nullptr, Add, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"passDownString", nullptr, PassDownString, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"passDownNumber", nullptr, PassDownNumber, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"constNumber", nullptr, ConstNumber, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"constString", nullptr, ConstString, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"phiTest", nullptr, PhiTest, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"loadModule", nullptr, loadModule, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"otherFunction", nullptr, otherFunction, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"logLeak", nullptr, NAPI_Global_logLeak, nullptr, nullptr, nullptr, napi_default, nullptr},
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
