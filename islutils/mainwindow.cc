#include <QtWidgets>
#include <QtCore>
#include <QtGui>
#include <fstream> // std::ifstream
#include "islutils/mainwindow.h"

#include "islutils/pet_wrapper.h"

static constexpr int TABSTOP = 2;

MainWindow::MainWindow(isl::ctx ctx, QWidget *parent) : context(ctx), QMainWindow(parent) {

  setupFileMenu();
  setupHelpMenu();
  setupEditor();

  QSplitter *splitter = new QSplitter(Qt::Horizontal);
  splitter->addWidget(scriptEditor);
  splitter->addWidget(codeEditor);

  setCentralWidget(splitter);
  setWindowTitle(tr("loop tactics"));
}

void MainWindow::about() {

}

void MainWindow::newFile() {

  codeEditor->clear();
}

void MainWindow::updateCode(const QString &code) {

  codeEditor->clear();
  codeEditor->setPlainText(code);
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

void MainWindow::openFile(QString path) {
    
  if (path.isNull())
    path = 
      QFileDialog::getOpenFileName(this, tr("Open File"), "", "C++ Files (*.cpp *.c *.h)");
  
  if (!path.isEmpty()) {
    const std::string path_as_std_string = path.toStdString();
    pet::Scop scop = pet::Scop(pet::Scop::parseFile(context, path_as_std_string));
    unsigned scop_start = scop.startPetLocation();
    unsigned scop_end = scop.endPetLocation();
    std::string scop_as_string = getScopAsString(scop_start, scop_end, path_as_std_string); 
    codeEditor->setPlainText(QString(scop_as_string.c_str()));
  }

  scriptEditor->setReadOnly(false);
  Q_EMIT filePathChanged(path);
  
}

void MainWindow::setupEditor() {

  QFont font;
  font.setFamily("Mono");
  font.setFixedPitch(true);
  font.setPointSize(12);
  
  codeEditor = new QTextEdit();
  codeEditor->setFont(font);
  codeEditor->setReadOnly(true);

  scriptEditor = new QTextEdit();
  scriptEditor->setFont(font);
  QFontMetrics metrics(font);
  scriptEditor->setTabStopWidth(TABSTOP * metrics.width(' '));
  scriptEditor->setReadOnly(true);

  highlighter = new Highlighter(context, scriptEditor->document());
  QObject::connect(highlighter, &Highlighter::codeChanged, this, &MainWindow::updateCode);
  QObject::connect(this, &MainWindow::filePathChanged, highlighter, &Highlighter::updatePath);

  QFile file("gemm.c");
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    codeEditor->setPlainText(file.readAll());
    scriptEditor->setPlainText(file.readAll());
  }
}

void MainWindow::setupFileMenu() {
  
  QMenu *fileMenu = new QMenu(tr("&File"), this);
  menuBar()->addMenu(fileMenu);

  fileMenu->addAction(tr("&Open"), this, [this]() { openFile(); }, QKeySequence::Open);
  fileMenu->addAction(tr("&Eit"), qApp, [this]() { QApplication::quit(); }, 
    QKeySequence::Quit);
}

void MainWindow::setupHelpMenu() {

  QMenu *helpMenu = new QMenu(tr("&Help"), this);
  menuBar()->addMenu(helpMenu);
  
  helpMenu->addAction(tr("&About"), this, &MainWindow::about);
}

