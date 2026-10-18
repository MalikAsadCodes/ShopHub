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

# GUI journeys (sign in, shop, checkout, admin pages).
# Runs on Qt's invisible "offscreen" display when this Qt build has it. Some builds
# (for example MSYS2's Qt on Windows) only ship the real "windows" display; then the
# test uses that and the windows appear briefly while it runs.
qt_add_executable(tst_gui tst_gui.cpp TestSupport.h)
target_link_libraries(tst_gui PRIVATE shophub_ui Qt6::Test)
add_test(NAME tst_gui COMMAND tst_gui)
find_file(SHOPHUB_QOFFSCREEN_PLUGIN
    NAMES qoffscreen.dll qoffscreend.dll libqoffscreen.so libqoffscreen.dylib
    PATHS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platforms"
    NO_DEFAULT_PATH)
if(SHOPHUB_QOFFSCREEN_PLUGIN)
    set_tests_properties(tst_gui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
else()
    message(STATUS "Qt 'offscreen' platform plugin not found: tst_gui will use the default display")
endif()
shophub_test_env(tst_gui)
