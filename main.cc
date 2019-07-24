/*
#include <iostream>
#include "islutils/tactics.h"

int main() {

  using namespace LoopTactics;
  using namespace Error;
  try {
    std::string tactics_id = "tactics_gemm_no_init";
    std::string pattern = "C(i, j)+= A(i, k)*B(k,j)";
    std::string path_to_file = "./test/inputs/gemm.c";

    Tactics t = Tactics(tactics_id, pattern, path_to_file);
    t.match();
    t.tile("i", {32});
    t.tile("k", {32});
    t.tile("j", {32});
    //t.show();
    t.interchange("i_p", "j_p");
    t.show();

  } catch (Error::Error e) {
    std::cout << e.message_ << "\n";
  }
  return 0;
}
*/
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
