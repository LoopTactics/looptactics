#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "islutils/highlighter.h"
#include <QMainWindow>
#include <isl/isl-noexceptions.h>

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
  void updateTimeUserFeedback(const userFeedback::TimingInfo &baseline_time,
    const userFeedback::TimingInfo &opt_time);
  void updateCacheUserFeedback(const userFeedback::CacheStats &stats);

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
