# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sergey Zamaro

cmake_minimum_required(VERSION 3.12)

project(dependency_injector LANGUAGES C CXX)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_library(dependency_injector SHARED)

target_compile_options(dependency_injector PRIVATE -Wall -Wextra -pedantic -std=c++26 -freflection)

target_sources(dependency_injector PRIVATE ${SOURCES})