#include "napi/NapiPropagationRules.h"

NapiPropagationRules& NapiPropagationRules::getInstance() {
    static NapiPropagationRules instance;
    return instance;
}

// 查询某函数是否有自定义传播规则
bool NapiPropagationRules::hasRule(const std::string& funcName) {
    return rules.find(funcName) != rules.end();
}

bool NapiPropagationRules::is_napi_function(const std::string& funcName) {
    return rules.find(funcName) != rules.end();
}


// 获取某函数的传播规则
const NapiPropagationRules::SrcToDstMap& NapiPropagationRules::getRule(const std::string& funcName) const{
    auto it = rules.find(funcName);
    return (it != rules.end()) ? it->second : emptyRule;
}


NapiPropagationRules::NapiPropagationRules() {
    // 初始化规则：函数名 -> {源参数索引 : 目标参数索引列表}

    // NAPI_EXTERN napi_status napi_call_function(napi_env env,
    //                                    napi_value recv,
    //                                    napi_value func,
    //                                    size_t argc,
    //                                    const napi_value* argv,
    //                                    napi_value* result);
    rules["napi_call_function"] = {
        {1, {5}},
        {2, {5}},
        {3, {5}},
        {4, {5}}
    };

    // napi_status napi_create_function(napi_env env,
    //                          const char* utf8name,
    //                          size_t length,
    //                          napi_callback cb,
    //                          void* data,
    //                          napi_value* result);
    rules["napi_create_function"] = {
        {1, {5}},
        {2, {5}},
        {3, {5}},
        {4, {5}}
    };

    // napi_status napi_get_cb_info(napi_env env,
    //                      napi_callback_info cbinfo,
    //                      size_t* argc,
    //                      napi_value* argv,
    //                      napi_value* thisArg,
    //                      void** data)
    rules["napi_get_cb_info"] = { // 参数0传播到参数3,4,5
        {1, {3, 4, 5}}   // 参数1传播到参数3,4,5
    };

    // napi_status napi_get_new_target(napi_env env,
    //                         napi_callback_info cbinfo,
    //                         napi_value* result)
    rules["napi_get_new_target"] = {         // 参数0传播到参数1
        {1, {2}}          // 参数1传播到参数2
    };

    // napi_status napi_new_instance(napi_env env,
    //                       napi_value cons,
    //                       size_t argc,
    //                       napi_value* argv,
    //                       napi_value* result)
    rules["napi_new_instance"] = {
        {1, {4}},
        {2, {4}},
        {3, {4}}
    };

    // napi_status napi_define_class(napi_env env,
    //                       const char* utf8name,
    //                       size_t length,
    //                       napi_callback constructor,
    //                       void* data,
    //                       size_t property_count,
    //                       const napi_property_descriptor* properties,
    //                       napi_value* result);
    rules["napi_define_class"] = {
        {1, {7}},
        {2, {7}},
        {3, {7}},
        {4, {7}},
        {5, {7}},
        {6, {7}}
    };

    // napi_status napi_wrap(napi_env env,
    //               napi_value js_object,
    //               void* native_object,
    //               napi_finalize finalize_cb,
    //               void* finalize_hint,
    //               napi_ref* result);
    rules["napi_wrap"] = {
        {1, {5}},
        {2, {5}},
        {3, {5}},
        {4, {5}}
    };
    
    // napi_status napi_unwrap(napi_env env,
    //                 napi_value js_object,
    //                 void** result);
    rules["napi_unwrap"] = {
        {1, {2}}
    };
    
    // napi_status napi_remove_wrap(napi_env env,
    //                      napi_value js_object,
    //                      void** result);
    rules["napi_remove_wrap"] = {
        {1, {2}}
    };

    // napi_status napi_check_object_type_tag(napi_env env,
    //                                napi_value js_object,
    //                                const napi_type_tag* type_tag,
    //                                bool* result);
    rules["napi_check_object_type_tag"] = {
        {1, {3}},
        {2, {3}}
    };
    
    // napi_status napi_add_finalizer(napi_env env,
    //                        napi_value js_object,
    //                        void* finalize_data,
    //                        node_api_basic_finalize finalize_cb,
    //                        void* finalize_hint,
    //                        napi_ref* result);
    rules["napi_add_finalizer"] = {
        {1, {5}},
        {2, {5}},
        {3, {5}},
        {4, {5}}
    };

    // napi_status napi_create_async_work(napi_env env,
    //                            napi_value async_resource,
    //                            napi_value async_resource_name,
    //                            napi_async_execute_callback execute,
    //                            napi_async_complete_callback complete,
    //                            void* data,
    //                            napi_async_work* result);
    rules["napi_create_async_work"] = {
        {1, {6}},
        {2, {6}},
        {3, {6}},
        {4, {6}},
        {5, {6}}
    };


    // napi_status napi_create_array(napi_env env, napi_value* result)
    rules["napi_create_array"] = {
        {0, {1}}
    };

    // napi_status napi_create_array_with_length(napi_env env,
    //                                size_t length,
    //                                napi_value* result)
    rules["napi_create_array_with_length"] = {
        {1, {2}}
    };
    

    // napi_status napi_create_arraybuffer(napi_env env,
    //                             size_t byte_length,
    //                             void** data,
    //                             napi_value* result)
    rules["napi_create_arraybuffer"] = {
        {1, {2,3}}
    };  

    // napi_status napi_create_buffer(napi_env env,
    //                        size_t size,
    //                        void** data,
    //                        napi_value* result)
    rules["napi_create_buffer"] = {
        {1, {2,3}}
    };

    // napi_status napi_create_buffer_copy(napi_env env,
    //                             size_t length,
    //                             const void* data,
    //                             void** result_data,
    //                             napi_value* result)
    rules["napi_create_buffer_copy"] = {
        {1, {3,4}},
        {2, {3,4}}
    };
    
    // napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result)
    rules["napi_create_int32"] = {
        {1, {2}}
    };
    
    // napi_status napi_create_uint32(napi_env env, uint32_t value, napi_value* result)
    rules["napi_create_uint32"] = {
        {1, {2}}
    };

    // napi_status napi_create_int64(napi_env env, int64_t value, napi_value* result)
    rules["napi_create_int64"] = {
        {1, {2}}
    };
    
    // napi_status napi_create_double(napi_env env, double value, napi_value* result)
    rules["napi_create_double"] = {
        {1, {2}}
    };  

    // napi_status napi_create_bigint_int64(napi_env env,
    //                              int64_t value,
    //                              napi_value* result);
    rules["napi_create_bigint_int64"] = {
        {1, {2}}
    };
    
    // napi_status napi_create_bigint_uint64(napi_env env,
    //                               uint64_t value,
    //                               napi_value* result);
    rules["napi_create_bigint_uint64"] = {
        {1, {2}}
    };


    // napi_status napi_create_bigint_words(napi_env env,
    //                              int sign_bit,
    //                              size_t word_count,
    //                              const uint64_t* words,
    //                              napi_value* result);
    rules["napi_create_bigint_words"] = {
        {3, {4}}
    };


    // napi_status napi_create_string_latin1(napi_env env,
    //                               const char* str,
    //                               size_t length,
    //                               napi_value* result);
    rules["napi_create_string_latin1"] = {
        {1, {3}}
    };

    // napi_status node_api_create_external_string_latin1(napi_env env,
    //                                        char* str,
    //                                        size_t length,
    //                                        napi_finalize finalize_callback,
    //                                        void* finalize_hint,
    //                                        napi_value* result,
    //                                        bool* copied);
    rules["node_api_create_external_string_latin1"] = {
        {1, {5,6}},
        {3, {5,6}}
    };

    // napi_status napi_create_string_utf16(napi_env env,
    //                              const char16_t* str,
    //                              size_t length,
    //                              napi_value* result)
    rules["napi_create_string_utf16"] = {
        {1, {3}}
    };

    // napi_status node_api_create_external_string_utf16(napi_env env,
    //                                      char16_t* str,
    //                                      size_t length,
    //                                      napi_finalize finalize_callback,
    //                                      void* finalize_hint,
    //                                      napi_value* result,
    //                                      bool* copied);
    rules["node_api_create_external_string_utf16"] = {
        {1, {5,6}},
        {3, {5,6}}
    };
    
    
    // napi_status napi_create_string_utf8(napi_env env,
    //                             const char* str,
    //                             size_t length,
    //                             napi_value* result)
    rules["napi_create_string_utf8"] = {
        {1, {3}}
    };
    
    // napi_status NAPI_CDECL node_api_create_property_key_latin1(napi_env env,
    //                                                            const char* str,
    //                                                            size_t length,
    //                                                            napi_value* result);
    rules["node_api_create_property_key_latin1"] = {
        {1, {3}}
    };

    // napi_status NAPI_CDECL node_api_create_property_key_utf16(napi_env env,
    //                                                           const char16_t* str,
    //                                                           size_t length,
    //                                                           napi_value* result);
    rules["node_api_create_property_key_utf16"] = {
        {1, {3}}
    };

    // napi_status NAPI_CDECL node_api_create_property_key_utf8(napi_env env,
    //                                                          const char* str,
    //                                                          size_t length,
    //                                                          napi_value* result);
    rules["node_api_create_property_key_utf8"] = {
        {1, {3}}
    };

    // napi_status napi_get_array_length(napi_env env,
    //                                   napi_value value,
    //                                   uint32_t* result)
    rules["napi_get_array_length"] = {
        {1, {2}}
    };

    // napi_status napi_get_arraybuffer_info(napi_env env,
    //                                       napi_value arraybuffer,
    //                                       void** data,
    //                                       size_t* byte_length)
    rules["napi_get_arraybuffer_info"] = {
        {1, {2,3}}
    };
    
    
    // napi_status napi_get_buffer_info(napi_env env,
    //                                  napi_value value,
    //                                  void** data,
    //                                  size_t* length) 
    rules["napi_get_buffer_info"] = {
        {1, {2,3}}
    };
    
    
    // napi_status napi_get_prototype(napi_env env,
    //                                napi_value object,
    //                                napi_value* result)
    rules["napi_get_prototype"] = {
        {1, {2}}
    };
    
    
    // napi_status napi_get_typedarray_info(napi_env env,
    //                                  napi_value typedarray,
    //                                  napi_typedarray_type* type,
    //                                  size_t* length,
    //                                  void** data,
    //                                  napi_value* arraybuffer,
    //                                  size_t* byte_offset)
    rules["napi_get_typedarray_info"] = {
        {1, {2,3,4,5,6}}
    };


    // napi_status napi_get_dataview_info(napi_env env,
    //                                  napi_value dataview,
    //                                  size_t* byte_length,
    //                                  void** data,
    //                                  napi_value* arraybuffer,
    //                                  size_t* byte_offset)
    rules["napi_get_dataview_info"] = {
        {1, {2,3,4,5}}
    };

    // napi_status napi_get_date_value(napi_env env,
    //                                napi_value value,
    //                                double* result)
    rules["napi_get_date_value"] = {
        {1, {2}}
    };

    // napi_status napi_get_value_bool(napi_env env, napi_value value, bool* result)
    rules["napi_get_value_bool"] = {
        {1, {2}}
    };

    // napi_status napi_get_value_double(napi_env env,
    //                                  napi_value value,
    //                                  double* result)
    rules["napi_get_value_double"] = {
        {1, {2}}
    };

    // napi_status napi_get_value_bigint_int64(napi_env env,
    //                                         napi_value value,
    //                                         int64_t* result,
    //                                         bool* lossless);
    rules["napi_get_value_bigint_int64"] = {
        {1, {2,3}}
    };


    // napi_status napi_get_value_bigint_uint64(napi_env env,
    //                                         napi_value value,
    //                                         uint64_t* result,
    //                                         bool* lossless);
    rules["napi_get_value_bigint_uint64"] = {
        {1, {2,3}}
    };

    // napi_status napi_get_value_bigint_words(napi_env env,
    //                                         napi_value value,
    //                                         int* sign_bit,
    //                                         size_t* word_count,
    //                                         uint64_t* words);
    rules["napi_get_value_bigint_words"] = {
        {1, {2,3,4}}
    };


    // napi_status napi_get_value_external(napi_env env,
    //                                     napi_value value,
    //                                     void** result)
    rules["napi_get_value_external"] = {
        {1, {2}}
    };

    // napi_status napi_get_value_int32(napi_env env,
    //                                 napi_value value,
    //                                 int32_t* result)
    rules["napi_get_value_int32"] = {
        {1, {2}}
    };

    
    // napi_status napi_get_value_int64(napi_env env,
    //                                 napi_value value,
    //                                 int64_t* result)
    rules["napi_get_value_int64"] = {
        {1, {2}}
    };

    // napi_status napi_get_value_string_latin1(napi_env env,
    //                                         napi_value value,
    //                                         char* buf,
    //                                         size_t bufsize,
    //                                         size_t* result)
    rules["napi_get_value_string_latin1"] = {
        {1, {2,3,4}}
    };


    // napi_status napi_get_value_string_utf8(napi_env env,
    //                                       napi_value value,
    //                                       char* buf,
    //                                       size_t bufsize,
    //                                       size_t* result)
    rules["napi_get_value_string_utf8"] = {
        {1, {2,3,4}}
    };

    // napi_status napi_get_value_string_utf16(napi_env env,
    //                                         napi_value value,
    //                                         char16_t* buf,
    //                                         size_t bufsize,
    //                                         size_t* result)
    rules["napi_get_value_string_utf16"] = {
        {1, {2,3,4}}
    };

    // napi_status napi_get_value_uint32(napi_env env,
    //                                  napi_value value,
    //                                  uint32_t* result)
    rules["napi_get_value_uint32"] = {
        {1, {2}}
    };

    // napi_status napi_get_boolean(napi_env env, bool value, napi_value* result)
    rules["napi_get_boolean"] = {
        {1, {2}}
    };

    // napi_status napi_get_global(napi_env env, napi_value* result)
    rules["napi_get_global"] = {
        {1, {2}}
    };
    
    // napi_status napi_get_null(napi_env env, napi_value* result)
    rules["napi_get_null"] = {
        {0, {1}}
    };
    
    //NAPI_EXTERN napi_status napi_get_property(napi_env env,
    //                                          napi_value object,
    //                                          napi_value key,
    //                                          napi_value* result)
    rules["napi_get_property"] = {
        {1, {3}}
    };
    //NAPI_EXTERN napi_status napi_delete_property(napi_env env,
    //                                             napi_value object,
    //                                             napi_value key,
    //                                             bool* result);
    rules["napi_delete_property"] = {
        {1, {3}}
    };

    //NAPI_EXTERN napi_status napi_set_named_property(napi_env env,
    //                                          napi_value object,
    //                                          const char* utf8name,
    //                                          napi_value value);
    rules["napi_set_named_property"] = {
        {3, {1}}
    };

    
    //NAPI_EXTERN napi_status napi_has_named_property(napi_env env,
    //                                          napi_value object,
    //                                          const char* utf8name,
    //                                          bool* result);
    rules["napi_has_named_property"] = {
        {1, {3}}
    };

    //NAPI_EXTERN napi_status napi_get_named_property(napi_env env,
    //                                          napi_value object,
    //                                          const char* utf8name,
    //                                          napi_value* result);
    rules["napi_get_named_property"] = {
        {1, {3}}
    };
    
    // napi_status napi_get_property_names(napi_env env,
    //                                     napi_value object,
    //                                     napi_value* result);
    rules["napi_get_property_names"] = {
        {1, {2}}
    };

    //NAPI_EXTERN napi_status napi_set_property(napi_env env,
    //                                          napi_value object,
    //                                          napi_value key,
    //                                          napi_value value);


    rules["napi_set_property"] = {
        {3, {1}}
    };

    //NAPI_EXTERN napi_status napi_has_property(napi_env env,
    //                                          napi_value object,
    //                                          napi_value key,
    //                                          bool* result);
    rules["napi_has_property"] = {
        {1, {3}}
    };



    // napi_status napi_get_all_property_names(napi_env env,
    //                             napi_value object,
    //                             napi_key_collection_mode key_mode,
    //                             napi_key_filter key_filter,
    //                             napi_key_conversion key_conversion,
    //                             napi_value* result);
    rules["napi_get_all_property_names"] = {
        {1,{5}},
        {2,{5}},
        {3,{5}},
        {4,{5}}
    };

    

    // napi_status napi_coerce_to_bool(napi_env env,
    //                                 napi_value value,
    //                                 napi_value* result)
    rules["napi_coerce_to_bool"] = {
        {1, {2}}
    };

    // napi_status napi_coerce_to_number(napi_env env,
    //                                 napi_value value,
    //                                 napi_value* result)
    rules["napi_coerce_to_number"] = {
        {1, {2}}
    };

    // napi_status napi_coerce_to_object(napi_env env,
    //                                 napi_value value,
    //                                 napi_value* result)
    rules["napi_coerce_to_object"] = {
        {1, {2}}
    };

    // napi_status napi_coerce_to_string(napi_env env,
    //                                 napi_value value,
    //                                 napi_value* result)
    rules["napi_coerce_to_string"] = {
        {1, {2}}
    };
    
    // napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
    rules["napi_typeof"] = {
        {1, {2}}
    };

    // napi_status napi_instanceof(napi_env env,
    //                             napi_value object,
    //                             napi_value constructor,
    //                             bool* result)
    rules["napi_instanceof"] = {
        {1, {3}},
        {2, {3}}
    };

    // napi_status napi_is_array(napi_env env, napi_value value, bool* result)
    rules["napi_is_array"] = {
        {1, {2}}
    };

    // napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result)
    rules["napi_is_arraybuffer"] = {
        {1, {2}}
    };

    // napi_status napi_is_buffer(napi_env env, napi_value value, bool* result)
    rules["napi_is_buffer"] = {
        {1, {2}}
    };
    
    // napi_status napi_is_date(napi_env env, napi_value value, bool* result)
    rules["napi_is_date"] = {
        {1, {2}}
    };

    // napi_status napi_is_error(napi_env env, napi_value value, bool* result)
    rules["napi_is_error"] = {
        {1, {2}}
    };

    // napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result)
    rules["napi_is_typedarray"] = {
        {1, {2}}
    };
    
    // napi_status napi_is_dataview(napi_env env, napi_value value, bool* result)
    rules["napi_is_dataview"] = {
        {1, {2}}
    };

    // napi_status napi_strict_equals(napi_env env,
    //                               napi_value lhs,
    //                               napi_value rhs,
    //                               bool* result)
    rules["napi_strict_equals"] = {
        {1, {3}},
        {2, {3}}
    };

    // napi_status napi_is_detached_arraybuffer(napi_env env,
    //                                          napi_value arraybuffer,
    //                                          bool* result)
    rules["napi_is_detached_arraybuffer"] = {
        {1, {2}}
    };

    // napi_status napi_get_element(napi_env env, napi_value object, uint32_t index, napi_value* result)
    rules["napi_get_element"] = {
        {1, {3}}
    };

//NAPI_EXTERN napi_status napi_set_element(napi_env env,
//                                         napi_value object,
//                                         uint32_t index,
//                                         napi_value value);
    rules["napi_set_element"] = {
        {1, {3}}
    };

//NAPI_EXTERN napi_status napi_has_element(napi_env env,
//                                         napi_value object,
//                                         uint32_t index,
//                                         bool* result);
    rules["napi_has_element"] = {
        {1, {3}}
    };

//NAPI_EXTERN napi_status napi_get_element(napi_env env,
//                                         napi_value object,
//                                         uint32_t index,
//                                         napi_value* result);
    rules["napi_get_element"] = {
        {1, {3}}
    };

//NAPI_EXTERN napi_status napi_delete_element(napi_env env,
//                                            napi_value object,
//                                            uint32_t index,
//                                            bool* result);
    rules["napi_delete_element"] = {
        {1, {3}}
    };

//NAPI_EXTERN napi_status napi_create_dataview(napi_env env,
//                                             size_t length,
//                                             napi_value arraybuffer,
//                                             size_t byte_offset,
//                                             napi_value* result);
    rules["napi_create_dataview"] = {
        {2,{5}}
    };
    
//NAPI_EXTERN napi_status napi_create_typedarray(napi_env env,
//                                               napi_typedarray_type type,
//                                               size_t length,
//                                               napi_value arraybuffer,
//                                               size_t byte_offset,
//                                               napi_value* result);
    rules["napi_create_typedarray"] = {
        {3,{5}}
    };


//NAPI_EXTERN napi_status napi_create_external_buffer(napi_env env,
//                                                    size_t length,
//                                                    void* data,
//                                                    napi_finalize finalize_cb,
//                                                    void* finalize_hint, 
//                                                    napi_value* result);
    rules["napi_create_external_buffer"] = {
        {2,{5}}
    };


// napi_status napi_create_date(napi_env env, double time, napi_value* result);
// napi_status napi_get_date_value(napi_env env, napi_value value, double* result)
// napi_status napi_is_date(napi_env env, napi_value value, bool* result)
    rules["napi_create_date"] = {
        {1, {2}}
    };

    rules["napi_get_date_value"] = {
        {1, {2}}
    };

    rules["napi_is_date"] = {
        {1, {2}}
    };
}

