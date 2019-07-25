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
  QString transformation_string_;
};

// FIXME: rename the class 
class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit Highlighter(isl::ctx context, QTextDocument *parent = 0);
    static int get_next_stmt_id();

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

  void do_transformation(const QString &text, bool recompute);
  void fuse(const QString &text, bool recompute);
  void tile(const QString &text, bool recompute);
  void unroll(const QString &text, bool recompute);
  void interchange(const QString &text, bool recompute);
  void loop_reverse(const QString &text, bool recompute);
  void match_pattern(const QString &text, bool recompute);
  bool match_pattern_helper(
    std::vector<Parser::AccessDescriptor> accesses_descriptors, 
    const pet::Scop &scop, bool recompute);
  void update_schedule(isl::schedule new_schedule, bool update_prev);
  void take_snapshot(const QString &text);
  void compare(bool with_baseline);

  struct HighlightingRule {
    QRegularExpression pattern_;
    QTextCharFormat format_;
    int id_rule_;
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
  QTextCharFormat fuseFormat_;

  isl::schedule current_schedule_;
  isl::schedule previous_schedule_;
  QString file_path_;

  // variable used to annotate the
  // stmt with an unique id.
public:
  static int stmt_id_;
};

#endif // HIGHLIGHTER_H
