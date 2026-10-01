cmake_minimum_required(VERSION 3.20)
project(tetrisphere_m1 LANGUAGES C CXX)

# Packaged Linux executables use only an origin-relative runtime path.
# Suppress CMake's automatic build-tree paths for every other target.
if(UNIX AND NOT APPLE)
    set(CMAKE_SKIP_RPATH FALSE)
    set(CMAKE_SKIP_BUILD_RPATH TRUE)
else()
    set(CMAKE_SKIP_RPATH TRUE)
endif()

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TETRISPHERE_GENERATED_DIR "" CACHE PATH "N64Recomp-generated function directory")
set(TETRISPHERE_GENERATION_MANIFEST "" CACHE FILEPATH "Sanitized Task 4 generation manifest")
set(TETRISPHERE_ROM "" CACHE FILEPATH "Private validated NTPE rev0 ROM for runtime tests")
set(RT64_ROOT "${CMAKE_SOURCE_DIR}/.local/deps/rt64"
    CACHE PATH "Exact locked RT64 checkout")
option(TETRISPHERE_RT64_PROBE "Copy and instrument pinned RT64 sources for private diagnostics" OFF)
set(TETRISPHERE_RSP_RECOMP_SOURCE "" CACHE FILEPATH
    "Private RSPRecomp output for the captured audio microcode")
set(TETRISPHERE_GRAPHICS_SNAPSHOT "" CACHE FILEPATH
    "Private 8 MiB graphics-task RDRAM capture")
set(TETRISPHERE_GRAPHICS_COMMANDS "" CACHE FILEPATH
    "Private captured graphics display list")
set(TETRISPHERE_AUDIO_SNAPSHOT "" CACHE FILEPATH
    "Private 8 MiB audio-task RDRAM capture")
set(N64MODERN_RUNTIME_ROOT "${CMAKE_SOURCE_DIR}/.local/deps/N64ModernRuntime"
    CACHE PATH "Exact locked N64ModernRuntime checkout")
if(MSVC)
    add_compile_options(
        "/experimental:deterministic"
        "/pathmap:${CMAKE_SOURCE_DIR}=."
        "/pathmap:${N64MODERN_RUNTIME_ROOT}=deps/N64ModernRuntime"
        "/pathmap:${RT64_ROOT}=deps/rt64")
else()
    add_compile_options(
        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
        "-ffile-prefix-map=${N64MODERN_RUNTIME_ROOT}=deps/N64ModernRuntime"
        "-ffile-prefix-map=${RT64_ROOT}=deps/rt64")
endif()
if(NOT TETRISPHERE_GENERATED_DIR OR NOT EXISTS "${TETRISPHERE_GENERATED_DIR}/funcs.h")
    message(FATAL_ERROR "Generate Task 4 output and set TETRISPHERE_GENERATED_DIR to its functions directory")
endif()
if(NOT TETRISPHERE_GENERATION_MANIFEST OR NOT EXISTS "${TETRISPHERE_GENERATION_MANIFEST}")
    message(FATAL_ERROR "Set TETRISPHERE_GENERATION_MANIFEST to the matching generated manifest")
endif()

file(READ "${TETRISPHERE_GENERATION_MANIFEST}" GENERATION_MANIFEST_JSON)
string(JSON GENERATION_SCHEMA ERROR_VARIABLE GENERATION_JSON_ERROR
       GET "${GENERATION_MANIFEST_JSON}" schema_version)
if(GENERATION_JSON_ERROR OR NOT GENERATION_SCHEMA EQUAL 1)
    message(FATAL_ERROR "Unsupported generation manifest schema: ${GENERATION_JSON_ERROR}")
endif()
string(JSON TETRISPHERE_GENERATION_ID GET "${GENERATION_MANIFEST_JSON}" generation_id)
string(JSON MANIFEST_RECOMPILER_COMMIT GET "${GENERATION_MANIFEST_JSON}" recompiler commit)

set(TETRISPHERE_DEPENDENCY_LOCK "${CMAKE_SOURCE_DIR}/config/dependencies-m1.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${TETRISPHERE_DEPENDENCY_LOCK}")
file(READ "${TETRISPHERE_DEPENDENCY_LOCK}" DEPENDENCY_LOCK_JSON)
file(SHA256 "${TETRISPHERE_DEPENDENCY_LOCK}" TETRISPHERE_DEPENDENCY_LOCK_ID)
string(JSON DEPENDENCY_COUNT LENGTH "${DEPENDENCY_LOCK_JSON}" dependencies)
math(EXPR DEPENDENCY_LAST "${DEPENDENCY_COUNT} - 1")
set(LOCKED_N64RECOMP_COMMIT "")
set(LOCKED_N64MODERNRUNTIME_COMMIT "")
set(LOCKED_RT64_COMMIT "")
foreach(INDEX RANGE 0 ${DEPENDENCY_LAST})
    string(JSON DEPENDENCY_NAME GET "${DEPENDENCY_LOCK_JSON}" dependencies ${INDEX} name)
    if(DEPENDENCY_NAME STREQUAL "N64Recomp")
        string(JSON LOCKED_N64RECOMP_COMMIT GET "${DEPENDENCY_LOCK_JSON}" dependencies ${INDEX} commit)
    elseif(DEPENDENCY_NAME STREQUAL "N64ModernRuntime")
        string(JSON LOCKED_N64MODERNRUNTIME_COMMIT GET "${DEPENDENCY_LOCK_JSON}" dependencies ${INDEX} commit)
    elseif(DEPENDENCY_NAME STREQUAL "rt64")
        string(JSON LOCKED_RT64_COMMIT GET "${DEPENDENCY_LOCK_JSON}" dependencies ${INDEX} commit)
    endif()
endforeach()
if(NOT LOCKED_N64RECOMP_COMMIT OR
   NOT MANIFEST_RECOMPILER_COMMIT STREQUAL LOCKED_N64RECOMP_COMMIT)
    message(FATAL_ERROR "Generation manifest N64Recomp commit does not match dependency lock")
endif()
if(NOT LOCKED_N64MODERNRUNTIME_COMMIT)
    message(FATAL_ERROR "N64ModernRuntime is absent from dependency lock")
endif()
if(NOT LOCKED_RT64_COMMIT)
    message(FATAL_ERROR "RT64 is absent from dependency lock")
endif()

string(JSON GENERATED_FILE_COUNT LENGTH "${GENERATION_MANIFEST_JSON}" generated_files)
if(GENERATED_FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "Generation manifest has no generated files")
endif()
math(EXPR GENERATED_FILE_LAST "${GENERATED_FILE_COUNT} - 1")
set(MANIFEST_GENERATED_PATHS "")
set(MANIFEST_RECOMP_SOURCES "")
set(GENERATED_HASH_MATERIAL "")
set(FUNCS_HEADER_COUNT 0)
foreach(INDEX RANGE 0 ${GENERATED_FILE_LAST})
    string(JSON GENERATED_PATH GET "${GENERATION_MANIFEST_JSON}" generated_files ${INDEX} path)
    string(JSON EXPECTED_HASH GET "${GENERATION_MANIFEST_JSON}" generated_files ${INDEX} sha256)
    if(IS_ABSOLUTE "${GENERATED_PATH}" OR GENERATED_PATH MATCHES "(^|/)\\.\\.(/|$)" OR
       GENERATED_PATH MATCHES "\\\\")
        message(FATAL_ERROR "Unsafe generated path in manifest: ${GENERATED_PATH}")
    endif()
    list(FIND MANIFEST_GENERATED_PATHS "${GENERATED_PATH}" DUPLICATE_INDEX)
    if(NOT DUPLICATE_INDEX EQUAL -1)
        message(FATAL_ERROR "Duplicate generated path in manifest: ${GENERATED_PATH}")
    endif()
    list(APPEND MANIFEST_GENERATED_PATHS "${GENERATED_PATH}")
    if(GENERATED_PATH STREQUAL "funcs.h")
        math(EXPR FUNCS_HEADER_COUNT "${FUNCS_HEADER_COUNT} + 1")
    endif()
    set(GENERATED_FILE "${TETRISPHERE_GENERATED_DIR}/${GENERATED_PATH}")
    if(NOT EXISTS "${GENERATED_FILE}")
        message(FATAL_ERROR "Generated file missing: ${GENERATED_PATH}")
    endif()
    file(SHA256 "${GENERATED_FILE}" ACTUAL_HASH)
    if(NOT ACTUAL_HASH STREQUAL EXPECTED_HASH)
        message(FATAL_ERROR "generated file hash mismatch: ${GENERATED_PATH}")
    endif()
    string(APPEND GENERATED_HASH_MATERIAL ":${GENERATED_PATH}:${ACTUAL_HASH}")
    if(GENERATED_PATH MATCHES "^funcs_[0-9]+\\.c$")
        list(APPEND MANIFEST_RECOMP_SOURCES "${GENERATED_PATH}")
    endif()
