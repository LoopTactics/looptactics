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
