# SPDX-License-Identifier: LGPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

project(kde-portal-plasma LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus)

add_library(kde-portal MODULE
    kde_portal.cpp
    portal/kde_portal_settings.cpp
)

target_link_libraries(kde-portal
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
)

add_library(kde-portal-plasma MODULE
    kguiaddons/kde_color_utils.cpp
    kcolorscheme/kde_palette.cpp
    kde_portal_plasma.cpp
    portal/kde_portal_settings.cpp
)

target_link_libraries(kde-portal-plasma
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
)

if(TARGET Qt6::GuiPrivate)
    target_link_libraries(kde-portal PRIVATE Qt6::GuiPrivate)
    target_link_libraries(kde-portal-plasma PRIVATE Qt6::GuiPrivate)
endif()

set_target_properties(kde-portal PROPERTIES
    PREFIX ""
    OUTPUT_NAME "kde-portal"
)

set_target_properties(kde-portal-plasma PROPERTIES
    PREFIX ""
    OUTPUT_NAME "kde-portal-plasma"
)

install(TARGETS kde-portal kde-portal-plasma
    LIBRARY DESTINATION plugins/platformthemes
)
