#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <isl/cpp.h>
#include "islutils/pet_wrapper.h"
#include "islutils/parser.h"
#include "islutils/loop_opt.h"

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

Q_SIGNALS:
  void codeChanged(const QString &code);

protected:
  void highlightBlock(const QString &text) override;

private:

  void tile(std::string tile_transformation);
  void unroll(std::string unroll_transformation);
  void interchange(std::string interchange_transformation);
  void matchPattern(const std::string &text);
  void matchPatternHelper(
    std::vector<Parser::AccessDescriptor> accesses_descriptors, const pet::Scop &scop);
  void updateSchedule(isl::schedule new_schedule);
  void takeSnapshot();

  struct HighlightingRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };
  QVector<HighlightingRule> highlightingRules;

  isl::ctx context_;
  LoopOptimizer opt_;
  QTextCharFormat patternFormat_;
  QTextCharFormat tileFormat_;
  QTextCharFormat interchangeFormat_;
  QTextCharFormat unrollFormat_;
  isl::schedule current_schedule_;
  QString file_path_;
};

#endif // HIGHLIGHTER_H
