##
## Licensed to the Apache Software Foundation (ASF) under one
## or more contributor license agreements.  See the NOTICE file
## distributed with this work for additional information
## regarding copyright ownership.  The ASF licenses this file
## to you under the Apache License, Version 2.0 (the
## "License"); you may not use this file except in compliance
## with the License.  You may obtain a copy of the License at
## 
##   http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing,
## software distributed under the License is distributed on an
## "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
## KIND, either express or implied.  See the License for the
## specific language governing permissions and limitations
## under the License.
##

cmake_minimum_required(VERSION 2.6)
set(CMAKE_C_FLAGS "-pthread -Wall -Werror")

project(dispatch-tools C)
find_library(proton_lib qpid-proton)
find_path(proton_include proton/driver.h)

find_library(dispatch_lib qpid-dispatch)
find_path(dispatch_include qpid/dispatch/server.h)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${proton_include}
    ${dispatch_include}
    )

##
## Build the applications
##
set(send_SOURCES
    src/tool.c
    src/sender.c
    )

add_executable(send ${send_SOURCES})
target_link_libraries(send ${proton_lib} ${dispatch_lib})

set(recv_SOURCES
    src/tool.c
    src/receiver.c
    )

add_executable(recv ${recv_SOURCES})
target_link_libraries(recv ${proton_lib} ${dispatch_lib})

