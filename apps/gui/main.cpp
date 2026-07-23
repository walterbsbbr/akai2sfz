#include "MainWindow.h"

#include <QApplication>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("WJ-VSC");
  app.setApplicationDisplayName("WJ-VSC (Vintage Sampler Converter)");

  MainWindow window;
  window.resize(1000, 700);
  window.show();

  return app.exec();
}
