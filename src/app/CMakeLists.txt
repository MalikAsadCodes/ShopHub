# shophub_ui: all Qt Widgets code. Built as a library so the GUI tests can use it.
qt_add_library(shophub_ui STATIC
    ui/Theme.h                      ui/Theme.cpp
    ui/UiCommon.h                   ui/UiCommon.cpp
    ui/MainWindow.h                 ui/MainWindow.cpp

    ui/models/TableModels.h         ui/models/TableModels.cpp

    ui/widgets/ImageView.h          ui/widgets/ImageView.cpp
    ui/widgets/ProductCard.h        ui/widgets/ProductCard.cpp
    ui/widgets/ProductGrid.h        ui/widgets/ProductGrid.cpp
    ui/widgets/Widgets.h            ui/widgets/Widgets.cpp

    ui/dialogs/AuthDialog.h         ui/dialogs/AuthDialog.cpp
    ui/dialogs/ShopDialogs.h        ui/dialogs/ShopDialogs.cpp
    ui/dialogs/AdminDialogs.h       ui/dialogs/AdminDialogs.cpp
    ui/dialogs/KhataDialogs.h       ui/dialogs/KhataDialogs.cpp

    ui/pages/Page.h                 ui/pages/Page.cpp
    ui/pages/CustomerPages.h        ui/pages/CustomerPages.cpp
    ui/pages/AdminPages.h           ui/pages/AdminPages.cpp
    ui/pages/KhataPages.h           ui/pages/KhataPages.cpp
)
target_include_directories(shophub_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(shophub_ui PUBLIC shophub_core Qt6::Widgets)

# The application itself. WIN32 = no console window on Windows.
qt_add_executable(ShopHub WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(ShopHub PRIVATE shophub_ui)
target_compile_definitions(ShopHub PRIVATE SHOPHUB_VERSION="${PROJECT_VERSION}")

# Windows: copy the Qt DLLs and plugins (including the SQLite driver) next to
# ShopHub.exe after every build, so it runs straight from VS Code or Explorer.
if(WIN32 AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET ShopHub POST_BUILD
        COMMAND Qt6::windeployqt --no-translations --verbose 0 "$<TARGET_FILE:ShopHub>"
        COMMENT "Copying Qt runtime files next to ShopHub.exe")
endif()
