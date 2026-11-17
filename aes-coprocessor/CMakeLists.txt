# Copyright 2026 James Sargent <acheton1984@letterboxes.org>
#
# This file is part of AESulator, a plugin for Beebium.
#
# AESulator is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version. AESulator is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
# You should have received a copy of the GNU General Public License along with AESulator.
# If not, see <https://www.gnu.org/licenses/>.

# AESulator: the Acorn ARM Evaluation System, a Beebium plugin.
# CMakeLists.txt, builds the plugin inside Beebium's tree
# version 1.0.0, 17 November 2026.

# The AES plugin, built inside a Beebium working copy (DOC, Beebium guide section 7).
# A copy of aes-coprocessor/ lands in src/extensions/aes-coprocessor: see tools/plugin-build.sh.
set(AES_SOURCES
    ArmCore.cpp
    AesMemory.cpp
    AesOptions.cpp
    AesTiming.cpp
    beebium/AesTube.cpp
    beebium/AesExtension.cpp
)
add_library(beebium_ext_aes STATIC ${AES_SOURCES})
target_include_directories(beebium_ext_aes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(beebium_ext_aes PUBLIC beebium_core)
set_target_properties(beebium_ext_aes PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BEEBIUM_BUILD_SERVICE)
    add_library(beebium_ext_aes_plugin SHARED plugin_entry.cpp)
    target_link_libraries(beebium_ext_aes_plugin PRIVATE beebium_ext_aes beebium_extension_api)
    set_target_properties(beebium_ext_aes_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "aes-coprocessor"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    beebium_finalize_plugin(TARGET beebium_ext_aes_plugin NAME aes-coprocessor)
endif()

# The counting wrapper, a test build only (USER, 2 Nov 2026): the same plugin with
# beebium/TubeLog.hpp compiled in, kept out of the server's own extensions folder and reached
# with --extension-dir, which overrides a matching cli name (DOC, Beebium cli.md).
if(BEEBIUM_BUILD_SERVICE AND BEEBIUM_BUILD_TESTS)
    set(AES_LOG_DIR ${CMAKE_CURRENT_BINARY_DIR}/tube-log/aes-coprocessor)
    add_library(beebium_ext_aes_log_plugin SHARED
        ${AES_SOURCES}
        plugin_entry.cpp
        ${CMAKE_SOURCE_DIR}/src/core/extension-api/PluginAbiVersion.cpp
    )
    target_compile_definitions(beebium_ext_aes_log_plugin PRIVATE AES_TUBE_LOG)
    target_include_directories(beebium_ext_aes_log_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(beebium_ext_aes_log_plugin PRIVATE beebium_core beebium_extension_api)
    set_target_properties(beebium_ext_aes_log_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "aes-coprocessor"
        LIBRARY_OUTPUT_DIRECTORY ${AES_LOG_DIR}
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    configure_file(manifest.json ${AES_LOG_DIR}/manifest.json COPYONLY)
endif()

if(BEEBIUM_BUILD_TESTS)
    add_executable(test_aes_machine tests/machine_test.cpp)
    target_link_libraries(test_aes_machine PRIVATE beebium_ext_aes Catch2::Catch2WithMain)
    target_compile_definitions(test_aes_machine PRIVATE
        BEEBIUM_ROM_DIR="${BEEBIUM_ROM_DIR}"
        AES_ROM_FACTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/rom-facts")
    catch_discover_tests(test_aes_machine)
endif()
