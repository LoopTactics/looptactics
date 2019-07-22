#include <QtWidgets>
#include <QtCore>
#include <QtGui>
#include <fstream> // std::ifstream
#include "islutils/mainwindow.h"
#include "islutils/pet_wrapper.h"

using namespace timeInfo;

static constexpr int TABSTOP = 2;

MainWindow::MainWindow(isl::ctx ctx, QWidget *parent) : context_(ctx), QMainWindow(parent) {

  setupFileMenu();
  setupHelpMenu();
  setupEditor();

  QSplitter *splitter = new QSplitter(Qt::Horizontal);
  QSplitter *nested_splitter = new QSplitter(Qt::Vertical);

  nested_splitter->addWidget(script_editor_);
  nested_splitter->addWidget(info_editor_);
  splitter->addWidget(nested_splitter);
  splitter->addWidget(code_editor_);

  setCentralWidget(splitter);
  setWindowTitle(tr("loop tactics"));
}

void MainWindow::about() {

}

void MainWindow::newFile() {

  code_editor_->clear();
}

void MainWindow::updateCode(const QString &code) {

  code_editor_->clear();
  code_editor_->setPlainText(code);
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
    pet::Scop scop = pet::Scop(pet::Scop::parseFile(context_, path_as_std_string));
    unsigned scop_start = scop.startPetLocation();
    unsigned scop_end = scop.endPetLocation();
    std::string scop_as_string = getScopAsString(scop_start, scop_end, path_as_std_string); 
    code_editor_->setPlainText(QString(scop_as_string.c_str()));
  }

  script_editor_->setReadOnly(false);
  Q_EMIT filePathChanged(path);
  
}

void MainWindow::updateUserFeedback(const TimingInfo &baseline_time,
  const TimingInfo &opt_time) {

  info_editor_->clear();

  std::string baseline = "Baseline min time: ";
  baseline += std::to_string(baseline_time.min_time) + "\n"; 
  baseline += "Baseline max time: ";
  baseline += std::to_string(baseline_time.max_time) + "\n";
  baseline += "Baseline avg time: ";
  baseline += std::to_string(baseline_time.avg_time) + "\n";
  baseline += "Baseline median time: ";
  baseline += std::to_string(baseline_time.median_time) + "\n";

  std::string opt = "Optimized min time: ";
  opt += std::to_string(opt_time.min_time) + "\n"; 
  opt += "Optimized max time: ";
  opt += std::to_string(opt_time.max_time) + "\n";
  opt += "Optimized avg time: ";
  opt += std::to_string(opt_time.avg_time) + "\n";
  opt += "Optimized median time: ";
  opt += std::to_string(opt_time.median_time) + "\n";
  
  QString baseline_qString = QString(baseline.c_str());
  QString opt_qString = QString(opt.c_str());
  info_editor_->setPlainText(baseline_qString + opt_qString);
  
}

void MainWindow::setupEditor() {

  QFont font;
  font.setFamily("Mono");
  font.setFixedPitch(true);
  font.setPointSize(12);
  
  info_editor_ = new QTextEdit();
  info_editor_->setFont(font);
  info_editor_->setReadOnly(true);
  
  code_editor_ = new QTextEdit();
  code_editor_->setFont(font);
  code_editor_->setReadOnly(true);

  script_editor_ = new QTextEdit();
  script_editor_->setFont(font);
  QFontMetrics metrics(font);
  script_editor_->setTabStopWidth(TABSTOP * metrics.width(' '));
  script_editor_->setReadOnly(true);

  highlighter_ = new Highlighter(context_, script_editor_->document());
  QObject::connect(highlighter_, &Highlighter::codeChanged, this, &MainWindow::updateCode);
  QObject::connect(highlighter_, &Highlighter::userFeedbackChanged, 
    this, &MainWindow::updateUserFeedback);
  QObject::connect(this, &MainWindow::filePathChanged, highlighter_, &Highlighter::updatePath);

  QFile file("gemm.c");
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    code_editor_->setPlainText(file.readAll());
    script_editor_->setPlainText(file.readAll());
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

