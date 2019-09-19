#ifndef HAYSTACK_RUNNER
#define HAYSTACK_RUNNER

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "islutils/pet_wrapper.h"
#include "islutils/feedback_definition.h"

class HaystackRunner : public QThread {

  Q_OBJECT

public:
  HaystackRunner(QObject *parent = nullptr, isl::ctx ctx = nullptr);
  ~HaystackRunner();

  void runModel(isl::schedule schedule, const QString &filePath);
  
Q_SIGNALS:
  void computedStats(const userFeedback::CacheStats);

protected:
  void run() override;

private:

  isl::ctx context_;
  isl::schedule schedule_;
  QString filePath_;

  QWaitCondition condition_;
  QMutex mutex_;
  bool restart_;
  bool abort_;
};
#endif
