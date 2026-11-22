cmake_minimum_required(VERSION 3.21)

project(vamp9_pos
    VERSION 1.0.0
    DESCRIPTION "vamp9 POS - Commercial Offline-First Supermarket Point of Sale System"
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# Global C++20 & Build Hygiene
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(POS_BUILD_TESTS "Build unit tests" ON)

# ------------------------------------------------------------------------------
# Strict Compiler Warnings Interface
# ------------------------------------------------------------------------------
add_library(pos_compiler_warnings INTERFACE)
target_compile_features(pos_compiler_warnings INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(pos_compiler_warnings INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
else()
    target_compile_options(pos_compiler_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Sql Svg)

# ------------------------------------------------------------------------------
# Layer 1: Domain (Pure C++20 — Zero Qt / Framework Dependencies)
# ------------------------------------------------------------------------------
add_library(pos_domain INTERFACE)
target_include_directories(pos_domain INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(pos_domain INTERFACE pos_compiler_warnings)

# ------------------------------------------------------------------------------
# Layer 2: Application (Use Cases & Services — Depends only on Domain)
# ------------------------------------------------------------------------------
add_library(pos_application STATIC
    src/application/CartService.h
    src/application/CartService.cpp
    src/application/SyncWorker.h
    src/application/SyncWorker.cpp
)
target_link_libraries(pos_application
    PUBLIC
        pos_domain
    PRIVATE
        pos_compiler_warnings
        Threads::Threads
)

# ------------------------------------------------------------------------------
# Layer 3: Infrastructure (SQLite/WAL, Hardware Adapters, File Logger)
# ------------------------------------------------------------------------------
add_library(pos_infrastructure STATIC
    src/infrastructure/persistence/DatabaseManager.h
    src/infrastructure/persistence/DatabaseManager.cpp
    src/infrastructure/persistence/SqliteProductRepository.h
    src/infrastructure/persistence/SqliteProductRepository.cpp
    src/infrastructure/hardware/MockReceiptPrinter.h
    src/infrastructure/hardware/MockReceiptPrinter.cpp
    src/infrastructure/logging/Logger.h
    src/infrastructure/logging/Logger.cpp
)
target_link_libraries(pos_infrastructure
    PUBLIC
        pos_domain
        Qt6::Core
        Qt6::Sql
    PRIVATE
        pos_compiler_warnings
)

# ------------------------------------------------------------------------------
# Layer 4: Presentation (Qt6 Widgets + Commercial Retail UI + ToastManager)
# ------------------------------------------------------------------------------
add_library(pos_presentation STATIC
    src/presentation/MainWindow.h
    src/presentation/MainWindow.cpp
    src/presentation/hardware/BarcodeScannerHandler.h
    src/presentation/hardware/BarcodeScannerHandler.cpp
    src/presentation/utils/AppPreferences.h
    src/presentation/utils/AppPreferences.cpp
    src/presentation/utils/CurrencyFormatter.h
    src/presentation/utils/CurrencyFormatter.cpp
    src/presentation/utils/TranslationManager.h
    src/presentation/utils/TranslationManager.cpp
    src/presentation/widgets/ProductCardWidget.h
    src/presentation/widgets/ProductCardWidget.cpp
    src/presentation/widgets/TicketItemDelegate.h
    src/presentation/widgets/TicketItemDelegate.cpp
    src/presentation/widgets/ToastManager.h
    src/presentation/widgets/ToastManager.cpp
    src/presentation/widgets/TopBar.h
    src/presentation/widgets/TopBar.cpp
    src/presentation/views/CheckoutView.h
    src/presentation/views/CheckoutView.cpp
    src/presentation/views/InventoryView.h
    src/presentation/views/InventoryView.cpp
    src/presentation/views/ReportsView.h
    src/presentation/views/ReportsView.cpp
    src/presentation/views/SettingsView.h
    src/presentation/views/SettingsView.cpp
)
target_link_libraries(pos_presentation
    PUBLIC
        pos_application
        pos_domain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
    PRIVATE
        pos_compiler_warnings
        $<$<BOOL:${WIN32}>:winmm>
)

# ------------------------------------------------------------------------------
# Executable Entry Point (Composition Root — WIN32 Subsystem, Zero Console)
# ------------------------------------------------------------------------------
if(WIN32)
    qt_add_executable(vamp9_pos WIN32 src/main.cpp assets/windows/app_icon.rc)
    set_target_properties(vamp9_pos PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    qt_add_executable(vamp9_pos src/main.cpp)
endif()

target_link_libraries(vamp9_pos
    PRIVATE
        pos_presentation
        pos_infrastructure
        pos_application
        pos_domain
        pos_compiler_warnings
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
        Qt6::Svg
)

# Copy runtime assets and SQL migrations alongside the binary
add_custom_command(TARGET vamp9_pos POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:vamp9_pos>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/database
        $<TARGET_FILE_DIR:vamp9_pos>/database
    COMMENT "Copying runtime assets and SQL migrations to output directory"
)

# ------------------------------------------------------------------------------
# Testing
# ------------------------------------------------------------------------------
if(POS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