endforeach()
if(NOT FUNCS_HEADER_COUNT EQUAL 1)
    message(FATAL_ERROR "Generation manifest must contain funcs.h exactly once")
endif()
file(GLOB_RECURSE ACTUAL_GENERATED_PATHS CONFIGURE_DEPENDS
     LIST_DIRECTORIES false RELATIVE "${TETRISPHERE_GENERATED_DIR}"
     "${TETRISPHERE_GENERATED_DIR}/*")
list(SORT ACTUAL_GENERATED_PATHS)
list(SORT MANIFEST_GENERATED_PATHS)
if(NOT ACTUAL_GENERATED_PATHS STREQUAL MANIFEST_GENERATED_PATHS)
    message(FATAL_ERROR "Generated artifact list does not exactly match generation manifest")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(N64MODERN_RUNTIME "${N64MODERN_RUNTIME_ROOT}")
set(N64MODERN_RUNTIME_RECEIPT
    "${CMAKE_BINARY_DIR}/n64modernruntime-verification.json")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build/verify_runtime.py"
            --root "${N64MODERN_RUNTIME}"
            --lock "${TETRISPHERE_DEPENDENCY_LOCK}"
            --receipt "${N64MODERN_RUNTIME_RECEIPT}"
    RESULT_VARIABLE N64MODERN_RUNTIME_VERIFY_RESULT
    OUTPUT_VARIABLE N64MODERN_RUNTIME_VERIFY_STDOUT
    ERROR_VARIABLE N64MODERN_RUNTIME_VERIFY_STDERR
)
if(NOT N64MODERN_RUNTIME_VERIFY_RESULT EQUAL 0)
    message(FATAL_ERROR
        "N64ModernRuntime checkout verification failed: "
        "${N64MODERN_RUNTIME_VERIFY_STDOUT}${N64MODERN_RUNTIME_VERIFY_STDERR}")
endif()
file(READ "${N64MODERN_RUNTIME_RECEIPT}" N64MODERN_RUNTIME_RECEIPT_JSON)
string(JSON N64MODERN_RUNTIME_VERIFIED_COMMIT GET
       "${N64MODERN_RUNTIME_RECEIPT_JSON}" commit)
string(JSON N64MODERN_RUNTIME_SOURCE_ID GET
       "${N64MODERN_RUNTIME_RECEIPT_JSON}" source_identity)
string(JSON N64MODERN_RUNTIME_LOCK_ID GET
       "${N64MODERN_RUNTIME_RECEIPT_JSON}" lock_digest)
if(NOT N64MODERN_RUNTIME_VERIFIED_COMMIT STREQUAL LOCKED_N64MODERNRUNTIME_COMMIT)
    message(FATAL_ERROR "N64ModernRuntime verification receipt does not match dependency lock")
endif()
if(NOT N64MODERN_RUNTIME_LOCK_ID STREQUAL TETRISPHERE_DEPENDENCY_LOCK_ID)
    message(FATAL_ERROR "N64ModernRuntime verification receipt has stale dependency lock")
endif()
add_custom_target(verify_n64modernruntime ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build/verify_runtime.py"
            --root "${N64MODERN_RUNTIME}"
            --lock "${TETRISPHERE_DEPENDENCY_LOCK}"
            --receipt "${N64MODERN_RUNTIME_RECEIPT}"
            --expected-commit "${N64MODERN_RUNTIME_VERIFIED_COMMIT}"
            --expected-source-identity "${N64MODERN_RUNTIME_SOURCE_ID}"
            --expected-lock-digest "${N64MODERN_RUNTIME_LOCK_ID}"
    COMMENT "Verifying exact clean N64ModernRuntime checkout"
    VERBATIM
)

set(RT64_RECEIPT "${CMAKE_BINARY_DIR}/rt64-verification.json")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build/verify_runtime.py"
            --name rt64 --root "${RT64_ROOT}"
            --lock "${TETRISPHERE_DEPENDENCY_LOCK}"
            --receipt "${RT64_RECEIPT}"
    RESULT_VARIABLE RT64_VERIFY_RESULT
    OUTPUT_VARIABLE RT64_VERIFY_STDOUT
    ERROR_VARIABLE RT64_VERIFY_STDERR)
if(NOT RT64_VERIFY_RESULT EQUAL 0)
    message(FATAL_ERROR "RT64 checkout verification failed: "
        "${RT64_VERIFY_STDOUT}${RT64_VERIFY_STDERR}")
endif()
file(READ "${RT64_RECEIPT}" RT64_RECEIPT_JSON)
string(JSON RT64_VERIFIED_COMMIT GET "${RT64_RECEIPT_JSON}" commit)
string(JSON RT64_SOURCE_ID GET "${RT64_RECEIPT_JSON}" source_identity)
string(JSON RT64_LOCK_ID GET "${RT64_RECEIPT_JSON}" lock_digest)
if(NOT RT64_VERIFIED_COMMIT STREQUAL LOCKED_RT64_COMMIT OR
   NOT RT64_LOCK_ID STREQUAL TETRISPHERE_DEPENDENCY_LOCK_ID)
    message(FATAL_ERROR "RT64 verification receipt does not match dependency lock")
endif()
add_custom_target(verify_rt64 ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build/verify_runtime.py"
            --name rt64 --root "${RT64_ROOT}"
            --lock "${TETRISPHERE_DEPENDENCY_LOCK}"
            --receipt "${RT64_RECEIPT}"
            --expected-commit "${RT64_VERIFIED_COMMIT}"
            --expected-source-identity "${RT64_SOURCE_ID}"
            --expected-lock-digest "${RT64_LOCK_ID}"
    COMMENT "Verifying exact clean RT64 checkout"
    VERBATIM)

set(N64RECOMP_INCLUDE "${N64MODERN_RUNTIME}/N64Recomp/include")
if(NOT EXISTS "${N64RECOMP_INCLUDE}/recomp.h")
    message(FATAL_ERROR "Pinned N64Recomp headers are missing; run tools/build/fetch_dependencies.py")
endif()

file(GLOB GENERATED_RECOMP_SOURCES_RELATIVE CONFIGURE_DEPENDS
     RELATIVE "${TETRISPHERE_GENERATED_DIR}" "${TETRISPHERE_GENERATED_DIR}/funcs_*.c")
