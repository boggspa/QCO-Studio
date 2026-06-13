cmake_minimum_required(VERSION 3.24)

project(QCOStudio
  VERSION 0.1.0
  DESCRIPTION "A cross-platform professional image editor foundation"
  LANGUAGES CXX
)

option(QCO_STUDIO_BUILD_APP "Build the Qt desktop application" ON)
option(QCO_STUDIO_BUILD_TESTS "Build tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(qcostudio_core
  src/core/Document.cpp
  src/core/UndoStack.cpp
)

target_include_directories(qcostudio_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(qcostudio_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(qcostudio_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(qcostudio_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(QCO_STUDIO_BUILD_APP)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

  qt_standard_project_setup()

  add_library(qcostudio_qt_support
    src/ui/ProjectArchive.cpp
    src/ui/ProjectArchive.h
  )

  target_link_libraries(qcostudio_qt_support
    PUBLIC
      qcostudio_core
      Qt6::Core
      Qt6::Gui
  )

  target_include_directories(qcostudio_qt_support
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_compile_features(qcostudio_qt_support PUBLIC cxx_std_20)

  if(MSVC)
    target_compile_options(qcostudio_qt_support PRIVATE /W4 /permissive-)
  else()
    target_compile_options(qcostudio_qt_support PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  qt_add_executable(qcostudio
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

  target_link_libraries(qcostudio
    PRIVATE
      qcostudio_core
      qcostudio_qt_support
      Qt6::Core
      Qt6::Gui
      Qt6::Widgets
  )

  target_compile_features(qcostudio PRIVATE cxx_std_20)

  set_target_properties(qcostudio PROPERTIES
    OUTPUT_NAME "QCO Studio"
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.qcostudio.desktop"
    MACOSX_BUNDLE_BUNDLE_NAME "QCO Studio"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  )

  if(MSVC)
    target_compile_options(qcostudio PRIVATE /W4 /permissive-)
  else()
    target_compile_options(qcostudio PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if(QCO_STUDIO_BUILD_TESTS)
    qt_add_executable(qcostudio_project_archive_tests
      tests/project_archive_tests.cpp
    )

    target_link_libraries(qcostudio_project_archive_tests
      PRIVATE
        qcostudio_qt_support
        Qt6::Core
        Qt6::Gui
    )

    add_test(NAME qcostudio_project_archive_tests COMMAND qcostudio_project_archive_tests)

    qt_add_executable(qcostudio_canvas_smoke_tests
      tests/canvas_smoke_tests.cpp
      src/ui/CanvasView.cpp
      src/ui/CanvasView.h
    )

    target_include_directories(qcostudio_canvas_smoke_tests
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(qcostudio_canvas_smoke_tests
      PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )

    add_test(NAME qcostudio_canvas_smoke_tests COMMAND qcostudio_canvas_smoke_tests)
  endif()
endif()

if(QCO_STUDIO_BUILD_TESTS)
  enable_testing()

  add_executable(qcostudio_core_tests
    tests/core_tests.cpp
  )

  target_link_libraries(qcostudio_core_tests PRIVATE qcostudio_core)
  target_compile_features(qcostudio_core_tests PRIVATE cxx_std_20)

  add_test(NAME qcostudio_core_tests COMMAND qcostudio_core_tests)
endif()
