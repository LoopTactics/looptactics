#include <QtWidgets>
#include <QtCore>
#include <QtGui>
#include <fstream> // std::ifstream
#include "islutils/mainwindow.h"

#include "islutils/pet_wrapper.h"

MainWindow::MainWindow(isl::ctx ctx, QWidget *parent) : context(ctx), QMainWindow(parent) {

  setupFileMenu();
  setupHelpMenu();
  setupEditor();

  QSplitter *splitter = new QSplitter(Qt::Horizontal);
  splitter->addWidget(scriptEditor);
  splitter->addWidget(editor);

  setCentralWidget(splitter);
  setWindowTitle(tr("loop tactics"));
}

void MainWindow::about() {
}

void MainWindow::newFile() {

  editor->clear();
}

std::string getScopAsString(
unsigned scop_start, unsigned scop_end, const std::string path_to_file) {

  std::string result{};
  std::ifstream source_file;
  source_file.open(path_to_file);
  std::string line{}; 
  while (std::getline(source_file, line)) {
    if (source_file.tellg() >= scop_start && source_file.tellg() <= scop_end) {
      result += line + "\n";
    }
  }
  source_file.close();
  return result;
}

void MainWindow::openFile(const QString &path) {
  
  QString fileName = path;
    
  if (fileName.isNull())
    fileName = 
      QFileDialog::getOpenFileName(this, tr("Open File"), "", "C++ Files (*.cpp *.c *.h)");
  
  if (!fileName.isEmpty()) {
    const std::string path_to_file = std::string(fileName.toLatin1().data());
    pet::Scop scop = pet::Scop(pet::Scop::parseFile(context, path_to_file));
    unsigned scop_start = scop.startPetLocation();
    unsigned scop_end = scop.endPetLocation();
    std::string scop_as_string = getScopAsString(scop_start, scop_end, path_to_file); 
    editor->setPlainText(QString(scop_as_string.c_str()));
  }
  
}

void MainWindow::setupEditor() {

  QFont font;
  font.setFamily("Mono");
  font.setFixedPitch(true);
  font.setPointSize(12);
  
  // TODO rename
  editor = new QTextEdit();
  editor->setFont(font);
  editor->setReadOnly(true);

  scriptEditor = new QTextEdit();
  scriptEditor->setFont(font);
  // FIXME : this should not "float" around the code.
  const int tabStop = 2;  // 2 characters
  QFontMetrics metrics(font);
  scriptEditor->setTabStopWidth(tabStop * metrics.width(' '));

  highlighter = new Highlighter(context, scriptEditor->document());

  QFile file("gemm.c");
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    editor->setPlainText(file.readAll());
    scriptEditor->setPlainText(file.readAll());
  }
}

void MainWindow::setupFileMenu() {
  
  QMenu *fileMenu = new QMenu(tr("&File"), this);
  menuBar()->addMenu(fileMenu);

  fileMenu->addAction(tr("&Open"), this, [this]() { openFile(); }, QKeySequence::Open);
  fileMenu->addAction(tr("&Eit"), qApp, [this]() { 
    //if (t)
    //  delete t;
    QApplication::quit(); }, 
    QKeySequence::Quit);
}

void MainWindow::setupHelpMenu() {

  QMenu *helpMenu = new QMenu(tr("&Help"), this);
  menuBar()->addMenu(helpMenu);
  
  helpMenu->addAction(tr("&About"), this, &MainWindow::about);
}





