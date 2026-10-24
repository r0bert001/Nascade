cmake_minimum_required(VERSION 3.16)
project(Nascade VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(NASCADE_CROSS "Cross-compile for TrimUI tg5040" OFF)
set(NASCADE_SYSROOT "" CACHE PATH "Target sysroot (SDK_usr_tg5040_a133p/usr parent)")
set(NASCADE_DEPS "" CACHE PATH "Cross-compiled dependency prefix (curl, smb2)")

if (NASCADE_CROSS)
    if (NOT NASCADE_SYSROOT OR NOT NASCADE_DEPS)
        message(FATAL_ERROR "NASCADE_SYSROOT and NASCADE_DEPS are required for cross builds")
    endif()
    set(SDL2_INCLUDE_DIRS "${NASCADE_SYSROOT}/usr/include/SDL2")
    set(SDL2_LIBRARIES SDL2)
    set(SDL2_LIBRARY_DIRS "${NASCADE_SYSROOT}/usr/lib")

    set(SDL2_TTF_INCLUDE_DIRS
        "${NASCADE_SYSROOT}/usr/include/SDL2"
        "${NASCADE_SYSROOT}/usr/include/freetype2")
    set(SDL2_TTF_LIBRARIES SDL2_ttf freetype)
    set(SDL2_TTF_LIBRARY_DIRS "${NASCADE_SYSROOT}/usr/lib")

    set(SQLITE3_INCLUDE_DIRS "${NASCADE_SYSROOT}/usr/include")
    set(SQLITE3_LIBRARIES "${NASCADE_SYSROOT}/usr/lib/libsqlite3.a")
    set(SQLITE3_LIBRARY_DIRS "${NASCADE_SYSROOT}/usr/lib")

    set(CURL_INCLUDE_DIRS "${NASCADE_DEPS}/include")
    set(CURL_LIBRARIES
        "${NASCADE_DEPS}/lib/libcurl.a"
        "${NASCADE_SYSROOT}/usr/lib/libssl.a"
        "${NASCADE_SYSROOT}/usr/lib/libcrypto.a"
        "${NASCADE_SYSROOT}/usr/lib/libz.a"
        dl pthread)
    set(CURL_LIBRARY_DIRS "${NASCADE_DEPS}/lib" "${NASCADE_SYSROOT}/usr/lib")
    add_compile_definitions(_REENTRANT)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)

    # We provide main() ourselves (SDL_MAIN_HANDLED), so SDL2main is not needed.
    list(FILTER SDL2_LIBRARIES EXCLUDE REGEX "SDL2main")
endif()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(nascade
    src/main.cpp
    src/app/App.cpp
    src/core/Config.cpp
    src/core/I18n.cpp
    src/core/Log.cpp
    src/core/Paths.cpp
    src/core/Util.cpp
    src/scan/GameScanner.cpp
    src/scan/PlatformScanner.cpp
    src/scan/RomFiles.cpp
    src/scrape/HttpClient.cpp
    src/scrape/LibretroScraper.cpp
    src/scrape/ScreenScraper.cpp
    src/storage/Database.cpp
    src/transfer/Downloader.cpp
    src/ui/FontCache.cpp
    src/ui/ImageCache.cpp
    src/ui/Input.cpp
    src/ui/Painter.cpp
    src/vfs/LocalVfs.cpp
    src/vfs/VfsFactory.cpp
)

target_include_directories(nascade PRIVATE
    src
    third_party
    third_party/stb
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)
target_link_directories(nascade PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${SQLITE3_LIBRARY_DIRS}
)
target_link_libraries(nascade PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${CURL_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ZLIB::ZLIB
    Threads::Threads
)

# libsmb2 is optional for desktop-only builds. Point these variables at a
# libsmb2 build/install tree to enable the SMB backend.
set(NASCADE_LIBSMB2_INCLUDE_DIR "" CACHE PATH "libsmb2 include root (contains libsmb2.h)")
set(NASCADE_LIBSMB2_BUILD_INCLUDE_DIR "" CACHE PATH "libsmb2 generated config.h directory")
set(NASCADE_LIBSMB2_LIBRARY "" CACHE FILEPATH "libsmb2 static/shared library")
if (NASCADE_LIBSMB2_INCLUDE_DIR AND NASCADE_LIBSMB2_LIBRARY)
    target_sources(nascade PRIVATE src/vfs/SmbVfs.cpp)
    target_include_directories(nascade PRIVATE
        ${NASCADE_LIBSMB2_INCLUDE_DIR}
        ${NASCADE_LIBSMB2_INCLUDE_DIR}/smb2
    )
    if (NASCADE_LIBSMB2_BUILD_INCLUDE_DIR)
        target_include_directories(nascade PRIVATE ${NASCADE_LIBSMB2_BUILD_INCLUDE_DIR})
    endif()
    target_link_libraries(nascade PRIVATE ${NASCADE_LIBSMB2_LIBRARY})
    target_compile_definitions(nascade PRIVATE NASCADE_HAVE_LIBSMB2=1)
    if (WIN32)
        target_link_libraries(nascade PRIVATE ws2_32)
    endif()
endif()

if (MSVC)
    target_compile_options(nascade PRIVATE /W4 /utf-8)
else()
    target_compile_options(nascade PRIVATE -Wall -Wextra -Wpedantic)
endif()
