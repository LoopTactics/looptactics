#include <QApplication>
#include "islutils/mainwindow.h"
#include "islutils/ctx.h"

int main(int argc, char **argv) {

  using namespace util;
  auto ctx = ScopedCtx(pet::allocCtx());

  QApplication app(argc, argv);
  MainWindow window(ctx);
  window.showMaximized();

  return app.exec();
}
