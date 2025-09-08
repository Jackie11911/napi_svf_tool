include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include
)

file(GLOB_RECURSE SRC_FILES 
    "svf-ex.cpp" 
    "napi/utils/*.cpp" 
    "napi/*.cpp" 
    "taintanalysis/*.cpp" 
    "JsonExporter/*.cpp" 
    "SourceAndSinks/*.cpp"
    "projectParser/*.cpp"
)

add_executable(napi_svf_tool ${SRC_FILES})

target_link_libraries(napi_svf_tool ${llvm_libs} ${SVF_LIB})
