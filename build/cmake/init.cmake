#############################################################################
# Name:        build/cmake/init.cmake
# Purpose:     Initialize variables based on user selection and system
#              information before creating build targets
# Author:      Tobias Taschner
# Created:     2016-09-24
# Copyright:   (c) 2016 wxWidgets development team
# Licence:     wxWindows licence
#############################################################################

function(checkCompilerDefaults)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
        #include <vector>
        int main() {
            std::vector<int> v{1,2,3};
            for (auto& n : v)
                --n;
            return v[0];
        }"
        wxHAVE_CXX11)

    check_cxx_source_compiles("
        #if defined(_MSVC_LANG)
            #if _MSVC_LANG < 201703L
                #error C++17 support is required
            #endif
        #elif __cplusplus < 201703L
            #error C++17 support is required
        #endif
        int main() {
            [[maybe_unused]] auto unused = 17;
        }"
        wxHAVE_CXX17)
endfunction()

if(DEFINED CMAKE_CXX_STANDARD)
    # User has explicitly set a CMAKE_CXX_STANDARD.
elseif(DEFINED wxBUILD_CXX_STANDARD AND NOT wxBUILD_CXX_STANDARD STREQUAL COMPILER_DEFAULT)
    # Standard is set using wxBUILD_CXX_STANDARD.
    set(CMAKE_CXX_STANDARD ${wxBUILD_CXX_STANDARD})
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    # CMAKE_CXX_STANDARD not defined.
    checkCompilerDefaults()
    if(NOT wxHAVE_CXX11)
        # If the standard is not set explicitly, and the default compiler settings
        # do not support c++11, request it explicitly.
        set(CMAKE_CXX_STANDARD 11)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
    endif()
endif()