list(SORT GENERATED_RECOMP_SOURCES_RELATIVE)
list(SORT MANIFEST_RECOMP_SOURCES)
if(NOT GENERATED_RECOMP_SOURCES_RELATIVE STREQUAL MANIFEST_RECOMP_SOURCES)
    message(FATAL_ERROR "Compiled generated source list does not match generation manifest")
endif()
set(GENERATED_RECOMP_SOURCES "")
foreach(GENERATED_SOURCE IN LISTS GENERATED_RECOMP_SOURCES_RELATIVE)
    list(APPEND GENERATED_RECOMP_SOURCES "${TETRISPHERE_GENERATED_DIR}/${GENERATED_SOURCE}")
endforeach()
if(NOT GENERATED_RECOMP_SOURCES)
    message(FATAL_ERROR "No genuine N64Recomp funcs_*.c files found")
endif()
set_source_files_properties(${GENERATED_RECOMP_SOURCES} PROPERTIES LANGUAGE CXX)
if(MSVC)
    set_source_files_properties(${GENERATED_RECOMP_SOURCES} PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(${GENERATED_RECOMP_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
endif()

file(SHA256 "${CMAKE_SOURCE_DIR}/config/recomp/symbols.toml" TETRISPHERE_MAP_ID)
file(SHA256 "${CMAKE_SOURCE_DIR}/config/recomp/tetrisphere.us.rev0.toml" TETRISPHERE_CONFIG_ID)
set(TETRISPHERE_BUILD_INPUTS
    CMakeLists.txt
    include/tetrisphere/diagnostics.h
    include/tetrisphere/recomp_support.h
    include/tetrisphere/runtime.h
    include/tetrisphere/microcode.h
    include/tetrisphere/linux_platform.h
    include/tetrisphere/close_game_menu.h
    include/tetrisphere/eeprom_store.h
    include/tetrisphere/rom_input.h
    include/tetrisphere/rom_cache.h
    include/tetrisphere/data_directory.h
    include/tetrisphere/settings_store.h
    include/tetrisphere/run_options.h
    include/tetrisphere/window_mode_controller.h
    include/tetrisphere/controller_slot.h
    include/tetrisphere/controller_ports.h
    include/tetrisphere/qa_virtual_controllers.h
    include/tetrisphere/port_input.h
    include/tetrisphere/canonical_trace.h
    include/tetrisphere/guest_trace_hooks.h
    include/tetrisphere/keyboard_state.h
    include/tetrisphere/audio_output.h
    include/tetrisphere/audio_length_pacer.h
    include/tetrisphere/diagnostic_log.h
    include/tetrisphere/rt64_context.h
    include/tetrisphere/dl_ui_tags.h
    include/tetrisphere/f3d_flat_shade.h
    include/tetrisphere/menu_medal_trace.h
    include/tetrisphere/full_frame_target_height.h
    include/tetrisphere/pause_capture_scene.h
    include/tetrisphere/prompt_atlas.h
    include/tetrisphere/training_prompts.h
    include/tetrisphere/run_identity.h
    src/diagnostic/main.cpp
    src/diagnostic/indirect_dispatch.cpp
    src/diagnostic/rescue_pause_snapshot_stub.cpp
    src/runtime/runtime.cpp
    src/runtime/unsupported.cpp
    src/platform/input_mapping.cpp
    src/platform/close_game_menu.cpp
    src/platform/eeprom_store.cpp
    src/platform/rom_input.cpp
    src/platform/rom_cache.cpp
    src/platform/settings_store.cpp
    src/platform/run_options.cpp
    src/platform/window_mode_controller.cpp
    src/platform/config_cli.cpp
    src/platform/windows_delay_libraries.cpp
    src/platform/controller_slot.cpp
    src/platform/controller_ports.cpp
    src/platform/qa_virtual_controllers.cpp
    src/platform/port_input.cpp
    src/platform/canonical_trace.cpp
    src/platform/guest_trace_hooks.cpp
    src/platform/keyboard_state.cpp
    src/platform/audio_output.cpp
    src/platform/diagnostic_log.cpp
    src/platform/linux_ai.cpp
    src/platform/linux_main.cpp
    src/platform/wav_capture.cpp
    src/graphics/rt64_context.cpp
    src/graphics/f3d_flat_shade.cpp
    src/graphics/prompt_atlas.cpp
    src/graphics/prompt_atlas_probe.cpp
    src/graphics/training_prompt_hooks.cpp
    src/graphics/training_prompts.cpp
    src/graphics/rt64_renderer.cpp
    src/audio/audio_bridge.cpp
    config/recomp/microcodes.json
    config/recomp/audio-rsp.toml
    config/recomp/runtime-symbols.json
    config/recomp/ui_anchor_callsites.json
    tools/recomp/ui_anchor_hooks.py
    tools/build/RuntimeBuildIdentity.cmake
    tools/build/fetch_dependencies.py
    tools/build/verify_runtime.py
    tools/build/verify_microcode_inputs.py
    tools/build/patch_rt64_probe.py
    tools/build/rt64_rescue_hud_probe.py
    tools/build/rt64_rescue_hud_layout.py
    tools/build/rt64_pause_snapshot.py
    tools/build/rt64_ui_tag_patch.py
    tools/build/rt64_ui_anchor_policy.py
    tools/build/rt64_m6_inspector.py
    tools/build/build_windows.ps1
    tests/rt64_host_stubs.cpp
)
set(TETRISPHERE_BUILD_INPUT_PATHS "")
foreach(INPUT IN LISTS TETRISPHERE_BUILD_INPUTS)
    list(APPEND TETRISPHERE_BUILD_INPUT_PATHS "${CMAKE_SOURCE_DIR}/${INPUT}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             ${TETRISPHERE_BUILD_INPUT_PATHS})
set(TETRISPHERE_BUILD_MATERIAL "${TETRISPHERE_MAP_ID}:${TETRISPHERE_CONFIG_ID}")
string(APPEND TETRISPHERE_BUILD_MATERIAL ":rt64_probe=${TETRISPHERE_RT64_PROBE}")
file(SHA256 "${TETRISPHERE_GENERATION_MANIFEST}" TETRISPHERE_MANIFEST_ID)
include("${CMAKE_SOURCE_DIR}/tools/build/RuntimeBuildIdentity.cmake")
tetrisphere_receipt_identity(N64MODERN_RUNTIME_RECEIPT_ID
                             "${N64MODERN_RUNTIME_RECEIPT}")
tetrisphere_receipt_identity(RT64_RECEIPT_ID "${RT64_RECEIPT}")
string(APPEND TETRISPHERE_BUILD_MATERIAL
       ":${TETRISPHERE_GENERATION_ID}:${TETRISPHERE_MANIFEST_ID}:"
       "${LOCKED_N64RECOMP_COMMIT}:${LOCKED_N64MODERNRUNTIME_COMMIT}:"
       "${N64MODERN_RUNTIME_SOURCE_ID}:${N64MODERN_RUNTIME_RECEIPT_ID}:"
       "${RT64_SOURCE_ID}:${RT64_RECEIPT_ID}${GENERATED_HASH_MATERIAL}")
foreach(INPUT IN LISTS TETRISPHERE_BUILD_INPUTS)
    file(SHA256 "${CMAKE_SOURCE_DIR}/${INPUT}" INPUT_HASH)
    string(APPEND TETRISPHERE_BUILD_MATERIAL ":${INPUT_HASH}")
endforeach()
tetrisphere_bind_runtime_identity(TETRISPHERE_BUILD_ID
    "${TETRISPHERE_BUILD_MATERIAL}"
    "${N64MODERN_RUNTIME_SOURCE_ID}:${RT64_SOURCE_ID}")

add_executable(tetrisphere_diag
    src/diagnostic/main.cpp
    src/platform/run_options.cpp
    src/diagnostic/indirect_dispatch.cpp
    src/diagnostic/rescue_pause_snapshot_stub.cpp
    src/platform/close_game_menu.cpp
    src/graphics/prompt_atlas_probe.cpp
    src/graphics/training_prompt_hooks.cpp
    src/runtime/runtime.cpp
    src/runtime/unsupported.cpp
    src/platform/eeprom_store.cpp
    src/platform/rom_input.cpp
    src/platform/rom_cache.cpp
    src/platform/guest_trace_hooks.cpp
    "${N64MODERN_RUNTIME}/librecomp/src/ai.cpp"
    "${N64MODERN_RUNTIME}/librecomp/src/cont.cpp"
    "${N64MODERN_RUNTIME}/librecomp/src/ultra_translation.cpp"
    "${N64MODERN_RUNTIME}/librecomp/src/vi.cpp"
    ${GENERATED_RECOMP_SOURCES}
)
add_library(tetrisphere_miniz STATIC "${RT64_ROOT}/src/contrib/miniz/miniz.c")
target_include_directories(tetrisphere_diag PRIVATE
    "${RT64_ROOT}/src/contrib/miniz")
target_link_libraries(tetrisphere_diag PRIVATE tetrisphere_miniz)

add_subdirectory("${N64MODERN_RUNTIME}/ultramodern"
                 "${CMAKE_BINARY_DIR}/n64modernruntime/ultramodern" EXCLUDE_FROM_ALL)
if(MSVC)
    get_target_property(ULTRAMODERN_COMPILE_OPTIONS ultramodern COMPILE_OPTIONS)
    if(ULTRAMODERN_COMPILE_OPTIONS)
        list(REMOVE_ITEM ULTRAMODERN_COMPILE_OPTIONS "-Wno-unused-parameter")
        set_property(TARGET ultramodern PROPERTY COMPILE_OPTIONS
                     "${ULTRAMODERN_COMPILE_OPTIONS}")
    endif()
endif()
add_dependencies(ultramodern verify_n64modernruntime)
add_dependencies(tetrisphere_diag verify_n64modernruntime)
add_dependencies(tetrisphere_diag verify_rt64)
add_executable(close_game_menu_test tests/close_game_menu_test.cpp
    src/platform/close_game_menu.cpp)
target_include_directories(close_game_menu_test PRIVATE
    "${CMAKE_SOURCE_DIR}/include" "${N64RECOMP_INCLUDE}")
if(MSVC)
    target_compile_options(close_game_menu_test PRIVATE /UNDEBUG)
else()
    target_compile_options(close_game_menu_test PRIVATE -UNDEBUG)
endif()
add_test(NAME close_game_menu COMMAND close_game_menu_test)

add_executable(run_options_test tests/run_options_test.cpp src/platform/run_options.cpp)
target_include_directories(run_options_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME run_options COMMAND run_options_test)
add_executable(window_mode_controller_test tests/window_mode_controller_test.cpp
    src/platform/window_mode_controller.cpp)
target_include_directories(window_mode_controller_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME window_mode_controller COMMAND window_mode_controller_test)

add_executable(runtime_contract_test
    tests/runtime_contract_test.cpp
    src/runtime/runtime.cpp
    src/runtime/unsupported.cpp
    src/platform/input_mapping.cpp
    src/platform/eeprom_store.cpp
    src/platform/wav_capture.cpp
)
target_include_directories(runtime_contract_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(runtime_contract_test SYSTEM PRIVATE
    "${N64RECOMP_INCLUDE}"
    "${N64MODERN_RUNTIME}/ultramodern/include"
    "${N64MODERN_RUNTIME}/thirdparty"
    "${N64MODERN_RUNTIME}/thirdparty/concurrentqueue"
    "${CMAKE_SOURCE_DIR}/.local/deps/N64ModernRuntime/thirdparty/json")
target_link_libraries(runtime_contract_test PRIVATE ultramodern)
add_dependencies(runtime_contract_test verify_n64modernruntime)
if(MSVC)
    target_compile_options(runtime_contract_test PRIVATE /W4)
else()
    target_compile_options(runtime_contract_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME runtime_registry
    COMMAND runtime_contract_test --registry "${CMAKE_SOURCE_DIR}/config/recomp/runtime-symbols.json")
add_test(NAME runtime_unsupported
    COMMAND "${CMAKE_COMMAND}"
    -DTEST_EXECUTABLE=$<TARGET_FILE:runtime_contract_test>
    -DTEST_MODE=--unsupported
    -DEXPECTED_PATTERN=fatal_unsupported.*osPiStartDma.*0x80000000.*0x80001234.*runtime-contract-test
    -P "${CMAKE_SOURCE_DIR}/tests/assert_runtime_fatal.cmake")
add_test(NAME runtime_unknown
    COMMAND "${CMAKE_COMMAND}"
    -DTEST_EXECUTABLE=$<TARGET_FILE:runtime_contract_test>
    -DTEST_MODE=--unknown
    -DEXPECTED_PATTERN=fatal_unsupported.*unknown_system_call.*0x81234567.*0x80004321.*runtime-contract-test
    -P "${CMAKE_SOURCE_DIR}/tests/assert_runtime_fatal.cmake")
add_test(NAME runtime_concurrent_fatal
    COMMAND "${CMAKE_COMMAND}"
    -DTEST_EXECUTABLE=$<TARGET_FILE:runtime_contract_test>
    -DTEST_MODE=--concurrent-fatal
    -DEXPECTED_PATTERN=fatal_unsupported.*func_80029674.*0x80029674.*0x800295A8.*boot-thread-entry
    -DEXPECTED_JSON_LINES=1
    -P "${CMAKE_SOURCE_DIR}/tests/assert_runtime_fatal.cmake")
add_test(NAME runtime_modern_backends
    COMMAND runtime_contract_test --backends)
add_test(NAME runtime_initial_image_layout
    COMMAND runtime_contract_test --initial-image)
add_test(NAME runtime_ipl3_state
    COMMAND runtime_contract_test --ipl3-state)
add_test(NAME runtime_eeprom_probe
    COMMAND runtime_contract_test --eeprom-probe)
add_test(NAME runtime_eeprom_roundtrip
    COMMAND runtime_contract_test --eeprom-roundtrip)
add_test(NAME runtime_eeprom_persistence
    COMMAND runtime_contract_test --eeprom-persistence
            "${CMAKE_CURRENT_BINARY_DIR}/eeprom-persistence")
add_executable(eeprom_store_test
    tests/eeprom_store_test.cpp src/platform/eeprom_store.cpp)
target_include_directories(eeprom_store_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME eeprom_interrupted_write
    COMMAND eeprom_store_test "${CMAKE_CURRENT_BINARY_DIR}/eeprom-store-test")
add_executable(settings_store_test tests/settings_store_test.cpp
    src/platform/settings_store.cpp src/platform/eeprom_store.cpp)
target_include_directories(settings_store_test PRIVATE "${CMAKE_SOURCE_DIR}/include"
    "${N64MODERN_RUNTIME}/thirdparty/json")
add_test(NAME settings_store_roundtrip
    COMMAND settings_store_test "${CMAKE_CURRENT_BINARY_DIR}/settings-store-test")
add_executable(config_gui_test tests/config_gui_test.cpp
    src/platform/config_gui.cpp src/platform/settings_store.cpp
    src/platform/eeprom_store.cpp)
target_include_directories(config_gui_test PRIVATE "${CMAKE_SOURCE_DIR}/include"
    "${N64MODERN_RUNTIME}/thirdparty/json")
add_test(NAME config_gui_graphics
    COMMAND config_gui_test "${CMAKE_CURRENT_BINARY_DIR}/config-gui-test")
add_executable(diagnostic_log_test tests/diagnostic_log_test.cpp
    src/platform/diagnostic_log.cpp)
target_include_directories(diagnostic_log_test PRIVATE "${CMAKE_SOURCE_DIR}/include"
    "${N64MODERN_RUNTIME}/thirdparty/json")
add_test(NAME diagnostic_log_metadata
    COMMAND diagnostic_log_test "${CMAKE_CURRENT_BINARY_DIR}")
add_executable(rom_input_test tests/rom_input_test.cpp src/platform/rom_input.cpp)
target_include_directories(rom_input_test PRIVATE "${CMAKE_SOURCE_DIR}/include"
    "${RT64_ROOT}/src/contrib/miniz")
target_link_libraries(rom_input_test PRIVATE tetrisphere_miniz)
add_test(NAME rom_input_formats
    COMMAND rom_input_test "${CMAKE_CURRENT_BINARY_DIR}/rom-input-test")
add_executable(rom_cache_test tests/rom_cache_test.cpp src/platform/rom_cache.cpp)
target_include_directories(rom_cache_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME rom_cache_roundtrip
    COMMAND rom_cache_test "${CMAKE_CURRENT_BINARY_DIR}/rom-cache-test")
add_test(NAME rom_missing_actionable_error
    COMMAND "${CMAKE_COMMAND}" -DTEST_EXECUTABLE=$<TARGET_FILE:tetrisphere_diag>
            -P "${CMAKE_SOURCE_DIR}/tests/assert_rom_error.cmake")
add_test(NAME runtime_sdl_audio_pacing
    COMMAND runtime_contract_test --sdl-audio-pacing)
add_test(NAME runtime_wav_capture
    COMMAND runtime_contract_test --wav-capture
            "${CMAKE_CURRENT_BINARY_DIR}/runtime-wav-capture.wav")
add_test(NAME runtime_zero_retrace_wait
    COMMAND runtime_contract_test --zero-retrace-wait)
add_test(NAME runtime_controller_prompts
    COMMAND runtime_contract_test --controller-prompts)
add_executable(prompt_atlas_test tests/prompt_atlas_test.cpp
    src/graphics/prompt_atlas.cpp)
target_include_directories(prompt_atlas_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME prompt_atlas_icons COMMAND prompt_atlas_test)
add_executable(f3d_flat_shade_test tests/f3d_flat_shade_test.cpp
    src/graphics/f3d_flat_shade.cpp)
target_include_directories(f3d_flat_shade_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME f3d_flat_shade_vertex_flag COMMAND f3d_flat_shade_test)
add_executable(menu_medal_trace_test tests/menu_medal_trace_test.cpp)
target_include_directories(menu_medal_trace_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME menu_medal_trace_return COMMAND menu_medal_trace_test)
add_executable(training_prompts_test tests/training_prompts_test.cpp
    src/graphics/training_prompts.cpp src/platform/input_mapping.cpp)
target_include_directories(training_prompts_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_test(NAME training_prompts COMMAND training_prompts_test)
add_test(NAME runtime_build_identity
    COMMAND "${CMAKE_COMMAND}"
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -P "${CMAKE_SOURCE_DIR}/tests/assert_runtime_build_identity.cmake")

add_executable(microcode_contract_test
    tests/microcode_contract_test.cpp
    src/graphics/rt64_renderer.cpp
    src/audio/audio_bridge.cpp
)
add_dependencies(microcode_contract_test verify_rt64)
target_include_directories(microcode_contract_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
if(MSVC)
    target_compile_options(microcode_contract_test PRIVATE /W4)
else()
    target_compile_options(microcode_contract_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME microcode_unit COMMAND microcode_contract_test --unit)
add_test(NAME microcode_python
    COMMAND "${Python3_EXECUTABLE}" -m unittest tests.test_m1_microcode)
set_tests_properties(microcode_python PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME rt64_probe_patch
    COMMAND "${Python3_EXECUTABLE}" -m unittest tests.test_rt64_probe_patch)
set_tests_properties(rt64_probe_patch PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME rt64_m6_inspector_patch
    COMMAND "${Python3_EXECUTABLE}" -m unittest tests.test_rt64_m6_inspector_patch)
set_tests_properties(rt64_m6_inspector_patch PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

set(MICROCODE_LIVE_INPUTS
    TETRISPHERE_RSP_RECOMP_SOURCE
    TETRISPHERE_GRAPHICS_SNAPSHOT
    TETRISPHERE_GRAPHICS_COMMANDS
    TETRISPHERE_AUDIO_SNAPSHOT)
set(MICROCODE_LIVE_ENABLED TRUE)
set(MICROCODE_LIVE_INPUT_COUNT 0)
foreach(INPUT_NAME IN LISTS MICROCODE_LIVE_INPUTS)
    if("${${INPUT_NAME}}" STREQUAL "")
        set(MICROCODE_LIVE_ENABLED FALSE)
    elseif(NOT EXISTS "${${INPUT_NAME}}")
        message(FATAL_ERROR "${INPUT_NAME} does not exist: ${${INPUT_NAME}}")
    else()
        math(EXPR MICROCODE_LIVE_INPUT_COUNT "${MICROCODE_LIVE_INPUT_COUNT} + 1")
    endif()
endforeach()
if(MICROCODE_LIVE_INPUT_COUNT GREATER 0 AND NOT MICROCODE_LIVE_ENABLED)
    message(FATAL_ERROR
        "Live microcode diagnostic requires all four private capture/RSP inputs")
endif()
if(MICROCODE_LIVE_ENABLED)
    set(MICROCODE_MANIFEST "${CMAKE_SOURCE_DIR}/config/recomp/microcodes.json")
    set(MICROCODE_RECEIPT "${CMAKE_BINARY_DIR}/microcode-input-verification.json")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/build/verify_microcode_inputs.py"
                --manifest "${MICROCODE_MANIFEST}"
                --graphics-snapshot "${TETRISPHERE_GRAPHICS_SNAPSHOT}"
                --graphics-commands "${TETRISPHERE_GRAPHICS_COMMANDS}"
                --audio-snapshot "${TETRISPHERE_AUDIO_SNAPSHOT}"
                --receipt "${MICROCODE_RECEIPT}"
        RESULT_VARIABLE MICROCODE_VERIFY_RESULT
        OUTPUT_VARIABLE MICROCODE_VERIFY_STDOUT
        ERROR_VARIABLE MICROCODE_VERIFY_STDERR)
    if(NOT MICROCODE_VERIFY_RESULT EQUAL 0)
        message(FATAL_ERROR "Microcode input verification failed: "
            "${MICROCODE_VERIFY_STDOUT}${MICROCODE_VERIFY_STDERR}")
    endif()
    file(READ "${MICROCODE_RECEIPT}" MICROCODE_RECEIPT_JSON)
    string(JSON MICROCODE_SOURCE_ID GET "${MICROCODE_RECEIPT_JSON}" source_identity)
    add_custom_target(verify_microcode_inputs ALL
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/build/verify_microcode_inputs.py"
                --manifest "${MICROCODE_MANIFEST}"
                --graphics-snapshot "${TETRISPHERE_GRAPHICS_SNAPSHOT}"
                --graphics-commands "${TETRISPHERE_GRAPHICS_COMMANDS}"
                --audio-snapshot "${TETRISPHERE_AUDIO_SNAPSHOT}"
                --receipt "${MICROCODE_RECEIPT}"
                --expected-source-identity "${MICROCODE_SOURCE_ID}"
        COMMENT "Verifying exact private microcode inputs"
        VERBATIM)
    set(RT64_STATIC ON CACHE BOOL "Build locked RT64 statically" FORCE)
    set(RT64_SDL_WINDOW_VULKAN ON CACHE BOOL "Use the host SDL Vulkan window" FORCE)
    set(NFD_PORTAL ON CACHE BOOL "Use xdg-desktop-portal without GTK headers" FORCE)
    set(TETRISPHERE_SAVED_BUILD_TESTING "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    add_subdirectory("${RT64_ROOT}" "${CMAKE_BINARY_DIR}/rt64" EXCLUDE_FROM_ALL)
    set(RT64_COPY_OUTPUT "${CMAKE_BINARY_DIR}/rt64-copy-src")
    set(RT64_COPY_ARGS --rt64-root "${RT64_ROOT}" --output "${RT64_COPY_OUTPUT}")
    set(RT64_COPY_NAMES rt64_application.cpp rt64_state.cpp
        rt64_projection_processor.cpp rt64_framebuffer_renderer.cpp
        rt64_framebuffer_manager.cpp rt64_workload_queue.cpp
        rt64_interpreter.cpp rt64_rdp.cpp rt64_rsp.cpp rt64_dynamic_libraries.cpp)
    if(TETRISPHERE_RT64_PROBE)
        list(APPEND RT64_COPY_ARGS --probe)
        list(APPEND RT64_COPY_NAMES rt64_present_queue.cpp)
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/build/patch_rt64_probe.py" ${RT64_COPY_ARGS}
        RESULT_VARIABLE RT64_COPY_RESULT
        OUTPUT_VARIABLE RT64_COPY_STDOUT
        ERROR_VARIABLE RT64_COPY_STDERR)
    if(NOT RT64_COPY_RESULT EQUAL 0)
        message(FATAL_ERROR "RT64 copied source verification failed: "
            "${RT64_COPY_STDOUT}${RT64_COPY_STDERR}")
    endif()
    get_target_property(RT64_COPY_TARGET_SOURCES rt64 SOURCES)
    set(RT64_COPY_REPLACED_SOURCES "")
    foreach(RT64_COPY_NAME IN LISTS RT64_COPY_NAMES)
        if(RT64_COPY_NAME STREQUAL "rt64_projection_processor.cpp" OR
           RT64_COPY_NAME STREQUAL "rt64_framebuffer_renderer.cpp")
            set(RT64_COPY_DIRECTORY "render")
        elseif(RT64_COPY_NAME STREQUAL "rt64_dynamic_libraries.cpp")
            set(RT64_COPY_DIRECTORY "common")
        else()
            set(RT64_COPY_DIRECTORY "hle")
        endif()
        set(RT64_COPY_ORIGINAL "${RT64_ROOT}/src/${RT64_COPY_DIRECTORY}/${RT64_COPY_NAME}")
        set(RT64_COPY_COUNT 0)
        foreach(RT64_COPY_SOURCE IN LISTS RT64_COPY_TARGET_SOURCES)
            if(RT64_COPY_SOURCE STREQUAL RT64_COPY_ORIGINAL)
                math(EXPR RT64_COPY_COUNT "${RT64_COPY_COUNT} + 1")
            endif()
        endforeach()
        if(NOT RT64_COPY_COUNT EQUAL 1)
            message(FATAL_ERROR "RT64 copy expected one source ${RT64_COPY_ORIGINAL}; "
                "found ${RT64_COPY_COUNT}")
        endif()
        list(REMOVE_ITEM RT64_COPY_TARGET_SOURCES "${RT64_COPY_ORIGINAL}")
        list(APPEND RT64_COPY_REPLACED_SOURCES "${RT64_COPY_OUTPUT}/${RT64_COPY_NAME}")
    endforeach()
    list(APPEND RT64_COPY_TARGET_SOURCES ${RT64_COPY_REPLACED_SOURCES})
    set_property(TARGET rt64 PROPERTY SOURCES "${RT64_COPY_TARGET_SOURCES}")
    target_include_directories(rt64 PRIVATE "${RT64_ROOT}/src/common" "${RT64_ROOT}/src/hle" "${RT64_ROOT}/src/render" "${RT64_ROOT}/src" "${CMAKE_SOURCE_DIR}/include")
    set(BUILD_TESTING "${TETRISPHERE_SAVED_BUILD_TESTING}")
    if(WIN32)
        set(TETRISPHERE_SDL_INCLUDE_DIRS
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/include")
        set(TETRISPHERE_SDL_LIBRARIES
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64/SDL2.lib"
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64/SDL2main.lib")
    else()
        set(TETRISPHERE_SDL_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
        set(TETRISPHERE_SDL_LIBRARIES ${SDL2_LIBRARIES})
    endif()
    set_source_files_properties("${TETRISPHERE_RSP_RECOMP_SOURCE}"
        PROPERTIES LANGUAGE CXX)
    add_executable(microcode_live_test
        tests/microcode_contract_test.cpp
        tests/rt64_host_stubs.cpp
        src/graphics/rt64_renderer.cpp
        src/audio/audio_bridge.cpp
        "${TETRISPHERE_RSP_RECOMP_SOURCE}"
        "${N64MODERN_RUNTIME}/librecomp/src/rsp.cpp"
    )
    target_compile_definitions(microcode_live_test PRIVATE
        TETRISPHERE_MICROCODE_LIVE HLSL_CPU
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
    if(NOT MSVC)
        target_compile_definitions(microcode_live_test PRIVATE FFX_GCC)
    else()
        target_compile_definitions(microcode_live_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    target_include_directories(microcode_live_test PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_BINARY_DIR}/src"
        "${RT64_ROOT}/src"
        "${RT64_ROOT}/src/contrib"
        "${RT64_ROOT}/src/contrib/hlslpp/include"
        "${RT64_ROOT}/src/contrib/imgui"
        "${RT64_ROOT}/src/contrib/mupen64plus-core/src/api"
        "${N64MODERN_RUNTIME}/librecomp/include"
        "${N64MODERN_RUNTIME}/librecomp/include/librecomp"
        "${N64MODERN_RUNTIME}/ultramodern/include"
        "${N64RECOMP_INCLUDE}"
        ${TETRISPHERE_SDL_INCLUDE_DIRS})
    target_link_libraries(microcode_live_test PRIVATE rt64 ${TETRISPHERE_SDL_LIBRARIES})
    add_dependencies(microcode_live_test verify_rt64 verify_microcode_inputs)
    if(MSVC)
        target_compile_options(microcode_live_test PRIVATE /W4)
    else()
        target_compile_options(microcode_live_test PRIVATE
            -Wall -Wextra -Wpedantic -msse4.1)
        set_source_files_properties("${TETRISPHERE_RSP_RECOMP_SOURCE}"
            PROPERTIES COMPILE_OPTIONS "-w")
    endif()
    add_test(NAME microcode_live
        COMMAND microcode_live_test --live
            "${TETRISPHERE_GRAPHICS_SNAPSHOT}"
            "${TETRISPHERE_GRAPHICS_COMMANDS}"
            "${TETRISPHERE_AUDIO_SNAPSHOT}")
    set_tests_properties(microcode_live PROPERTIES TIMEOUT 15)

    add_executable(tetrisphere-m1
        src/diagnostic/main.cpp
        src/platform/run_options.cpp
        src/platform/window_mode_controller.cpp
        src/diagnostic/indirect_dispatch.cpp
        src/graphics/prompt_atlas.cpp
        src/graphics/prompt_atlas_probe.cpp
        src/graphics/training_prompt_hooks.cpp
        src/graphics/training_prompts.cpp
        src/platform/linux_main.cpp
        src/platform/close_game_menu.cpp
        src/platform/linux_ai.cpp
        src/platform/wav_capture.cpp
        src/platform/eeprom_store.cpp
        src/platform/rom_input.cpp
        src/platform/rom_cache.cpp
        src/platform/settings_store.cpp
        src/platform/controller_slot.cpp
        src/platform/controller_ports.cpp
        src/platform/qa_virtual_controllers.cpp
        src/platform/port_input.cpp
        src/platform/canonical_trace.cpp
        src/platform/guest_trace_hooks.cpp
        src/platform/keyboard_state.cpp
        src/platform/audio_output.cpp
        src/platform/diagnostic_log.cpp
        src/platform/input_mapping.cpp
        src/graphics/rt64_context.cpp
        src/graphics/f3d_flat_shade.cpp
        src/runtime/runtime.cpp
        src/runtime/unsupported.cpp
        "${TETRISPHERE_RSP_RECOMP_SOURCE}"
        "${N64MODERN_RUNTIME}/librecomp/src/cont.cpp"
        "${N64MODERN_RUNTIME}/librecomp/src/rsp.cpp"
        "${N64MODERN_RUNTIME}/librecomp/src/ultra_translation.cpp"
        "${N64MODERN_RUNTIME}/librecomp/src/vi.cpp"
        ${GENERATED_RECOMP_SOURCES})
    target_compile_definitions(tetrisphere-m1 PRIVATE
        TETRISPHERE_LINUX_DEMO HLSL_CPU IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        TETRISPHERE_BUILD_ID="${TETRISPHERE_BUILD_ID}"
        TETRISPHERE_MAP_ID="${TETRISPHERE_MAP_ID}")
    if(NOT MSVC)
        target_compile_definitions(tetrisphere-m1 PRIVATE FFX_GCC)
    else()
        # Keep the Windows min/max macros out of RT64 and hlslpp headers.
        target_compile_definitions(tetrisphere-m1 PRIVATE NOMINMAX)
    endif()
    target_include_directories(tetrisphere-m1 PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${TETRISPHERE_GENERATED_DIR}"
        "${N64MODERN_RUNTIME}/librecomp/src"
        "${N64MODERN_RUNTIME}/librecomp/include"
        "${N64MODERN_RUNTIME}/librecomp/include/librecomp"
        "${N64MODERN_RUNTIME}/thirdparty/xxHash"
        "${RT64_ROOT}/src"
        "${RT64_ROOT}/src/contrib"
        "${RT64_ROOT}/src/contrib/miniz"
        "${RT64_ROOT}/src/contrib/hlslpp/include"
        "${RT64_ROOT}/src/contrib/imgui"
        "${RT64_ROOT}/src/contrib/mupen64plus-core/src/api"
        "${CMAKE_BINARY_DIR}/src")
    target_include_directories(tetrisphere-m1 SYSTEM PRIVATE
        "${N64RECOMP_INCLUDE}"
        "${N64MODERN_RUNTIME}/ultramodern/include"
        "${N64MODERN_RUNTIME}/thirdparty"
        "${N64MODERN_RUNTIME}/thirdparty/concurrentqueue"
        "${N64MODERN_RUNTIME}/thirdparty/json"
        ${TETRISPHERE_SDL_INCLUDE_DIRS})
    target_link_libraries(tetrisphere-m1 PRIVATE ultramodern rt64
                          tetrisphere_miniz
                          ${TETRISPHERE_SDL_LIBRARIES})
    add_executable(audio_output_test tests/audio_output_test.cpp
        src/platform/audio_output.cpp)
    if(NOT WIN32)
        find_path(TETRISPHERE_AUDIO_SDL_INCLUDE SDL.h
            HINTS "/home/linuxbrew/.linuxbrew/include" PATH_SUFFIXES SDL2)
        find_library(TETRISPHERE_AUDIO_SDL_LIBRARY SDL2
            HINTS "/home/linuxbrew/.linuxbrew/lib")
    endif()
    target_include_directories(audio_output_test PRIVATE
        "${CMAKE_SOURCE_DIR}/include" ${TETRISPHERE_SDL_INCLUDE_DIRS}
        ${TETRISPHERE_AUDIO_SDL_INCLUDE})
    target_link_libraries(audio_output_test PRIVATE ${TETRISPHERE_SDL_LIBRARIES}
        ${TETRISPHERE_AUDIO_SDL_LIBRARY})
    if(WIN32)
        target_link_directories(audio_output_test PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
    endif()
    add_test(NAME audio_device_recovery COMMAND audio_output_test)
    add_executable(tetrisphere-config
        src/platform/config_cli.cpp src/platform/config_gui.cpp
        src/platform/settings_store.cpp
        src/platform/eeprom_store.cpp)
    target_include_directories(tetrisphere-config PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${N64MODERN_RUNTIME}/thirdparty/json"
        ${TETRISPHERE_SDL_INCLUDE_DIRS})
    target_link_libraries(tetrisphere-config PRIVATE ${TETRISPHERE_SDL_LIBRARIES})
    if(UNIX AND NOT APPLE)
        set_target_properties(tetrisphere-m1 PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
        set_target_properties(tetrisphere-config PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
    endif()
    if(WIN32)
        target_link_directories(tetrisphere-config PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
    else()
        find_path(TETRISPHERE_CONFIG_SDL_INCLUDE SDL.h
            HINTS "/home/linuxbrew/.linuxbrew/include" PATH_SUFFIXES SDL2)
        find_library(TETRISPHERE_CONFIG_SDL_LIBRARY SDL2
            HINTS "/home/linuxbrew/.linuxbrew/lib")
        if(NOT TETRISPHERE_CONFIG_SDL_INCLUDE OR NOT TETRISPHERE_CONFIG_SDL_LIBRARY)
            message(FATAL_ERROR "SDL2 development files are required for tetrisphere-config")
        endif()
        target_include_directories(tetrisphere-config PRIVATE
            "${TETRISPHERE_CONFIG_SDL_INCLUDE}")
        target_link_libraries(tetrisphere-config PRIVATE
            "${TETRISPHERE_CONFIG_SDL_LIBRARY}")
    endif()
    add_test(NAME config_cli
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/config_cli_test.py"
                $<TARGET_FILE:tetrisphere-config> "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME portable_data
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/portable_data_test.py"
                $<TARGET_FILE:tetrisphere-config> "${CMAKE_CURRENT_BINARY_DIR}")
    add_executable(controller_slot_test tests/controller_slot_test.cpp
        src/platform/controller_slot.cpp)
    add_executable(controller_ports_test tests/controller_ports_test.cpp
        src/platform/controller_ports.cpp)
    add_executable(qa_virtual_controllers_test tests/qa_virtual_controllers_test.cpp
        src/platform/qa_virtual_controllers.cpp src/platform/controller_ports.cpp)
    add_executable(port_input_test tests/port_input_test.cpp
        src/platform/port_input.cpp)
    add_executable(canonical_trace_test tests/canonical_trace_test.cpp
        src/platform/canonical_trace.cpp)
    add_executable(guest_trace_hooks_test tests/guest_trace_hooks_test.cpp
        src/platform/guest_trace_hooks.cpp src/platform/canonical_trace.cpp)
    add_executable(rt64_graphics_config_test tests/rt64_graphics_config_test.cpp)
    add_executable(keyboard_state_test tests/keyboard_state_test.cpp
        src/platform/keyboard_state.cpp src/platform/input_mapping.cpp)
    foreach(INPUT_TARGET controller_slot_test controller_ports_test
            qa_virtual_controllers_test
            port_input_test keyboard_state_test)
        target_include_directories(${INPUT_TARGET} PRIVATE "${CMAKE_SOURCE_DIR}/include"
            ${TETRISPHERE_SDL_INCLUDE_DIRS} ${TETRISPHERE_CONFIG_SDL_INCLUDE})
    endforeach()
    target_include_directories(qa_virtual_controllers_test PRIVATE
        "${N64MODERN_RUNTIME}/thirdparty/json")
    target_include_directories(canonical_trace_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
    target_include_directories(guest_trace_hooks_test PRIVATE
        "${CMAKE_SOURCE_DIR}/include" "${N64RECOMP_INCLUDE}")
    target_include_directories(rt64_graphics_config_test PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${N64MODERN_RUNTIME}/ultramodern/include"
        "${N64MODERN_RUNTIME}/librecomp/include"
        "${N64MODERN_RUNTIME}/thirdparty")
    target_compile_definitions(guest_trace_hooks_test PRIVATE TETRISPHERE_LINUX_DEMO)
    target_link_libraries(controller_slot_test PRIVATE ${TETRISPHERE_SDL_LIBRARIES})
    target_link_libraries(controller_ports_test PRIVATE ${TETRISPHERE_SDL_LIBRARIES})
    target_link_libraries(qa_virtual_controllers_test PRIVATE ${TETRISPHERE_SDL_LIBRARIES})
    if(WIN32)
        target_link_directories(controller_slot_test PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
        target_link_directories(controller_ports_test PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
        target_link_directories(qa_virtual_controllers_test PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
    else()
        target_link_libraries(controller_slot_test PRIVATE
            "${TETRISPHERE_CONFIG_SDL_LIBRARY}")
        target_link_libraries(controller_ports_test PRIVATE
            "${TETRISPHERE_CONFIG_SDL_LIBRARY}")
        target_link_libraries(qa_virtual_controllers_test PRIVATE
            "${TETRISPHERE_CONFIG_SDL_LIBRARY}")
    endif()
    add_test(NAME controller_reconnect COMMAND controller_slot_test)
    add_test(NAME controller_two_ports COMMAND controller_ports_test)
    add_test(NAME qa_virtual_two_ports COMMAND qa_virtual_controllers_test)
    add_test(NAME controller_port_mapping COMMAND port_input_test)
    add_test(NAME canonical_trace COMMAND canonical_trace_test)
    add_test(NAME guest_trace_hooks COMMAND guest_trace_hooks_test)
    add_test(NAME rt64_graphics_config COMMAND rt64_graphics_config_test)
    add_test(NAME keyboard_focus_loss COMMAND keyboard_state_test)
    add_dependencies(tetrisphere-m1 verify_n64modernruntime verify_rt64
                     verify_microcode_inputs)
    if(NOT MSVC)
        target_compile_options(tetrisphere-m1 PRIVATE -Wall -Wextra -Wpedantic -msse4.1)
        set_source_files_properties("${TETRISPHERE_RSP_RECOMP_SOURCE}"
            PROPERTIES COMPILE_OPTIONS "-w")
    endif()
    if(WIN32)
        if(NOT MSVC)
            message(FATAL_ERROR "Windows portable library loading requires MSVC delay imports")
        endif()
        foreach(TETRISPHERE_PORTABLE_TARGET tetrisphere-m1 tetrisphere-config)
            target_sources(${TETRISPHERE_PORTABLE_TARGET} PRIVATE
                src/platform/windows_delay_libraries.cpp)
            target_link_libraries(${TETRISPHERE_PORTABLE_TARGET} PRIVATE delayimp)
            target_link_options(${TETRISPHERE_PORTABLE_TARGET} PRIVATE "/DELAYLOAD:SDL2.dll")
            add_custom_command(TARGET ${TETRISPHERE_PORTABLE_TARGET} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_FILE_DIR:${TETRISPHERE_PORTABLE_TARGET}>/lib"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64/SDL2.dll"
                    "$<TARGET_FILE_DIR:${TETRISPHERE_PORTABLE_TARGET}>/lib/SDL2.dll"
                VERBATIM)
        endforeach()
        target_link_options(tetrisphere-m1 PRIVATE "/DELAYLOAD:dxcompiler.dll")
        add_custom_command(TARGET tetrisphere-m1 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxcompiler.dll"
                "$<TARGET_FILE_DIR:tetrisphere-m1>/lib/dxcompiler.dll"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxil.dll"
                "$<TARGET_FILE_DIR:tetrisphere-m1>/lib/dxil.dll"
            VERBATIM)
    endif()
    if(WIN32)
        # RT64's Windows target also exports the legacy bare SDL2 library
        # names. Make their pinned vendored directory visible to consumers.
        target_link_directories(microcode_live_test PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
        target_link_directories(tetrisphere-m1 PRIVATE
            "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
        add_custom_command(TARGET tetrisphere-m1 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RT64_ROOT}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64/SDL2.dll"
                "$<TARGET_FILE_DIR:tetrisphere-m1>/SDL2.dll"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxcompiler.dll"
                "$<TARGET_FILE_DIR:tetrisphere-m1>/dxcompiler.dll"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxil.dll"
                "$<TARGET_FILE_DIR:tetrisphere-m1>/dxil.dll"
            VERBATIM)
    endif()
endif()
if(TETRISPHERE_ROM)
    add_test(NAME runtime_pi_raw_read
        COMMAND runtime_contract_test --pio "${TETRISPHERE_ROM}")
    add_test(NAME runtime_invalid_pio_destination
        COMMAND "${CMAKE_COMMAND}"
        -DTEST_EXECUTABLE=$<TARGET_FILE:runtime_contract_test>
        -DTEST_MODE=--invalid-pio-destination
        -DTEST_ARGUMENT=${TETRISPHERE_ROM}
        -DEXPECTED_PATTERN=fatal_unsupported.*osPiRawReadIo_destination.*0x80800000.*0x80029554.*boot-cartridge-pio
        -DEXPECTED_JSON_LINES=1
        -P "${CMAKE_SOURCE_DIR}/tests/assert_runtime_fatal.cmake")
    add_test(NAME runtime_boot_progress
        COMMAND "${CMAKE_COMMAND}"
        -DTEST_EXECUTABLE=$<TARGET_FILE:tetrisphere_diag>
        -DTEST_ROM=${TETRISPHERE_ROM}
        -P "${CMAKE_SOURCE_DIR}/tests/assert_runtime_boot.cmake")
endif()
target_include_directories(tetrisphere_diag PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${TETRISPHERE_GENERATED_DIR}"
)
target_include_directories(tetrisphere_diag SYSTEM PRIVATE "${N64RECOMP_INCLUDE}")
target_include_directories(tetrisphere_diag SYSTEM PRIVATE
    "${N64MODERN_RUNTIME}/ultramodern/include"
    "${N64MODERN_RUNTIME}/thirdparty"
    "${N64MODERN_RUNTIME}/thirdparty/concurrentqueue"
    "${N64MODERN_RUNTIME}/thirdparty/json")
target_include_directories(tetrisphere_diag PRIVATE
    "${N64MODERN_RUNTIME}/librecomp/src"
    "${N64MODERN_RUNTIME}/librecomp/include/librecomp")
target_link_libraries(tetrisphere_diag PRIVATE ultramodern)
target_compile_definitions(tetrisphere_diag PRIVATE
    TETRISPHERE_BUILD_ID="${TETRISPHERE_BUILD_ID}"
    TETRISPHERE_MAP_ID="${TETRISPHERE_MAP_ID}"
)
if(MSVC)
    target_compile_options(tetrisphere_diag PRIVATE /W4)
else()
    target_compile_options(tetrisphere_diag PRIVATE -Wall -Wextra -Wpedantic)
endif()
