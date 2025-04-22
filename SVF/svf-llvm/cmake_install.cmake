# Install script for directory: /home/jackie/project/SVF/svf-llvm

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "SvfLLVM" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/jackie/project/SVF/lib/libSvfLLVM.a")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jackie/project/SVF/svf-llvm/tools/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/lib/libSvfLLVM.a")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/lib" TYPE STATIC_LIBRARY FILES "/home/jackie/project/SVF/lib/libSvfLLVM.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/include/SVF-LLVM/BasicTypes.h;/usr/local/include/SVF-LLVM/BreakConstantExpr.h;/usr/local/include/SVF-LLVM/CHGBuilder.h;/usr/local/include/SVF-LLVM/CppUtil.h;/usr/local/include/SVF-LLVM/DCHG.h;/usr/local/include/SVF-LLVM/GEPTypeBridgeIterator.h;/usr/local/include/SVF-LLVM/ICFGBuilder.h;/usr/local/include/SVF-LLVM/LLVMLoopAnalysis.h;/usr/local/include/SVF-LLVM/LLVMModule.h;/usr/local/include/SVF-LLVM/LLVMUtil.h;/usr/local/include/SVF-LLVM/ObjTypeInference.h;/usr/local/include/SVF-LLVM/SVFIRBuilder.h;/usr/local/include/SVF-LLVM/SymbolTableBuilder.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/include/SVF-LLVM" TYPE FILE FILES
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/BasicTypes.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/BreakConstantExpr.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/CHGBuilder.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/CppUtil.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/DCHG.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/GEPTypeBridgeIterator.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/ICFGBuilder.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/LLVMLoopAnalysis.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/LLVMModule.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/LLVMUtil.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/ObjTypeInference.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/SVFIRBuilder.h"
    "/home/jackie/project/SVF/svf-llvm/include/SVF-LLVM/SymbolTableBuilder.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/lib/extapi.bc")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/lib" TYPE FILE FILES "/home/jackie/project/SVF/lib/extapi.bc")
endif()

