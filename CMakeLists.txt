cmake_minimum_required(VERSION 3.24)

project(QimageMax
  VERSION 0.1.0
  DESCRIPTION "A cross-platform professional image editor foundation"
  LANGUAGES CXX
)

option(QIMAGEMAX_BUILD_APP "Build the Qt desktop application" ON)
option(QIMAGEMAX_BUILD_TESTS "Build tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(qimagemax_core
  src/core/Document.cpp
  src/core/UndoStack.cpp
)

target_include_directories(qimagemax_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(qimagemax_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(qimagemax_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(qimagemax_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(QIMAGEMAX_BUILD_APP)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

  qt_standard_project_setup()

  add_library(qimagemax_qt_support
    src/ui/ProjectArchive.cpp
    src/ui/ProjectArchive.h
  )

  target_link_libraries(qimagemax_qt_support
    PUBLIC
      qimagemax_core
      Qt6::Core
      Qt6::Gui
  )

  target_include_directories(qimagemax_qt_support
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_compile_features(qimagemax_qt_support PUBLIC cxx_std_20)

  if(MSVC)
    target_compile_options(qimagemax_qt_support PRIVATE /W4 /permissive-)
  else()
    target_compile_options(qimagemax_qt_support PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  qt_add_executable(qimagemax
    MACOSX_BUNDLE
    WIN32
    src/app/Logging.cpp
    src/app/Logging.h
    src/app/main.cpp
    src/ui/CanvasView.cpp
    src/ui/CanvasView.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
  )

  target_link_libraries(qimagemax
    PRIVATE
      qimagemax_core
      qimagemax_qt_support
      Qt6::Core
      Qt6::Gui
      Qt6::Widgets
  )

  target_compile_features(qimagemax PRIVATE cxx_std_20)

  set_target_properties(qimagemax PROPERTIES
    OUTPUT_NAME "Qimage Max"
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.qimagemax.desktop"
    MACOSX_BUNDLE_BUNDLE_NAME "Qimage Max"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  )

  if(MSVC)
    target_compile_options(qimagemax PRIVATE /W4 /permissive-)
  else()
    target_compile_options(qimagemax PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if(QIMAGEMAX_BUILD_TESTS)
    qt_add_executable(qimagemax_project_archive_tests
      tests/project_archive_tests.cpp
    )

    target_link_libraries(qimagemax_project_archive_tests
      PRIVATE
        qimagemax_qt_support
        Qt6::Core
        Qt6::Gui
    )

    add_test(NAME qimagemax_project_archive_tests COMMAND qimagemax_project_archive_tests)
  endif()
endif()

if(QIMAGEMAX_BUILD_TESTS)
  enable_testing()

  add_executable(qimagemax_core_tests
    tests/core_tests.cpp
  )

  target_link_libraries(qimagemax_core_tests PRIVATE qimagemax_core)
  target_compile_features(qimagemax_core_tests PRIVATE cxx_std_20)

  add_test(NAME qimagemax_core_tests COMMAND qimagemax_core_tests)
endif()
