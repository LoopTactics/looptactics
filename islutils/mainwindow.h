#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "islutils/tactics.h"
#include "islutils/highlighter.h"
#include <QMainWindow>
#include "islutils/timing_info.h"
#include <isl/cpp.h>

class QTextEdit;

class MainWindow : public QMainWindow {
  
  Q_OBJECT

public:
  explicit MainWindow(isl::ctx ctx, QWidget *parent = 0);
  
public Q_SLOTS:
  void about();
  void newFile();
  void openFile(QString path = QString());
  void updateCode(const QString &code);
  void updateUserFeedback(const timeInfo::TimingInfo &baseline_time,
    const timeInfo::TimingInfo &opt_time); 

Q_SIGNALS:
  void filePathChanged(const QString &path);

private:
  void setupEditor();
  void setupFileMenu();
  void setupHelpMenu();

  isl::ctx context_;
  
  QWidget *graphical_interface_ = nullptr;
  QTextEdit *code_editor_ = nullptr;
  QTextEdit *script_editor_ = nullptr;
  // FIXME: rename me to feedback editor 
  QTextEdit *info_editor_ = nullptr;
  Highlighter *highlighter_ = nullptr;
};


#endif
