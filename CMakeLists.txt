#############################################################################
# Copyright (c) 2010-2012, Johannes Asal
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# - The name of the author may not be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS  FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
# CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT  LIMITED  TO,  PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS; OR BUSINESS
# INTERRUPTION)  HOWEVER  CAUSED AND ON ANY THEORY OF LIABILITY,  WHETHER  IN
# CONTRACT,  STRICT  LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

project(CCONFIG)
cmake_minimum_required(VERSION 2.4)

if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(src_dir ${CMAKE_SOURCE_DIR}/src)
set(test_dir ${CMAKE_SOURCE_DIR}/test)
include_directories(${src_dir})

#######################################################################################
## Set compiler flags

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

message("Build type ${CMAKE_BUILD_TYPE}")

message("Using CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")
message("Using CMAKE_CC_COMPILER = ${CMAKE_CC_COMPILER}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message("Using CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
else(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message("Using CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE}")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

#######################################################################################
## Find boost libraries

find_package(Boost COMPONENTS program_options REQUIRED)

#######################################################################################
## Rules for generating parser from grammar

set(gen_dir ${CMAKE_SOURCE_DIR}/gen)
set(antlr_dir ${CMAKE_SOURCE_DIR}/antlr)
set(config_grammar ${CMAKE_SOURCE_DIR}/grammar/Config.g)
set(schema_grammar ${CMAKE_SOURCE_DIR}/grammar/ConfigSchema.g)

set_source_files_properties(
	${gen_dir}/ConfigParser.hpp
	${gen_dir}/ConfigParser.cpp
	${gen_dir}/ConfigLexer.hpp
	${gen_dir}/ConfigLexer.cpp
	${gen_dir}/ConfigSchemaParser.hpp
	${gen_dir}/ConfigSchemaParser.cpp
	${gen_dir}/ConfigSchemaLexer.hpp
	${gen_dir}/ConfigSchemaLexer.cpp
	PROPERTIES GENERATED true)

add_custom_command(
	OUTPUT ${gen_dir}/ConfigParser.hpp ${gen_dir}/ConfigParser.cpp
		${gen_dir}/ConfigLexer.hpp ${gen_dir}/ConfigLexer.cpp
	COMMAND mkdir -p ${gen_dir}
	COMMAND java -jar ${antlr_dir}/antlr-3.4-with-cpp.jar
		-o ${gen_dir} ${config_grammar}
	DEPENDS ${config_grammar}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_command(
	OUTPUT ${gen_dir}/ConfigSchemaParser.hpp ${gen_dir}/ConfigSchemaParser.cpp
		${gen_dir}/ConfigSchemaLexer.hpp ${gen_dir}/ConfigSchemaLexer.cpp
	COMMAND mkdir -p ${gen_dir}
	COMMAND java -jar ${antlr_dir}/antlr-3.4-with-cpp.jar
		-o ${gen_dir} ${schema_grammar}
	DEPENDS ${schema_grammar}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target(generated_parser
	DEPENDS
	${gen_dir}/ConfigParser.cpp
	${gen_dir}/ConfigSchemaParser.cpp
)

include_directories(${antlr_dir}/include)
include_directories(${gen_dir})

################################################################################################
## Create library

add_library(cconfig STATIC
	${src_dir}/config_schema.cpp
	${gen_dir}/ConfigParser.cpp
	${gen_dir}/ConfigLexer.cpp
	${gen_dir}/ConfigSchemaParser.cpp
	${gen_dir}/ConfigSchemaLexer.cpp
)
add_dependencies(cconfig generated_parser)

################################################################################################
## Create generator tools

add_executable(cconfig_code_gen ${src_dir}/cconfig_code_gen.cpp)
target_link_libraries(cconfig_code_gen cconfig ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_executable(cconfig_stub_gen ${src_dir}/cconfig_stub_gen.cpp)
target_link_libraries(cconfig_stub_gen cconfig ${Boost_PROGRAM_OPTIONS_LIBRARY})

################################################################################################
## Create test program

add_executable(test ${test_dir}/test.cpp)
target_link_libraries(test cconfig)

add_executable(test_schema ${test_dir}/test_schema.cpp)
target_link_libraries(test_schema cconfig)
