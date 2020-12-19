# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

if (NOT DEFINED FROM_WHO)
  set(FROM_WHO Zephyr)
endif()

target_compile_definitions(app PRIVATE "-DMCUBOOT_BLINKY2_FROM=\"${FROM_WHO}\"")

target_sources(app 
  PRIVATE 
    src/main.c
    src/detools/detools.c
    src/heatshrink/heatshrink_decoder.c)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)