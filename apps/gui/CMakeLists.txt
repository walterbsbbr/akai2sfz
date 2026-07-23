set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
  add_executable(WJ-VSC MACOSX_BUNDLE
      main.cpp
      MainWindow.cpp
      MainWindow.h
      icons.qrc
      WJ-VSC.icns
  )
  set_source_files_properties(WJ-VSC.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  set_target_properties(WJ-VSC PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
      MACOSX_BUNDLE_BUNDLE_NAME "WJ-VSC"
      MACOSX_BUNDLE_GUI_IDENTIFIER "app.wjvsc.gui"
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_ICON_FILE "WJ-VSC.icns"
  )
else()
  add_executable(WJ-VSC
      main.cpp
      MainWindow.cpp
      MainWindow.h
      icons.qrc
  )
endif()

target_link_libraries(WJ-VSC PRIVATE akai2sfz_core Qt6::Widgets)

install(TARGETS WJ-VSC
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# `cmake --build build --target package_gui` -- deixa o .app autonomo
# (bundla Qt e libsndfile) e gera um .dmg ao lado. So faz sentido no macOS
# e com a GUI habilitada; ver README.
if(APPLE)
  find_program(MACDEPLOYQT_EXECUTABLE macdeployqt6 macdeployqt
      HINTS ${Qt6_DIR}/../../../bin /opt/homebrew/opt/qt/bin /opt/homebrew/bin
  )
  if(MACDEPLOYQT_EXECUTABLE)
    # Empacota numa COPIA isolada em build/dist/, nunca no .app original de
    # apps/gui/CMakeFiles/... -- rodar macdeployqt direto no .app de
    # desenvolvimento o deixa com dois cópias de Qt carregadas (Homebrew +
    # embutida) em qualquer build normal seguinte, causando
    # "QObject::moveToThread" / "Could not load the Qt platform plugin
    # cocoa" e crash ao abrir. Passos:
    # 0) copia o .app recem-buildado para build/dist/ (limpando antes)
    # 1) bundla Qt + libsndfile (e tudo que eles arrastam) para dentro da copia
    # 2) reassina ad-hoc -- macdeployqt invalida a assinatura ao copiar libs
    #    para dentro do bundle, e o macOS (Apple Silicon) recusa rodar/montar
    #    um .app com assinatura quebrada
    # 3) so entao gera o .dmg, a partir do .app ja assinado
    set(_wjvsc_dist_dir "${CMAKE_BINARY_DIR}/dist")
    set(_wjvsc_dist_app "${_wjvsc_dist_dir}/WJ-VSC.app")
    add_custom_target(package_gui
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_wjvsc_dist_app}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_wjvsc_dist_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                $<TARGET_BUNDLE_DIR:WJ-VSC> "${_wjvsc_dist_app}"
        COMMAND ${MACDEPLOYQT_EXECUTABLE}
                "${_wjvsc_dist_app}"
                -verbose=1
        COMMAND codesign --force --deep --sign - "${_wjvsc_dist_app}"
        COMMAND hdiutil create -volname WJ-VSC -srcfolder "${_wjvsc_dist_app}"
                -ov -format UDZO
                "${_wjvsc_dist_dir}/WJ-VSC.dmg"
        DEPENDS WJ-VSC
        COMMENT "Empacotando WJ-VSC.app (copia isolada em build/dist/, Qt + libsndfile embutidos), reassinando e gerando .dmg"
        VERBATIM
    )
  else()
    message(STATUS "macdeployqt nao encontrado -- alvo 'package_gui' nao disponivel")
  endif()
endif()
