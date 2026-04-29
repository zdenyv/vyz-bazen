# Auto-generate version from git tags (e.g. v1.5.2 → "1.5.2", v1.5.2-3-gabcdef → "1.5.2-3")
execute_process(
    COMMAND git describe --tags --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
string(REGEX REPLACE "^v" "" FW_VERSION "${GIT_DESCRIBE}")
string(REGEX REPLACE "-g[0-9a-f]+$" "" FW_VERSION "${FW_VERSION}")
message(STATUS "Firmware version: ${FW_VERSION}")
configure_file(version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h @ONLY)

# List any additional sources here
target_sources(
    ${CMAKE_PROJECT_NAME}
    PUBLIC
    application.c
    zdeny_relay.c
    )

# If you added some folder with header files you need to list them here
target_include_directories(
    ${CMAKE_PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
