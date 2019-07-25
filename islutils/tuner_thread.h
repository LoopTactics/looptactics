#ifndef TUNER_THREAD_H
#define TUNER_THREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <isl/cpp.h>

#include "islutils/timing_info.h"

class TunerThread : public QThread {

  Q_OBJECT

public:
  TunerThread(QObject *parent = nullptr, isl::ctx ctx = nullptr);
  ~TunerThread();

  void compare(const isl::schedule baseline_schedule, 
    const isl::schedule opt_schedule, const QString &file_path);

Q_SIGNALS:
  void compareSchedules(const timeInfo::TimingInfo &time_baseline, 
    const timeInfo::TimingInfo &time_opt);

protected:
  void run() override;

private:

  isl::ctx context_;

  QMutex mutex_;
  QWaitCondition condition_;
  bool restart_;
  bool abort_;

  isl::schedule baseline_schedule_;
  isl::schedule opt_schedule_;
  QString file_path_;

};

#endif