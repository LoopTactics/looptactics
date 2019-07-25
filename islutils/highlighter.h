#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <isl/cpp.h>
#include "islutils/pet_wrapper.h"
#include "islutils/parser.h"
#include "islutils/loop_opt.h"
#include "islutils/tuner_thread.h"
#include "islutils/timing_info.h"

class QTextDocument;

using namespace LoopTactics;

struct BlockSchedule : public QTextBlockUserData {
  isl::schedule schedule_block_;
};

// FIXME: rename the class 
class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit Highlighter(isl::ctx context, QTextDocument *parent = 0);

public Q_SLOTS:
  void updatePath(const QString &path); 
  void updateTime(const timeInfo::TimingInfo &baseline_time, 
    const timeInfo::TimingInfo &opt_time);

Q_SIGNALS:
  void codeChanged(const QString &code);
  void userFeedbackChanged(const timeInfo::TimingInfo &baseline_time,
    const timeInfo::TimingInfo &opt_time);

protected:
  void highlightBlock(const QString &text) override;

private:

  void tile(std::string tile_transformation);
  void unroll(std::string unroll_transformation);
  void interchange(std::string interchange_transformation);
  void loop_reverse(std::string loop_reverse_transformation);
  void match_pattern(const std::string &text);
  bool match_pattern_helper(
    std::vector<Parser::AccessDescriptor> accesses_descriptors, const pet::Scop &scop);
  void update_schedule(isl::schedule new_schedule);
  void take_snapshot();
  void compare(bool with_baseline);

  struct HighlightingRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };
  QVector<HighlightingRule> highlightingRules;

  isl::ctx context_;
  LoopOptimizer opt_;
  TunerThread tuner_;
  
  QTextCharFormat patternFormat_;
  QTextCharFormat tileFormat_;
  QTextCharFormat interchangeFormat_;
  QTextCharFormat unrollFormat_;
  QTextCharFormat loopReversalFormat_;
  QTextCharFormat timeFormat_;

  isl::schedule current_schedule_;
  QString file_path_;
};

#endif // HIGHLIGHTER_H
