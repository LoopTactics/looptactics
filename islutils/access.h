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

  bool isEqualModuloMap(const DimCandidate &other) const {
    return inputDimPos_ == other.inputDimPos_;
  }

  bool operator==(const DimCandidate &other) const {
    return isEqualModuloMap(other) && candidateMap_ == other.candidateMap_;
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
  isl::val constant_;
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

  // Placeholder fold is an identifier of a set of placeholders that must get
  // assigned the same candidate value modulo the matched map.  The idea is to
  // reuse, at the API level, placeholders in multiple places to indicate
  // equality of the matched access patterns.
  // This vector is co-indexed with placeholders_.  By default, each
  // placeholder gets assigned its index in the placeholders_ list, that is
  // placeholderFolds_[i] == i. Placeholders that belong to the same group have
  // the same fold index, by convention we assume it is the index in
  // placeholders_ of the first placeholder in the fold.
  // One placeholder cannot belong to multiple folds.
  std::vector<size_t> placeholderFolds_;
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
