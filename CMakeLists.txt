cmake_minimum_required(VERSION 3.21)

project(spv2headerconv)

add_executable(spv2headerconv SpvToHeaderConverter.cpp)
set_target_properties(spv2headerconv PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_features(spv2headerconv PUBLIC cxx_std_20)
target_include_directories(spv2headerconv PRIVATE Headers/spdlog/)

#install(TARGETS spv2headerconv RUNTIME DESTINATION bin)

