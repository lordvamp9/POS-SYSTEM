find_package(Qt6 6.2 REQUIRED COMPONENTS Test)

add_executable(test_cart_calculations
    test_cart_calculations.cpp
)

target_link_libraries(test_cart_calculations
    PRIVATE
        pos_application
        pos_domain
        pos_compiler_warnings
        Qt6::Test
)

add_test(NAME CartCalculationsTests COMMAND test_cart_calculations)

add_executable(test_currency_formatter
    test_currency_formatter.cpp
    ../src/presentation/utils/CurrencyFormatter.cpp
)

target_include_directories(test_currency_formatter
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_currency_formatter
    PRIVATE
        pos_compiler_warnings
        Qt6::Core
        Qt6::Test
)

add_test(NAME CurrencyFormatterTests COMMAND test_currency_formatter)

add_executable(test_ui_and_toasts
    test_ui_and_toasts.cpp
)

target_compile_definitions(test_ui_and_toasts
    PRIVATE
        VAMP9_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(test_ui_and_toasts
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
        Qt6::Test
)

add_test(NAME UiAndToastsTests COMMAND test_ui_and_toasts)
