#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <isl/cpp.h>
#include "islutils/pet_wrapper.h"
#include "islutils/parser.h"

class QTextDocument;

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(isl::ctx context, QTextDocument *parent = 0);

protected:
    void highlightBlock(const QString &text) override;

private:

    void matchPattern(const std::string &text);
    void matchPatternHelper(
      std::vector<Parser::AccessDescriptor> accesses_descriptors, const pet::Scop &scop);

    isl::ctx context;
    isl::schedule current_schedule;

    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat patternFormat;
};

#endif // HIGHLIGHTER_H
