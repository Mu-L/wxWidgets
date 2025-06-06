#############################################################################
# Name:        build/cmake/toolkit.cmake
# Purpose:     CMake platform toolkit options
# Author:      Tobias Taschner
# Created:     2016-10-03
# Copyright:   (c) 2016 wxWidgets development team
# Licence:     wxWindows licence
#############################################################################

# Options required for toolkit selection/detection
wx_option(wxUSE_GUI "Use GUI" ON)

if(CMAKE_OSX_SYSROOT MATCHES iphoneos)
    set(IPHONE ON)

    # workaround a bug where try_compile (and functions using it,
    # like find_package, check_c_source_compiles) fails
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

if(WIN32)
    set(wxDEFAULT_TOOLKIT msw)
    set(wxTOOLKIT_OPTIONS msw gtk2 gtk3 qt)
    set(wxPLATFORM WIN32)
elseif(APPLE AND IPHONE)
    set(wxDEFAULT_TOOLKIT osx_iphone)
    set(wxTOOLKIT_OPTIONS osx_iphone)
    set(wxPLATFORM OSX)
elseif(APPLE)
    set(wxDEFAULT_TOOLKIT osx_cocoa)
    set(wxTOOLKIT_OPTIONS osx_cocoa gtk2 gtk3 gtk4 qt)
    set(wxPLATFORM OSX)
elseif(UNIX)
    set(wxDEFAULT_TOOLKIT gtk3)
    set(wxTOOLKIT_OPTIONS gtk2 gtk3 gtk4 qt)
    set(wxPLATFORM UNIX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

wx_option(wxBUILD_TOOLKIT "Toolkit used by wxWidgets" ${wxDEFAULT_TOOLKIT}
    STRINGS ${wxTOOLKIT_OPTIONS})
# TODO: set to univ for universal build
set(wxBUILD_WIDGETSET "")

# Create shortcut variable for easy toolkit tests
string(TOUPPER ${wxBUILD_TOOLKIT} wxBUILD_TOOLKIT_UPPER)
set(WX${wxBUILD_TOOLKIT_UPPER} ON)
if(wxBUILD_TOOLKIT MATCHES "^gtk*")
    set(WXGTK ON)
elseif(wxBUILD_TOOLKIT MATCHES "^osx*")
    set(WXOSX ON)
elseif(wxBUILD_TOOLKIT MATCHES "qt")
    set(WXQT ON)
endif()

set(wxTOOLKIT_DEFINITIONS __WX${wxBUILD_TOOLKIT_UPPER}__)

if(NOT wxUSE_GUI)
    set(wxBUILD_TOOLKIT "base")
    string(TOUPPER ${wxBUILD_TOOLKIT} wxBUILD_TOOLKIT_UPPER)
    set(WX${wxBUILD_TOOLKIT_UPPER} ON)
    set(wxTOOLKIT_DEFINITIONS __WX${wxBUILD_TOOLKIT_UPPER}__)
endif()

# Initialize toolkit variables
if(wxUSE_GUI)
set(wxTOOLKIT_INCLUDE_DIRS)
set(wxTOOLKIT_LIBRARIES)
set(wxTOOLKIT_VERSION)

if(WXGTK)
    if(WXGTK4)
        set(gtk_lib GTK4)
    elseif(WXGTK3)
        set(gtk_lib GTK3)
    elseif(WXGTK2)
        set(gtk_lib GTK2)
    endif()

    find_package(${gtk_lib} REQUIRED)
    list(APPEND wxTOOLKIT_INCLUDE_DIRS ${${gtk_lib}_INCLUDE_DIRS})
    list(APPEND wxTOOLKIT_LIBRARIES ${${gtk_lib}_LIBRARIES})
    list(APPEND wxTOOLKIT_DEFINITIONS ${${gtk_lib}_DEFINITIONS})
    list(APPEND wxTOOLKIT_DEFINITIONS __WXGTK__)
    set(wxTOOLKIT_VERSION ${${gtk_lib}_VERSION})

    if(WIN32 AND MSVC)
        if(WXGTK4)
            list(APPEND wxTOOLKIT_LIBRARIES
                libgtk-4.dll.a
                libgdk-4.dll.a
            )
        elseif(WXGTK3)
            list(APPEND wxTOOLKIT_LIBRARIES
                libgtk-3.dll.a
                libgdk-3.dll.a
            )
        elseif(WXGTK2)
            list(APPEND wxTOOLKIT_LIBRARIES
                gtk-win32-2.0
                gdk-win32-2.0
            )
        endif()
        list(APPEND wxTOOLKIT_LIBRARIES
            gio-2.0
            pangocairo-1.0
            gdk_pixbuf-2.0
            cairo
            pango-1.0
            gobject-2.0
            gthread-2.0
            glib-2.0
        )
    endif()
endif()

# We need X11 for non-GTK Unix ports (X11) and for GTK with X11
# support, but not for Wayland-only GTK (necessarily 3 or later), which is why
# we have to do this after find_package(GTKx) above, as this is what sets
# wxHAVE_GDK_X11.
if(UNIX AND NOT WIN32 AND (WXX11 OR WXGTK2 OR (WXGTK AND wxHAVE_GDK_X11)))
    find_package(X11 REQUIRED)
    list(APPEND wxTOOLKIT_INCLUDE_DIRS ${X11_INCLUDE_DIR})
    list(APPEND wxTOOLKIT_LIBRARIES ${X11_LIBRARIES})
endif()

if(WXQT)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)

    if(QT_VERSION_MAJOR EQUAL 5)
        set(QT_COMPONENTS Core Widgets Gui OpenGL OpenGL Test)
    elseif(QT_VERSION_MAJOR EQUAL 6)
        set(QT_COMPONENTS Core Widgets Gui OpenGL OpenGLWidgets Test)
    endif()

    foreach(QT_COMPONENT ${QT_COMPONENTS})
        find_package(Qt${QT_VERSION_MAJOR} COMPONENTS ${QT_COMPONENT} REQUIRED)
        list(APPEND wxTOOLKIT_INCLUDE_DIRS ${Qt${QT_VERSION_MAJOR}${QT_COMPONENT}_INCLUDE_DIRS})
        list(APPEND wxTOOLKIT_LIBRARIES ${Qt${QT_VERSION_MAJOR}${QT_COMPONENT}_LIBRARIES})
        list(APPEND wxTOOLKIT_DEFINITIONS ${Qt${QT_VERSION_MAJOR}${QT_COMPONENT}_COMPILE_DEFINITIONS})
    endforeach()
    set(wxTOOLKIT_VERSION ${Qt${QT_VERSION_MAJOR}Core_VERSION})

    if(ANDROID)
        # A hack to remove _${ANDROID_ABI} that Qt5AndroidSupport.cmake added
        # which breaks wx-config.
        set(CMAKE_SHARED_LIBRARY_SUFFIX_C ${CMAKE_SHARED_LIBRARY_SUFFIX})
        set(CMAKE_SHARED_LIBRARY_SUFFIX_CXX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif()
endif()

if(wxBUILD_TOOLKIT MATCHES "osx_cocoa")
    list(APPEND wxTOOLKIT_DEFINITIONS __WXMAC__ __WXOSX__)
endif()

endif() # wxUSE_GUI
