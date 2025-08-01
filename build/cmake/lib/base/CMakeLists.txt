#############################################################################
# Name:        build/cmake/lib/base/CMakeLists.txt
# Purpose:     CMake file for base library
# Author:      Tobias Taschner
# Created:     2016-09-20
# Copyright:   (c) 2016 wxWidgets development team
# Licence:     wxWindows licence
#############################################################################

wx_append_sources(BASE_FILES BASE_CMN)
wx_append_sources(BASE_FILES BASE_AND_GUI_CMN)

if(WIN32)
    wx_append_sources(BASE_FILES BASE_WIN32)
    wx_append_sources(BASE_FILES BASE_AND_GUI_WIN32)
elseif(APPLE)
    if(WXOSX_COCOA OR WXOSX_IPHONE)
        wx_append_sources(BASE_FILES BASE_OSX_SHARED)
        if(WXOSX_COCOA)
            wx_append_sources(BASE_FILES BASE_AND_GUI_OSX_COCOA)
        endif()
    else()
        wx_append_sources(BASE_FILES BASE_OSX_NOTWXMAC)
    endif()
elseif(UNIX)
    wx_append_sources(BASE_FILES BASE_UNIX)
endif()

wx_add_library(wxbase IS_BASE ${BASE_FILES})

# Note that when using built-in versions of the libraries, their include
# directories must come before the system ones which could be used by some
# other dependency.
if(wxUSE_ZLIB)
    wx_lib_include_directories_before(wxbase ${ZLIB_INCLUDE_DIRS})
    wx_lib_link_libraries(wxbase PRIVATE ${ZLIB_LIBRARIES})
endif()
if(wxUSE_REGEX)
    wx_lib_include_directories_before(wxbase ${REGEX_INCLUDE_DIRS})
    wx_lib_link_libraries(wxbase PRIVATE ${REGEX_LIBRARIES})
endif()
if(wxUSE_LIBLZMA)
    wx_lib_include_directories(wxbase ${LIBLZMA_INCLUDE_DIRS})
    wx_lib_link_libraries(wxbase PRIVATE ${LIBLZMA_LIBRARIES})
endif()
if(UNIX AND wxUSE_SECRETSTORE)
    wx_lib_include_directories(wxbase ${LIBSECRET_INCLUDE_DIRS})
    # Avoid linking with libsecret-1.so directly, we load this
    # library dynamically in wxSecretStoreLibSecretImpl to avoid
    # requiring it being installed on the target system.
    list(REMOVE_ITEM LIBSECRET_LIBRARIES secret-1)
    wx_lib_link_libraries(wxbase PRIVATE ${LIBSECRET_LIBRARIES})
endif()
if(wxUSE_LIBICONV)
    wx_lib_include_directories(wxbase ${ICONV_INCLUDE_DIR})
    wx_lib_link_libraries(wxbase PRIVATE ${ICONV_LIBRARIES})
endif()

if(APPLE)
    wx_lib_link_libraries(wxbase
        PUBLIC
        "-framework CoreFoundation"
    )
    if(WXOSX_COCOA)
        wx_lib_link_libraries(wxbase
            PRIVATE
            "-framework Security"
            PUBLIC
            "-framework Carbon"
            "-framework Cocoa"
            "-framework IOKit"
            "-framework QuartzCore"
        )
    endif()
elseif(UNIX)
    wx_lib_link_libraries(wxbase PRIVATE ${CMAKE_DL_LIBS})
endif()
