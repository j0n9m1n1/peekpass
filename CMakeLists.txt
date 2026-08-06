cmake_minimum_required(VERSION 3.21)

project(peekpass VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent LinguistTools Network)

qt_standard_project_setup()

qt_add_executable(peekpass WIN32
    src/main.cpp
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
)

target_link_libraries(peekpass PRIVATE Qt6::Widgets Qt6::Concurrent Qt6::Network)

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
    target_compile_definitions(peekpass PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(peekpass PRIVATE advapi32)
endif()

if(MSVC)
    target_compile_options(peekpass PRIVATE /utf-8)
endif()

install(TARGETS peekpass
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
