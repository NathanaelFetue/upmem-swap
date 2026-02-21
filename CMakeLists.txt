cmake_minimum_required(VERSION 3.16)
project(UPMEMSwap
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "User-space swap manager for UPMEM PIM architecture"
)

# ===== OPTIONS =====
option(BUILD_SHARED_LIBS "Build shared library instead of static" OFF)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build tests" ON)
option(WITH_DPU_SDK "Build with UPMEM SDK (requires dpu-pkg-config)" OFF)

# ===== COMPILER FLAGS =====
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

# ===== DEPENDENCIES =====
# Optional: UPMEM SDK (for real hardware)
if(WITH_DPU_SDK)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DPU dpu)
    if(DPU_FOUND)
        message(STATUS "Found UPMEM SDK: ${DPU_LIBRARIES}")
    else()
        message(WARNING "UPMEM SDK not found; building simulator-only version")
        set(WITH_DPU_SDK OFF)
    endif()
endif()

# ===== SOURCE FILES =====
set(SOURCES
    simulator/src/upmem_swap.c
    simulator/src/page_table.c
    simulator/src/memory_sim.c
    simulator/src/stats.c
    simulator/src/ssd_baseline.c
)

set(HEADERS
    simulator/src/upmem_swap.h
    simulator/src/page_table.h
    simulator/src/memory_sim.h
    simulator/src/stats.h
    simulator/src/ssd_baseline.h
    simulator/src/config.h
)

# ===== CORE LIBRARY =====
add_library(upmem_swap ${SOURCES})

target_include_directories(upmem_swap 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/simulator/src>
        $<INSTALL_INTERFACE:include/upmem_swap>
)

target_link_libraries(upmem_swap PUBLIC m)

# Add DPU SDK flags if available
if(WITH_DPU_SDK AND DPU_FOUND)
    target_link_libraries(upmem_swap PUBLIC ${DPU_LIBRARIES})
    target_compile_options(upmem_swap PRIVATE ${DPU_CFLAGS})
    target_compile_definitions(upmem_swap PRIVATE UPMEM_SDK_AVAILABLE=1)
endif()

# ===== EXPORTS =====
set_target_properties(upmem_swap PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ===== EXAMPLES =====
if(BUILD_EXAMPLES)
    add_executable(example_integration simulator/example_integration.c)
    target_link_libraries(example_integration PRIVATE upmem_swap)
    target_include_directories(example_integration PRIVATE simulator/src)
endif()

# ===== TESTS =====
if(BUILD_TESTS)
    enable_testing()
    add_executable(test_basic simulator/src/benchmark_batch_improved.c)
    target_link_libraries(test_basic PRIVATE upmem_swap)
    target_include_directories(test_basic PRIVATE simulator/src)
endif()

# ===== BENCHMARK TARGETS =====
add_executable(benchmark_batch_improved simulator/src/benchmark_batch_improved.c)
target_link_libraries(benchmark_batch_improved PRIVATE upmem_swap)
target_include_directories(benchmark_batch_improved PRIVATE simulator/src)

# ===== INSTALLATION =====
# Install library
install(TARGETS upmem_swap
    EXPORT UPMEMSwapTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${HEADERS}
    DESTINATION include/upmem_swap
)

# Install examples
if(BUILD_EXAMPLES)
    install(TARGETS example_integration
        DESTINATION bin
    )
    install(FILES simulator/example_integration.c
        DESTINATION share/upmem_swap/examples
    )
endif()

# ===== CMAKE CONFIG FILES =====
include(CMakePackageConfigHelpers)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/UPMEMSwapConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Generate targets file
install(EXPORT UPMEMSwapTargets
    FILE UPMEMSwapTargets.cmake
    NAMESPACE UPMEMSwap::
    DESTINATION lib/cmake/UPMEMSwap
)

# Generate config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UPMEMSwapConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/UPMEMSwapConfig.cmake"
    INSTALL_DESTINATION lib/cmake/UPMEMSwap
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/UPMEMSwapConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/UPMEMSwapConfigVersion.cmake"
    DESTINATION lib/cmake/UPMEMSwap
)

# ===== SUMMARY =====
message(STATUS "")
message(STATUS "=== UPMEM Swap Build Configuration ===")
message(STATUS "Version:           ${PROJECT_VERSION}")
message(STATUS "Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared Library:    ${BUILD_SHARED_LIBS}")
message(STATUS "Build Examples:    ${BUILD_EXAMPLES}")
message(STATUS "Build Tests:       ${BUILD_TESTS}")
message(STATUS "UPMEM SDK:         ${WITH_DPU_SDK}")
message(STATUS "Install Prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
