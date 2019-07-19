#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "islutils/tactics.h"
#include "islutils/highlighter.h"
#include <QMainWindow>

#include <isl/cpp.h>

class QTextEdit;

class MainWindow : public QMainWindow {
  
  Q_OBJECT

public:
  explicit MainWindow(isl::ctx ctx, QWidget *parent = 0);
  
public Q_SLOTS:
  void about();
  void newFile();
  void openFile(const QString &path = QString());

private:
  void setupEditor();
  void setupFileMenu();
  void setupHelpMenu();

  isl::ctx context;
  
  QWidget *graphical_interface = nullptr;
  QTextEdit *editor = nullptr;
  QTextEdit *scriptEditor = nullptr;
  Highlighter *highlighter = nullptr;
};


#endif
