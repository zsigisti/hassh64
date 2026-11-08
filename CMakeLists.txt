cmake_minimum_required(VERSION 3.14)
project(hassh64 VERSION 2.3.1 LANGUAGES C)

# Header-only. Use with add_subdirectory() or FetchContent, then
#   target_link_libraries(your_target PRIVATE hassh64::hassh64)
add_library(hassh64 INTERFACE)
add_library(hassh64::hassh64 ALIAS hassh64)
target_include_directories(hassh64 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_features(hassh64 INTERFACE c_std_99)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(hassh64_vectors test/vectors.c)
        target_link_libraries(hassh64_vectors PRIVATE hassh64)
        add_test(NAME vectors COMMAND hassh64_vectors)
        add_executable(hassh64_stream test/stream.c)
        target_link_libraries(hassh64_stream PRIVATE hassh64)
        add_test(NAME stream COMMAND hassh64_stream)
        add_executable(hassh64_cli tools/hassh64.c)
        set_target_properties(hassh64_cli PROPERTIES OUTPUT_NAME hassh64)
        target_link_libraries(hassh64_cli PRIVATE hassh64)
        if(WIN32)
            target_link_libraries(hassh64_cli PRIVATE shell32)
        endif()
        add_test(NAME cli COMMAND hassh64_cli -s 42 hello)
        set_tests_properties(cli PROPERTIES PASS_REGULAR_EXPRESSION "^825305b7d83ed2a7")
    endif()

    include(GNUInstallDirs)
    install(FILES hassh.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS hassh64 EXPORT hassh64Targets)
    install(EXPORT hassh64Targets NAMESPACE hassh64:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hassh64)
    include(CMakePackageConfigHelpers)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/hassh64Config.cmake
         "include(\"\${CMAKE_CURRENT_LIST_DIR}/hassh64Targets.cmake\")\n")
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/hassh64ConfigVersion.cmake
         COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hassh64Config.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/hassh64ConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hassh64)
endif()
