set(CMAKE_AUTOMOC ON)

if(APPLE)
  add_executable(akai2sfz_gui MACOSX_BUNDLE
      main.cpp
      MainWindow.cpp
      MainWindow.h
  )
  set_target_properties(akai2sfz_gui PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
      MACOSX_BUNDLE_BUNDLE_NAME "akai2sfz"
      MACOSX_BUNDLE_GUI_IDENTIFIER "app.akai2sfz.gui"
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
  )
else()
  add_executable(akai2sfz_gui
      main.cpp
      MainWindow.cpp
      MainWindow.h
  )
endif()

target_link_libraries(akai2sfz_gui PRIVATE akai2sfz_core Qt6::Widgets)

install(TARGETS akai2sfz_gui
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
    # 3 passos separados (nao usar a flag -dmg do macdeployqt direto):
    # 1) bundla Qt + libsndfile (e tudo que eles arrastam) para dentro do .app
    # 2) reassina ad-hoc -- macdeployqt invalida a assinatura ao copiar libs
    #    para dentro do bundle, e o macOS (Apple Silicon) recusa rodar/montar
    #    um .app com assinatura quebrada
    # 3) so entao gera o .dmg, a partir do .app ja assinado
    add_custom_target(package_gui
        COMMAND ${MACDEPLOYQT_EXECUTABLE}
                $<TARGET_BUNDLE_DIR:akai2sfz_gui>
                -verbose=1
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:akai2sfz_gui>
        COMMAND hdiutil create -volname akai2sfz -srcfolder $<TARGET_BUNDLE_DIR:akai2sfz_gui>
                -ov -format UDZO
                $<TARGET_BUNDLE_DIR:akai2sfz_gui>/../akai2sfz_gui.dmg
        DEPENDS akai2sfz_gui
        COMMENT "Empacotando akai2sfz_gui.app (Qt + libsndfile), reassinando e gerando .dmg"
        VERBATIM
    )
  else()
    message(STATUS "macdeployqt nao encontrado -- alvo 'package_gui' nao disponivel")
  endif()
endif()
