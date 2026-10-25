# Each test is a small Qt Test executable linked against shophub_core only
# (no GUI), using an in-memory SQLite database.

# On Windows the test executables need Qt's bin folder on PATH to find the DLLs.
set(SHOPHUB_QT_BIN "")
if(WIN32 AND TARGET Qt6::windeployqt)
    get_target_property(_wdq Qt6::windeployqt IMPORTED_LOCATION)
    if(NOT _wdq)
        get_target_property(_wdq Qt6::windeployqt IMPORTED_LOCATION_RELEASE)
    endif()
    if(_wdq)
        get_filename_component(SHOPHUB_QT_BIN "${_wdq}" DIRECTORY)
    endif()
endif()

function(shophub_test_env name)
    if(SHOPHUB_QT_BIN AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
        set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${SHOPHUB_QT_BIN}")
    endif()
endfunction()

function(shophub_add_test name)
    qt_add_executable(${name} ${name}.cpp TestSupport.h)
    target_link_libraries(${name} PRIVATE shophub_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    shophub_test_env(${name})
endfunction()

# Unit tests
shophub_add_test(tst_money)
shophub_add_test(tst_validation)
shophub_add_test(tst_pricing)
shophub_add_test(tst_orderstatus)

# Integration tests (real SQLite, in memory)
shophub_add_test(tst_database)
shophub_add_test(tst_auth)
shophub_add_test(tst_catalog)
shophub_add_test(tst_cart_checkout)
shophub_add_test(tst_orders_inventory)
# Lets the sample-data test compare the app's accounts with docs/customer_accounts.csv.
target_compile_definitions(tst_orders_inventory PRIVATE SHOPHUB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
shophub_add_test(tst_khata)
shophub_add_test(tst_qist)

# GUI journeys (sign in, shop, checkout, admin pages).
# tst_gui picks Qt's invisible "offscreen" display when it can load it, and the normal
# display otherwise (see main() in tst_gui.cpp); windows then appear briefly while it runs.
qt_add_executable(tst_gui tst_gui.cpp TestSupport.h)
target_link_libraries(tst_gui PRIVATE shophub_ui Qt6::Test)
add_test(NAME tst_gui COMMAND tst_gui)
shophub_test_env(tst_gui)
