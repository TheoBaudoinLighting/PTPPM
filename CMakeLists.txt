cmake_minimum_required(VERSION 3.15)
project(PTPPM VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options de compilation
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Répertoire des dépendances
set(DEPS_DIR "${CMAKE_BINARY_DIR}/deps")
file(MAKE_DIRECTORY ${DEPS_DIR})

include(FetchContent)

# ========== [GLAD] ==========
FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

# ========== [GLFW] ==========
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# Ajouter des options de compilation pour GLFW
set_target_properties(glfw PROPERTIES C_STANDARD 99)
target_compile_options(glfw PRIVATE -Wno-cast-function-type -Wno-pedantic)

# ========== [IMGUI] ==========
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking  # Utiliser la branche docking pour ImGui
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${glfw_SOURCE_DIR}/include
)
target_link_libraries(imgui PUBLIC glfw glad)

# ========== [SPDLOG] ==========
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# ========== [NLOHMANN JSON] ==========
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# ========== [SQLite3] ==========
set(SQLITE_DIR "${DEPS_DIR}/sqlite")
file(MAKE_DIRECTORY ${SQLITE_DIR})
if(NOT EXISTS "${SQLITE_DIR}/sqlite3.c")
    message(STATUS "Téléchargement de SQLite3...")
    file(DOWNLOAD
            "https://www.sqlite.org/2024/sqlite-amalgamation-3450200.zip"
            "${CMAKE_CURRENT_BINARY_DIR}/sqlite.zip"
            SHOW_PROGRESS
    )
    file(ARCHIVE_EXTRACT
            INPUT "${CMAKE_CURRENT_BINARY_DIR}/sqlite.zip"
            DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/sqlite-extract"
    )
    file(GLOB SQLITE_FILES
            "${CMAKE_CURRENT_BINARY_DIR}/sqlite-extract/sqlite-amalgamation-*/sqlite3.c"
            "${CMAKE_CURRENT_BINARY_DIR}/sqlite-extract/sqlite-amalgamation-*/sqlite3.h"
    )
    foreach(SQLITE_FILE ${SQLITE_FILES})
        get_filename_component(FILENAME ${SQLITE_FILE} NAME)
        file(COPY ${SQLITE_FILE} DESTINATION ${SQLITE_DIR})
    endforeach()
endif()
add_library(sqlite3_lib STATIC ${SQLITE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${SQLITE_DIR})

# ========== [ASIO] ==========
FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

# ========== [EXÉCUTABLE PRINCIPAL] ==========
file(GLOB_RECURSE SRC_FILES src/*.cpp)
file(GLOB_RECURSE UTILS_FILES src/utils/*.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX "src/utils/.*\\.cpp$")

add_executable(PTPPM ${SRC_FILES} ${UTILS_FILES})

target_include_directories(PTPPM PRIVATE
        ${ASIO_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${SQLITE_DIR}
)

target_link_libraries(PTPPM PRIVATE
        glfw
        glad
        imgui
        sqlite3_lib
        spdlog
        nlohmann_json
        ws2_32
        mswsock
)

# ========== [INSTALLATION & RESSOURCES] ==========
install(TARGETS PTPPM RUNTIME DESTINATION bin)

set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
if(EXISTS ${RESOURCES_DIR})
    add_custom_command(TARGET PTPPM POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RESOURCES_DIR} $<TARGET_FILE_DIR:PTPPM>/resources
    )
endif()

# ========== [MESSAGE DE FIN] ==========
add_custom_command(TARGET PTPPM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
        "==============================================="
        "Compilation terminée avec succès!"
        "L'exécutable PTPPM se trouve dans le répertoire:"
        "$<TARGET_FILE_DIR:PTPPM>"
        "==============================================="
)
