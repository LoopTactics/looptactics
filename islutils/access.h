#ifndef ACCESS_H
#define ACCESS_H

#include <isl/cpp.h>

#include <functional>
#include <vector>

#include "islutils/operators.h"

namespace matchers {

class DimCandidate {
public:
  DimCandidate(int inputDimPos, isl::map candidateMap)
      : inputDimPos_(inputDimPos), candidateMap_(candidateMap) {}

  bool operator==(const DimCandidate &other) const {
    return inputDimPos_ == other.inputDimPos_ &&
           candidateMap_ == other.candidateMap_;
  }

  // Assuming the input space of all candidates is the same (e.g., schedule
  // space), we only need to keep the position in this space.
  // TODO: We may want to abstract away the candidate description together with
  // the candidate-filling function and comparison operators.
  int inputDimPos_;

  isl::map candidateMap_;
};

class Placeholder {
public:
  isl::val coefficient_;
  int outDimPos_;
  std::vector<DimCandidate> candidates_;
};

class PlaceholderSet {
public:
  std::vector<Placeholder> placeholders_;

  // Each inner vector has a set of indices of placeholders that should appear
  // together in a relation.  Different groups must correspond to different
  // relations.  We store indices separately because a placeholder may appear
  // in multiple relations, actual objects are stored in placeholders_.  We
  // don't store references because of how the match is currently structured: a
  // vector of candidates, each of which is itself a vector with the same index
  // as the position of the placeholder in placeholders_.  This may change in
  // the future for a more C++-idiomatic API.
  std::vector<std::vector<size_t>> placeholderGroups_;
};

class Match {
public:
  // Not the same semantic as the set of placeholders.
  std::vector<std::vector<DimCandidate>> matches_;
};

std::vector<std::vector<DimCandidate>> match(isl::union_map access,
                                             PlaceholderSet &ps);

} // namespace matchers

#endif // ACCESS_H
