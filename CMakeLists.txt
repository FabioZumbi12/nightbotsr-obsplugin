#  Nightbot SR Plugin for OBS - CMake Build Configuration
#  This CMake configuration file sets up the build process.
#  It includes options for enabling the obs-frontend-api and Qt functionalities,
#  and specifies custom output directories for the built plugin and its associated data.
#
#  If the build fail (the folder build_x64 don't exists), run on terminal: cmake --preset windows-x64
#
#  For buils on github, push tags:
#  - git tag v1.0.0
#  - git push origin --tags
cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_INNO_SETUP "Build installer using Inno Setup" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    SIMDE_DISABLE_NATIVE_ALIASES
    SIMDE_NO_NATIVE
)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Network Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# https://files.jrsoftware.org/is/6/innosetup-6.6.0.exe
# Encontra o compilador do Inno Setup (opcional)
set(SETUP_ROOT "${CMAKE_SOURCE_DIR}/setup")
set(INNO_SETUP_COMPILER "C:/Program Files (x86)/Inno Setup 6/ISCC.exe")

if(ENABLE_INNO_SETUP AND EXISTS "${INNO_SETUP_COMPILER}")
        add_custom_command(TARGET ${_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Executando Inno Setup como $<CONFIG>..."
        COMMAND "${INNO_SETUP_COMPILER}" "${CMAKE_SOURCE_DIR}/setup.iss" "/DBuildConfig=$<CONFIG>"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Criando instalador ${_name}-Setup.exe (opcional)..."
    )
else()
    message(STATUS "Inno Setup desabilitado ou nao encontrado. Ignorando criacao do instalador.")
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp
          src/nightbot-auth.cpp
          src/nightbot-api.cpp
          src/nightbot-dock.cpp
          src/nightbot-settings.cpp
          src/SettingsManager.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

install(
  TARGETS ${CMAKE_PROJECT_NAME}
  RUNTIME DESTINATION "obs-plugins/64bit"
  LIBRARY DESTINATION "obs-plugins/64bit"
)

install(DIRECTORY data/ DESTINATION "data/obs-plugins/${_name}")
