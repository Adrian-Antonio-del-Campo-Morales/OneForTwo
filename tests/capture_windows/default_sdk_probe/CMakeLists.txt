cmake_minimum_required(VERSION 3.24)

project(OneInTwoDefaultSdkProbe LANGUAGES CXX)

message(STATUS
    "OneInTwo selected Windows SDK: '${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}'")
