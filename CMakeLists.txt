cmake_minimum_required(VERSION 3.21)

project(peekpass VERSION 0.3.3 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent LinguistTools Network)

qt_standard_project_setup()

add_library(peekpass_core STATIC
    src/AppSettings.cpp
    src/AppSettings.h
    src/BwClient.cpp
    src/BwClient.h
    src/PasswordManager.cpp
    src/PasswordManager.h
    src/ProtonPassClient.cpp
    src/ProtonPassClient.h
    src/I18n.cpp
    src/I18n.h
    src/QuickWindow.cpp
    src/QuickWindow.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/TranslationCatalog.cpp
    src/VaultTypes.h
    src/WinHotkey.cpp
    src/WinHotkey.h
    src/SingleInstance.cpp
    src/SingleInstance.h
)

target_include_directories(peekpass_core PUBLIC src)
target_link_libraries(peekpass_core PUBLIC Qt6::Widgets Qt6::Concurrent Qt6::Network)
qt_add_executable(peekpass WIN32 src/main.cpp)
target_link_libraries(peekpass PRIVATE peekpass_core)
target_compile_definitions(peekpass PRIVATE PEEKPASS_VERSION="${PROJECT_VERSION}")

set(PEEKPASS_TRANSLATIONS
    translations/peekpass_ko.ts
)

qt_add_lrelease(peekpass
    TS_FILES ${PEEKPASS_TRANSLATIONS}
    QM_FILES_OUTPUT_VARIABLE PEEKPASS_QM_FILES
)

add_custom_target(peekpass_copy_translations ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:peekpass>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PEEKPASS_QM_FILES} "$<TARGET_FILE_DIR:peekpass>/translations"
    DEPENDS ${PEEKPASS_QM_FILES}
    VERBATIM
)
add_dependencies(peekpass_copy_translations peekpass_lrelease)

if(WIN32)
    target_compile_definitions(peekpass_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(peekpass_core PUBLIC advapi32)
endif()

if(MSVC)
    target_compile_options(peekpass PRIVATE /utf-8)
    target_compile_options(peekpass_core PRIVATE /utf-8)
endif()

if(BUILD_TESTING)
    add_executable(peekpass_fake_pass_cli
        tests/FakePassCli.cpp
    )
    target_link_libraries(peekpass_fake_pass_cli PRIVATE Qt6::Core)

    add_executable(peekpass_proton_tests
        tests/ProtonPassClientTests.cpp
    )
    target_link_libraries(peekpass_proton_tests PRIVATE peekpass_core)

    add_executable(peekpass_ui_tests tests/QuickWindowTests.cpp)
    target_link_libraries(peekpass_ui_tests PRIVATE peekpass_core)

    if(MSVC)
        target_compile_options(peekpass_fake_pass_cli PRIVATE /utf-8)
        target_compile_options(peekpass_proton_tests PRIVATE /utf-8)
        target_compile_options(peekpass_ui_tests PRIVATE /utf-8)
    endif()

    add_test(
        NAME proton_pass_client
        COMMAND peekpass_proton_tests "$<TARGET_FILE:peekpass_fake_pass_cli>"
    )
    set_tests_properties(proton_pass_client PROPERTIES TIMEOUT 30)
    add_test(NAME quick_window
        COMMAND peekpass_ui_tests "$<TARGET_FILE:peekpass_fake_pass_cli>"
    )
    set_tests_properties(quick_window PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS peekpass
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