if(MSVC)
    if(CMAKE_VERSION VERSION_LESS "3.15")
    # CMake 3.15 and later use MSVC_RUNTIME_LIBRARY property, see functions.cmake
    # Determine MSVC runtime library flag
    set(MSVC_LIB_USE "/MD")
    set(MSVC_LIB_REPLACE "/MT")
    if(wxBUILD_USE_STATIC_RUNTIME)
        set(MSVC_LIB_USE "/MT")
        set(MSVC_LIB_REPLACE "/MD")
    endif()
    # Set MSVC runtime flags for all configurations
    foreach(cfg "" ${CMAKE_CONFIGURATION_TYPES})
        set(c_flag_var CMAKE_C_FLAGS)
        set(cxx_flag_var CMAKE_CXX_FLAGS)
        if(cfg)
            string(TOUPPER ${cfg} cfg_upper)
            wx_string_append(c_flag_var "_${cfg_upper}")
            wx_string_append(cxx_flag_var "_${cfg_upper}")
        endif()
        if(${c_flag_var} MATCHES ${MSVC_LIB_REPLACE})
            string(REPLACE ${MSVC_LIB_REPLACE} ${MSVC_LIB_USE} ${c_flag_var} "${${c_flag_var}}")
            set(${c_flag_var} ${${c_flag_var}} CACHE STRING
              "Flags used by the C compiler during ${cfg_upper} builds." FORCE)
        endif()
        if(${cxx_flag_var} MATCHES ${MSVC_LIB_REPLACE})
            string(REPLACE ${MSVC_LIB_REPLACE} ${MSVC_LIB_USE} ${cxx_flag_var} "${${cxx_flag_var}}")
            set(${cxx_flag_var} ${${cxx_flag_var}} CACHE STRING
              "Flags used by the CXX compiler during ${cfg_upper} builds." FORCE)
        endif()
    endforeach()
    endif()

    if(wxBUILD_SHARED AND wxBUILD_USE_STATIC_RUNTIME AND wxUSE_STD_IOSTREAM)
        # Objects like std::cout are defined as extern in <iostream> and implemented in libcpmt.
        # This is statically linked into wxbase (stdstream.cpp).
        # When building an application with both wxbase and libcpmt,
        # the linker gives 'multiply defined symbols' error.
        message(WARNING "wxUSE_STD_IOSTREAM combined with wxBUILD_USE_STATIC_RUNTIME will fail to link when using std::cout or similar functions")
    endif()

    if(wxBUILD_OPTIMISE)
        set(MSVC_LINKER_RELEASE_FLAGS " /LTCG /OPT:REF /OPT:ICF")
        wx_string_append(CMAKE_EXE_LINKER_FLAGS_RELEASE "${MSVC_LINKER_RELEASE_FLAGS}")
        wx_string_append(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${MSVC_LINKER_RELEASE_FLAGS}")
        wx_string_append(CMAKE_STATIC_LINKER_FLAGS_RELEASE " /LTCG")
        set(MSVC_COMPILER_RELEASE_FLAGS " /Ox /Oi /Ot /Oy /GS- /Gy /GL /Gw")
        wx_string_append(CMAKE_CXX_FLAGS_RELEASE "${MSVC_COMPILER_RELEASE_FLAGS}")
        wx_string_append(CMAKE_C_FLAGS_RELEASE "${MSVC_COMPILER_RELEASE_FLAGS}")
    endif()

    if(NOT wxBUILD_STRIPPED_RELEASE)
        set(MSVC_PDB_FLAG " /DEBUG")
    endif()
    wx_string_append(CMAKE_EXE_LINKER_FLAGS_RELEASE "${MSVC_PDB_FLAG}")
    wx_string_append(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${MSVC_PDB_FLAG}")

    if(wxBUILD_MSVC_MULTIPROC)
        wx_string_append(CMAKE_C_FLAGS " /MP")
        wx_string_append(CMAKE_CXX_FLAGS " /MP")
    endif()

    if(NOT POLICY CMP0092)
        string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
    if(wxBUILD_OPTIMISE)
        set(GCC_PREFERRED_RELEASE_FLAGS " -O2 -fomit-frame-pointer")
        wx_string_append(CMAKE_CXX_FLAGS_RELEASE "${GCC_PREFERRED_RELEASE_FLAGS}")
        wx_string_append(CMAKE_C_FLAGS_RELEASE "${GCC_PREFERRED_RELEASE_FLAGS}")
    endif()

    if(wxBUILD_STRIPPED_RELEASE)
        set(LD_STRIPPING_FLAG " -s")
        wx_string_append(CMAKE_EXE_LINKER_FLAGS_RELEASE "${LD_STRIPPING_FLAG}")
        wx_string_append(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${LD_STRIPPING_FLAG}")
    else()
        set(COMPILER_DBGSYM_FLAG " -g")
        wx_string_append(CMAKE_CXX_FLAGS_RELEASE "${COMPILER_DBGSYM_FLAG}")
        wx_string_append(CMAKE_C_FLAGS_RELEASE "${COMPILER_DBGSYM_FLAG}")
    endif()

    if(wxBUILD_USE_STATIC_RUNTIME AND NOT APPLE)
        if(MINGW)
            set(STATIC_LINKER_FLAGS " -static")
        else()
            set(STATIC_LINKER_FLAGS " -static-libgcc -static-libstdc++")
        endif()
        wx_string_append(CMAKE_EXE_LINKER_FLAGS "${STATIC_LINKER_FLAGS}")
        wx_string_append(CMAKE_SHARED_LINKER_FLAGS "${STATIC_LINKER_FLAGS}")
    endif()
endif()

if(NOT wxBUILD_COMPATIBILITY STREQUAL "NONE")
    set(WXWIN_COMPATIBILITY_3_2 ON)
    if(wxBUILD_COMPATIBILITY VERSION_LESS 3.2)
        set(WXWIN_COMPATIBILITY_3_0 ON)
    endif()
endif()

# Build wxBUILD_FILE_ID used for config and setup path
#TODO: build different id for WIN32
set(wxBUILD_FILE_ID "${wxBUILD_TOOLKIT}${wxBUILD_WIDGETSET}-")
wx_string_append(wxBUILD_FILE_ID "unicode")
if(NOT wxBUILD_SHARED)
    wx_string_append(wxBUILD_FILE_ID "-static")
endif()
wx_string_append(wxBUILD_FILE_ID "-${wxMAJOR_VERSION}.${wxMINOR_VERSION}")
wx_get_flavour(lib_flavour "-")
wx_string_append(wxBUILD_FILE_ID "${lib_flavour}")

set(wxPLATFORM_ARCH)
if(CMAKE_GENERATOR_PLATFORM)
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
        string(TOLOWER ${CMAKE_GENERATOR_PLATFORM} wxPLATFORM_ARCH)
    endif()
elseif(CMAKE_VS_PLATFORM_NAME)
    if(NOT CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
        string(TOLOWER ${CMAKE_VS_PLATFORM_NAME} wxPLATFORM_ARCH)
    endif()
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(wxPLATFORM_ARCH "x64")
    endif()
endif()

set(wxARCH_SUFFIX)
set(wxCOMPILER_PREFIX)
set(wxPLATFORM_LIB_DIR)

if(WIN32)
    # TODO: include compiler version in wxCOMPILER_PREFIX for official builds
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(wxCOMPILER_PREFIX "vc")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(wxCOMPILER_PREFIX "gcc")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(wxCOMPILER_PREFIX "clang")
    else()
        message(FATAL_ERROR "Unknown WIN32 compiler type")
    endif()

    if(wxPLATFORM_ARCH)
        set(wxARCH_SUFFIX "_${wxPLATFORM_ARCH}")
    endif()
endif()

if(WIN32_MSVC_NAMING)
    if(wxBUILD_SHARED)
        set(lib_suffix "_dll")
    else()
        set(lib_suffix "_lib")
    endif()

    set(wxPLATFORM_LIB_DIR "${wxCOMPILER_PREFIX}${wxARCH_SUFFIX}${lib_suffix}")

    # Generator expression to not create different Debug and Release directories
    set(GEN_EXPR_DIR "$<1:/>")
    set(wxINSTALL_INCLUDE_DIR "include")
else()
    set(GEN_EXPR_DIR "/")
    wx_get_flavour(lib_flavour "-")
    set(wxINSTALL_INCLUDE_DIR "include/wx-${wxMAJOR_VERSION}.${wxMINOR_VERSION}${lib_flavour}")
endif()

if(wxBUILD_CUSTOM_SETUP_HEADER_PATH)
    if(NOT EXISTS "${wxBUILD_CUSTOM_SETUP_HEADER_PATH}/wx/setup.h")
        message(FATAL_ERROR "wxBUILD_CUSTOM_SETUP_HEADER_PATH needs to contain a wx/setup.h file")
    endif()
    set(wxSETUP_HEADER_PATH ${wxBUILD_CUSTOM_SETUP_HEADER_PATH})
else()
    # Set path where setup.h will be created
    if(WIN32_MSVC_NAMING)
        set(lib_unicode u)
        set(wxSETUP_HEADER_PATH
            ${wxOUTPUT_DIR}/${wxPLATFORM_LIB_DIR}/${wxBUILD_TOOLKIT}${lib_unicode})
        file(MAKE_DIRECTORY ${wxSETUP_HEADER_PATH}/wx)
        file(MAKE_DIRECTORY ${wxSETUP_HEADER_PATH}d/wx)
        set(wxSETUP_HEADER_FILE_DEBUG ${wxSETUP_HEADER_PATH}d/wx/setup.h)
    else()
        set(wxSETUP_HEADER_PATH
            ${wxOUTPUT_DIR}/wx/include/${wxBUILD_FILE_ID})
        file(MAKE_DIRECTORY ${wxSETUP_HEADER_PATH}/wx)
    endif()
endif()
set(wxSETUP_HEADER_FILE ${wxSETUP_HEADER_PATH}/wx/setup.h)

set(wxBUILD_FILE ${wxSETUP_HEADER_PATH}/build.cfg)
set(wxBUILD_FILE_DEBUG ${wxSETUP_HEADER_PATH}d/build.cfg)

if(DEFINED wxSETUP_HEADER_FILE_DEBUG)
    # Append configuration specific suffix to setup header path
    wx_string_append(wxSETUP_HEADER_PATH "$<$<CONFIG:Debug>:d>")
endif()

if(NOT wxBUILD_DEBUG_LEVEL STREQUAL "Default")
    add_compile_options("-DwxDEBUG_LEVEL=${wxBUILD_DEBUG_LEVEL}")
endif()

# Constants for setup.h creation
if(NOT wxUSE_EXPAT)
    set(wxUSE_XRC OFF)
endif()
set(wxUSE_XML ${wxUSE_XRC})

if(DEFINED wxUSE_OLE AND wxUSE_OLE)
    set(wxUSE_OLE_AUTOMATION ON)
endif()

if(wxUSE_ACTIVEX AND DEFINED wxUSE_OLE AND NOT wxUSE_OLE)
    message(WARNING "wxActiveXContainer requires wxUSE_OLE... disabled")
    wx_option_force_value(wxUSE_ACTIVEX OFF)
endif()

if(wxUSE_DRAG_AND_DROP AND DEFINED wxUSE_OLE AND NOT wxUSE_OLE)
    message(WARNING "wxUSE_DRAG_AND_DROP requires wxUSE_OLE... disabled")
    wx_option_force_value(wxUSE_DRAG_AND_DROP OFF)
endif()

if(wxUSE_ACCESSIBILITY AND DEFINED wxUSE_OLE AND NOT wxUSE_OLE)
    message(WARNING "wxUSE_ACCESSIBILITY requires wxUSE_OLE... disabled")
    wx_option_force_value(wxUSE_ACCESSIBILITY OFF)
endif()

if(wxUSE_MEDIACTRL AND DEFINED wxUSE_ACTIVEX AND NOT wxUSE_ACTIVEX)
    message(WARNING "wxMediaCtl requires wxActiveXContainer... disabled")
    wx_option_force_value(wxUSE_MEDIACTRL OFF)
endif()

if(wxUSE_WEBVIEW AND wxUSE_WEBVIEW_IE AND DEFINED wxUSE_ACTIVEX AND NOT wxUSE_ACTIVEX)
    message(WARNING "wxWebViewIE requires wxActiveXContainer... disabled")
    wx_option_force_value(wxUSE_WEBVIEW_IE OFF)
endif()

if(wxUSE_OPENGL)
    set(wxUSE_GLCANVAS ON)
endif()

if(wxUSE_ARCHIVE_STREAMS AND NOT wxUSE_STREAMS)
    message(WARNING "wxArchive requires wxStreams... disabled")
    wx_option_force_value(wxUSE_ARCHIVE_STREAMS OFF)
endif()

if(wxUSE_ZIPSTREAM AND (NOT wxUSE_ARCHIVE_STREAMS OR NOT wxUSE_ZLIB))
    message(WARNING "wxZip requires wxArchive or wxZlib... disabled")
    wx_option_force_value(wxUSE_ZIPSTREAM OFF)
endif()

if(wxUSE_TARSTREAM AND NOT wxUSE_ARCHIVE_STREAMS)
    message(WARNING "wxTar requires wxArchive... disabled")
    wx_option_force_value(wxUSE_TARSTREAM OFF)
endif()

if(wxUSE_FILESYSTEM AND (NOT wxUSE_STREAMS OR (NOT wxUSE_FILE AND NOT wxUSE_FFILE)))
    message(WARNING "wxFileSystem requires wxStreams and wxFile or wxFFile... disabled")
    wx_option_force_value(wxUSE_FILESYSTEM OFF)
endif()

if(wxUSE_FS_ARCHIVE AND (NOT wxUSE_FILESYSTEM OR NOT wxUSE_ARCHIVE_STREAMS))
    message(WARNING "wxArchiveFSHandler requires wxArchive and wxFileSystem... disabled")
    wx_option_force_value(wxUSE_FS_ARCHIVE OFF)
endif()

if(wxUSE_FS_ARCHIVE AND (NOT wxUSE_FILESYSTEM OR NOT wxUSE_ARCHIVE_STREAMS))
    message(WARNING "wxArchiveFSHandler requires wxArchive and wxFileSystem... disabled")
    wx_option_force_value(wxUSE_FS_ARCHIVE OFF)
endif()

if(wxUSE_FS_ZIP AND NOT wxUSE_FS_ARCHIVE)
    message(WARNING "wxZipFSHandler requires wxArchiveFSHandler... disabled")
    wx_option_force_value(wxUSE_FS_ZIP OFF)
endif()

if(wxUSE_TEXTFILE AND (NOT wxUSE_FILE OR NOT wxUSE_TEXTBUFFER))
    message(WARNING "wxTextFile requires wxFile and wxTextBuffer... disabled")
    wx_option_force_value(wxUSE_TEXTFILE OFF)
endif()

if(wxUSE_MIMETYPE AND NOT wxUSE_TEXTFILE)
    message(WARNING "wxUSE_MIMETYPE requires wxTextFile... disabled")
    wx_option_force_value(wxUSE_MIMETYPE OFF)
endif()

if(wxUSE_CONFIG)
    if(NOT wxUSE_TEXTFILE)
        message(WARNING "wxConfig requires wxTextFile... disabled")
        wx_option_force_value(wxUSE_CONFIG OFF)
    else()
        set(wxUSE_CONFIG_NATIVE ON)
    endif()
endif()

if(wxUSE_INTL AND NOT wxUSE_FILE)
    message(WARNING "I18n code requires wxFile... disabled")
    wx_option_force_value(wxUSE_INTL OFF)
endif()

if(wxUSE_THREADS)
    if(ANDROID)
        # Android has pthreads but FindThreads fails due to missing pthread_cancel
        set(CMAKE_USE_PTHREADS_INIT 1)
        set(CMAKE_THREAD_LIBS_INIT "")
        set(Threads_FOUND TRUE)
    else()
        find_package(Threads REQUIRED)
    endif()
endif()

if(wxUSE_LIBLZMA)
    find_package(LibLZMA)
    if(NOT LIBLZMA_FOUND)
        message(WARNING "libLZMA not found, LZMA compression won't be available")
        wx_option_force_value(wxUSE_LIBLZMA OFF)
    endif()
endif()

if (wxUSE_WEBREQUEST)
    if(wxUSE_WEBREQUEST_CURL)
        find_package(CURL)
        if(NOT CURL_FOUND)
            message(WARNING "CURL not found, wxWebSessionBackendCURL won't be available")
            wx_option_force_value(wxUSE_WEBREQUEST_CURL OFF)
        endif()
    endif()

    include(CheckCSourceCompiles)
    if(wxUSE_WEBREQUEST_WINHTTP)
        check_c_source_compiles("#include <windows.h>
                                 #include <winhttp.h>
                                 int main(){return 0;}"
                                HAVE_WINHTTP_H)
        if(NOT HAVE_WINHTTP_H)
            message(WARNING "winhttp.h not found, wxWebSessionBackendWinHTTP won't be available")
            wx_option_force_value(wxUSE_WEBREQUEST_WINHTTP OFF)
        endif()
    endif()

    if (NOT(wxUSE_WEBREQUEST_WINHTTP OR wxUSE_WEBREQUEST_URLSESSION OR wxUSE_WEBREQUEST_CURL))
        message(WARNING "wxUSE_WEBREQUEST requires at least one backend, it won't be available")
        wx_option_force_value(wxUSE_WEBREQUEST OFF)
    endif()
endif()

if(UNIX)
    if(wxUSE_SECRETSTORE AND NOT APPLE)
        # The required APIs are always available under MSW and OS X but we must
        # have GNOME libsecret under Unix to be able to compile this class.
        find_package(LIBSECRET)
        if(NOT LIBSECRET_FOUND)
            if(wxUSE_SECRETSTORE STREQUAL ON)
                message(FATAL_ERROR "wxSecretStore support requested, but libsecret was not found: either install it or don't set wxUSE_SECRETSTORE to ON")
            endif()

            # wxUSE_SECRETSTORE must be AUTO, continue with a warning.
            message(WARNING "libsecret not found, wxSecretStore won't be available")
            wx_option_force_value(wxUSE_SECRETSTORE OFF)
        endif()
    endif()

    if(wxUSE_LIBICONV)
        find_package(ICONV)
        if(NOT ICONV_FOUND)
            message(WARNING "iconv not found")
            wx_option_force_value(wxUSE_LIBICONV OFF)
        endif()
    endif()

    if(wxBUILD_LARGEFILE_SUPPORT)
        set(HAVE_LARGEFILE_SUPPORT ON)
    endif()
endif(UNIX)

if(wxUSE_GUI)
    if(WXMSW AND wxUSE_METAFILE)
        # this one should probably be made separately configurable
        set(wxUSE_ENH_METAFILE ON)
    endif()

    # Direct2D check
    if(WIN32 AND wxUSE_GRAPHICS_DIRECT2D)
        check_include_file(d2d1.h HAVE_D2D1_H)
        if (NOT HAVE_D2D1_H)
            wx_option_force_value(wxUSE_GRAPHICS_DIRECT2D OFF)
        endif()
    endif()
     if(MSVC) # match setup.h
        wx_option_force_value(wxUSE_GRAPHICS_DIRECT2D ${wxUSE_GRAPHICS_CONTEXT})
     endif()

    # WXQT checks
    if(WXQT)
        wx_option_force_value(wxUSE_WEBVIEW OFF)
        wx_option_force_value(wxUSE_METAFILE OFF)
        if(WIN32)
            wx_option_force_value(wxUSE_ACCESSIBILITY OFF)
            wx_option_force_value(wxUSE_OWNER_DRAWN OFF)
        endif()
    endif()

    # WXGTK checks, match include/wx/gtk/chkconf.h
    if(WXGTK)
        wx_option_force_value(wxUSE_METAFILE OFF)

        if(WIN32)
            wx_option_force_value(wxUSE_CAIRO ON)
            wx_option_force_value(wxUSE_ACCESSIBILITY OFF)
            wx_option_force_value(wxUSE_OWNER_DRAWN OFF)
        endif()

        if(NOT UNIX)
            wx_option_force_value(wxUSE_WEBVIEW OFF)
            wx_option_force_value(wxUSE_MEDIACTRL OFF)
            wx_option_force_value(wxUSE_UIACTIONSIMULATOR OFF)
            wx_option_force_value(wxUSE_OPENGL OFF)
            set(wxUSE_GLCANVAS OFF)
        endif()
    endif()

    # extra dependencies
    if(wxUSE_OPENGL)
        if(WXOSX_IPHONE)
            set(OPENGL_FOUND TRUE)
            set(OPENGL_INCLUDE_DIR "")
            set(OPENGL_LIBRARIES "-framework OpenGLES" "-framework QuartzCore" "-framework GLKit")
        else()
            find_package(OpenGL)
            if(OPENGL_FOUND)
                foreach(gltarget OpenGL::GL OpenGL::OpenGL)
                    if(TARGET ${gltarget})
                        set(OPENGL_LIBRARIES ${gltarget} ${OPENGL_LIBRARIES})
                    endif()
                endforeach()
            endif()
            if(WXGTK3 AND OpenGL_EGL_FOUND AND wxUSE_GLCANVAS_EGL)
                if(TARGET OpenGL::EGL)
                    set(OPENGL_LIBRARIES OpenGL::EGL ${OPENGL_LIBRARIES})
                else()
                    # It's possible for OpenGL::EGL not to be set even when EGL
                    # is found, at least under Cygwin (see #23673), so use the
                    # library directly like this to avoid link problems.
                    set(OPENGL_LIBRARIES ${OPENGL_egl_LIBRARY} ${OPENGL_LIBRARIES})
                endif()
                set(OPENGL_INCLUDE_DIR ${OPENGL_INCLUDE_DIR} ${OPENGL_EGL_INCLUDE_DIRS})
                find_package(WAYLANDEGL)
                if(WAYLANDEGL_FOUND AND wxHAVE_GDK_WAYLAND)
                    list(APPEND OPENGL_LIBRARIES ${WAYLANDEGL_LIBRARIES})
                endif()
            endif()
        endif()
        if(NOT OPENGL_FOUND)
            message(WARNING "opengl not found, wxGLCanvas won't be available")
            wx_option_force_value(wxUSE_OPENGL OFF)
        endif()
        if(UNIX AND (NOT WXGTK3 OR NOT OpenGL_EGL_FOUND))
            wx_option_force_value(wxUSE_GLCANVAS_EGL OFF)
        endif()
    endif()

    if(wxUSE_WEBVIEW)
        if(WXGTK)
            if(wxUSE_WEBVIEW_WEBKIT)
                set(WEBKIT_LIBSOUP_VERSION 2.4)
                if(WXGTK2)
                    find_package(WEBKIT 1.0)
                elseif(WXGTK3)
                    find_package(WEBKIT2 4.1 QUIET)
                    if(WEBKIT2_FOUND)
                        set(WEBKIT_LIBSOUP_VERSION 3.0)
                    else()
                        find_package(WEBKIT2 4.0)
                    endif()
                    if(NOT WEBKIT2_FOUND)
                        find_package(WEBKIT 3.0)
                    endif()
                endif()
                find_package(LIBSOUP ${WEBKIT_LIBSOUP_VERSION})
            endif()
            set(wxUSE_WEBVIEW_WEBKIT OFF)
            set(wxUSE_WEBVIEW_WEBKIT2 OFF)
            if(WEBKIT_FOUND AND LIBSOUP_FOUND)
                set(wxUSE_WEBVIEW_WEBKIT ON)
            elseif(WEBKIT2_FOUND AND LIBSOUP_FOUND)
                set(wxUSE_WEBVIEW_WEBKIT2 ON)
            elseif(NOT wxUSE_WEBVIEW_CHROMIUM)
                message(WARNING "webkit or chromium not found or enabled, wxWebview won't be available")
                wx_option_force_value(wxUSE_WEBVIEW OFF)
            endif()
        elseif(APPLE)
            if(NOT wxUSE_WEBVIEW_WEBKIT AND NOT wxUSE_WEBVIEW_CHROMIUM)
                message(WARNING "webkit and chromium not found or enabled, wxWebview won't be available")
                wx_option_force_value(wxUSE_WEBVIEW OFF)
            endif()
        else()
            set(wxUSE_WEBVIEW_WEBKIT OFF)
        endif()

        if(WXMSW AND NOT wxUSE_WEBVIEW_IE AND NOT wxUSE_WEBVIEW_EDGE AND NOT wxUSE_WEBVIEW_CHROMIUM)
            message(WARNING "WebviewIE and WebviewEdge and WebviewChromium not found or enabled, wxWebview won't be available")
            wx_option_force_value(wxUSE_WEBVIEW OFF)
        endif()

        if(wxUSE_WEBVIEW_CHROMIUM AND WIN32 AND NOT MSVC)
            message(FATAL_ERROR "WebviewChromium libcef_dll_wrapper can only be built with MSVC")
        endif()

        if(wxUSE_WEBVIEW_CHROMIUM)
            # CEF requires C++17: we trust CMAKE_CXX_STANDARD if it is defined,
            # or the previously tested wxHAVE_CXX17 if the compiler supports C++17 anyway.
            if(NOT (CMAKE_CXX_STANDARD GREATER_EQUAL 17 OR wxHAVE_CXX17))
                # We shouldn't disable this option as it's disabled by default and
                # if it is on, it means that CEF is meant to be used, but we can't
                # continue either as libcef_dll_wrapper will fail to build
                # (actually it may still succeed with CEF v116 which provided
                # its own stand-in for std::in_place used in CEF headers, but
                # not with the later versions, so just fail instead of trying
                # to detect CEF version here, as even v116 officially only
                # supports C++17 anyhow).
                if (DEFINED CMAKE_CXX_STANDARD)
                    set(cxx17_error_details "configured to use C++${CMAKE_CXX_STANDARD}")
                else()
                    set(cxx17_error_details "the compiler doesn't support C++17 by default and CMAKE_CXX_STANDARD is not set")
                endif()
                message(FATAL_ERROR "WebviewChromium requires at least C++17 but ${cxx17_error_details}")
            endif()
        endif()
    endif()

    set(wxWebviewInfo "enable wxWebview")
    if(wxUSE_WEBVIEW)
        if(wxUSE_WEBVIEW_WEBKIT)
            list(APPEND webviewBackends "WebKit")
        endif()
        if(wxUSE_WEBVIEW_WEBKIT2)
            list(APPEND webviewBackends "WebKit2")
        endif()
        if(wxUSE_WEBVIEW_EDGE)
            if(wxUSE_WEBVIEW_EDGE_STATIC)
                list(APPEND webviewBackends "Edge (static)")
            else()
                list(APPEND webviewBackends "Edge")
            endif()
        endif()
        if(wxUSE_WEBVIEW_IE)
            list(APPEND webviewBackends "IE")
        endif()
        if(wxUSE_WEBVIEW_CHROMIUM)
            list(APPEND webviewBackends "Chromium")
        endif()
        string(REPLACE ";" ", " webviewBackends "${webviewBackends}")
        set(wxWebviewInfo "${wxWebviewInfo} with ${webviewBackends}")
    endif()
    set(wxTHIRD_PARTY_LIBRARIES ${wxTHIRD_PARTY_LIBRARIES} wxUSE_WEBVIEW ${wxWebviewInfo})

    if(wxUSE_PRIVATE_FONTS AND WXGTK)
        find_package(FONTCONFIG)
        find_package(PANGOFT2)
        if(NOT FONTCONFIG_FOUND OR NOT PANGOFT2_FOUND)
            message(WARNING "Fontconfig or PangoFT2 not found, Private fonts won't be available")
            wx_option_force_value(wxUSE_PRIVATE_FONTS OFF)
        endif()
    endif()

    if(wxUSE_MEDIACTRL AND WXGTK AND NOT APPLE AND NOT WIN32)
        find_package(GSTREAMER 1.0 COMPONENTS video)
        if(NOT GSTREAMER_FOUND)
            find_package(GSTREAMER 0.10 COMPONENTS interfaces)
        endif()

        set(wxUSE_GSTREAMER ${GSTREAMER_FOUND})
        set(wxUSE_GSTREAMER_PLAYER OFF)
        if(GSTREAMER_PLAYER_INCLUDE_DIRS)
            set(wxUSE_GSTREAMER_PLAYER ON)
        endif()

        if(NOT GSTREAMER_FOUND)
            message(WARNING "GStreamer not found, wxMediaCtrl won't be available")
            wx_option_force_value(wxUSE_MEDIACTRL OFF)
        endif()
    else()
        set(wxUSE_GSTREAMER OFF)
        set(wxUSE_GSTREAMER_PLAYER OFF)
    endif()

    if(wxUSE_SOUND AND wxUSE_LIBSDL AND UNIX AND NOT APPLE)
        find_package(SDL2)
        if(NOT SDL2_FOUND)
            find_package(SDL)
        endif()
        if(NOT SDL2_FOUND AND NOT SDL_FOUND)
            message(WARNING "SDL not found, SDL Audio back-end won't be available")
            wx_option_force_value(wxUSE_LIBSDL OFF)
        endif()
    else()
        set(wxUSE_LIBSDL OFF)
    endif()

    if(wxUSE_NOTIFICATION_MESSAGE AND UNIX AND WXGTK AND wxUSE_LIBNOTIFY)
        find_package(LIBNOTIFY)
        if(NOT LIBNOTIFY_FOUND)
            message(WARNING "Libnotify not found, it won't be used for notifications")
            wx_option_force_value(wxUSE_LIBNOTIFY OFF)
        else()
            if(NOT LIBNOTIFY_VERSION VERSION_LESS 0.7)
                set(wxUSE_LIBNOTIFY_0_7 ON)
            endif()
            list(APPEND wxTOOLKIT_EXTRA "libnotify")
        endif()
    else()
        set(wxUSE_LIBNOTIFY OFF)
    endif()

    if(wxUSE_UIACTIONSIMULATOR AND UNIX AND WXGTK)
        if(wxUSE_XTEST)
            find_package(XTEST)
            if(XTEST_FOUND)
                list(APPEND wxTOOLKIT_INCLUDE_DIRS ${XTEST_INCLUDE_DIRS})
                list(APPEND wxTOOLKIT_LIBRARIES ${XTEST_LIBRARIES})
            else()
                if(WXGTK3)
                    # This class can't work without XTest with GTK+ 3
                    # which uses XInput2 and so ignores XSendEvent().
                    message(STATUS "XTest not found, wxUIActionSimulator won't be available")
                    wx_option_force_value(wxUSE_UIACTIONSIMULATOR OFF)
                endif()
                # The other ports can use XSendEvent(), so don't warn
                wx_option_force_value(wxUSE_XTEST OFF)
            endif()
        else(WXGTK3)
            # As per above, wxUIActionSimulator can't be used in this case,
            # but there is no need to warn, presumably the user knows what
            # he's doing if wxUSE_XTEST was explicitly disabled.
            wx_option_force_value(wxUSE_UIACTIONSIMULATOR OFF)
        endif()
    endif()

    if(wxUSE_HTML AND UNIX AND wxUSE_LIBMSPACK)
        find_package(MSPACK)
        if(NOT MSPACK_FOUND)
            message(STATUS "libmspack not found")
            wx_option_force_value(wxUSE_LIBMSPACK OFF)
        endif()
    else()
        set(wxUSE_LIBMSPACK OFF)
    endif()

    if(WXGTK AND wxUSE_PRINTING_ARCHITECTURE AND wxUSE_GTKPRINT)
        find_package(GTKPRINT ${wxTOOLKIT_VERSION})
        if(GTKPRINT_FOUND)
            list(APPEND wxTOOLKIT_INCLUDE_DIRS ${GTKPRINT_INCLUDE_DIRS})
            list(APPEND wxTOOLKIT_EXTRA "GTK+ printing")
        else()
            message(STATUS "GTK printing support not found (GTK+ >= 2.10), library will use GNOME printing support or standard PostScript printing")
            wx_option_force_value(wxUSE_GTKPRINT OFF)
        endif()
    else()
        set(wxUSE_GTKPRINT OFF)
    endif()

    if(WXGTK AND wxUSE_MIMETYPE AND wxUSE_LIBGNOMEVFS)
        find_package(GNOMEVFS2)
        if(GNOMEVFS2_FOUND)
            list(APPEND wxTOOLKIT_INCLUDE_DIRS ${GNOMEVFS2_INCLUDE_DIRS})
            list(APPEND wxTOOLKIT_LIBRARIES ${GNOMEVFS2_LIBRARIES})
            list(APPEND wxTOOLKIT_EXTRA "gnomevfs")
        else()
            message(STATUS "libgnomevfs not found, library won't be used to associate MIME type")
            wx_option_force_value(wxUSE_LIBGNOMEVFS OFF)
        endif()
    else()
        set(wxUSE_LIBGNOMEVFS OFF)
    endif()

    if(WXGTK3 AND wxUSE_SPELLCHECK)
        find_package(GSPELL)
        if(GSPELL_FOUND)
            list(APPEND wxTOOLKIT_INCLUDE_DIRS ${GSPELL_INCLUDE_DIRS})
            list(APPEND wxTOOLKIT_LIBRARIES ${GSPELL_LIBRARIES})
        else()
            message(STATUS "gspell-1 not found, spell checking in wxTextCtrl won't be available")
            wx_option_force_value(wxUSE_SPELLCHECK OFF)
        endif()
    endif()

    if(wxUSE_CAIRO AND NOT WXGTK)
        find_package(Cairo)
        if(NOT CAIRO_FOUND)
            message(WARNING "Cairo not found, Cairo renderer won't be available")
            wx_option_force_value(wxUSE_CAIRO OFF)
        endif()
    endif()

    if(WXGTK AND NOT APPLE AND NOT WIN32)
        find_package(XKBCommon)
        if(XKBCOMMON_FOUND)
            list(APPEND wxTOOLKIT_INCLUDE_DIRS ${XKBCOMMON_INCLUDE_DIRS})
            list(APPEND wxTOOLKIT_LIBRARIES ${XKBCOMMON_LIBRARIES})
            set(HAVE_XKBCOMMON ON)
        else()
            message(STATUS "libxkbcommon not found, key codes in key events may be incorrect")
        endif()
    endif()

endif(wxUSE_GUI)

# test if precompiled headers are supported using the cotire test project
if(DEFINED wxBUILD_PRECOMP_PREV AND NOT wxBUILD_PRECOMP STREQUAL wxBUILD_PRECOMP_PREV)
    set(CLEAN_PRECOMP_TEST TRUE)
endif()
set(wxBUILD_PRECOMP_PREV ${wxBUILD_PRECOMP} CACHE INTERNAL "")

if((wxBUILD_PRECOMP STREQUAL "ON" AND CMAKE_VERSION VERSION_LESS "3.16") OR (wxBUILD_PRECOMP STREQUAL "COTIRE"))
    if(DEFINED CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED)
        set(try_flags "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=${CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED}")
    endif()
    if (CLEAN_PRECOMP_TEST)
        try_compile(RESULT_VAR_CLEAN
                    "${wxBINARY_DIR}/CMakeFiles/cotire_test"
                    "${wxSOURCE_DIR}/build/cmake/modules/cotire_test"
                    CotireExample clean_cotire
                    CMAKE_FLAGS ${try_flags}
        )
    endif()
    try_compile(RESULT_VAR
                "${wxBINARY_DIR}/CMakeFiles/cotire_test"
                "${wxSOURCE_DIR}/build/cmake/modules/cotire_test"
                CotireExample
                CMAKE_FLAGS ${try_flags}
                OUTPUT_VARIABLE OUTPUT_VAR
    )

    # check if output has precompiled header warnings. The build can still succeed, so check the output
    # likely caused by gcc hardening: https://bugzilla.redhat.com/show_bug.cgi?id=1721553
    # cc1plus: warning /path/to/project/cotire/name_CXX_prefix.hxx.gch: had text segment at different address
    string(FIND "${OUTPUT_VAR}" "had text segment at different address" HAS_MESSAGE)
    if(${HAS_MESSAGE} GREATER -1)
        set(RESULT_VAR FALSE)
    endif()

    if(NOT RESULT_VAR)
        message(WARNING "precompiled header (PCH) test failed, it will be turned off")
        wx_option_force_value(wxBUILD_PRECOMP OFF)
    else()
        set(USE_COTIRE ON)
    endif()
endif()
