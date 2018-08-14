#ifndef ACCESS_H
#define ACCESS_H

#include <isl/cpp.h>

#include <functional>
#include <vector>

namespace matchers {

class DimCandidate {
public:
  DimCandidate(int inputDimPos, isl::id outTupleId)
      : inputDimPos_(inputDimPos), outTupleId_(outTupleId) {}

  bool operator==(const DimCandidate &other) const {
    return inputDimPos_ == other.inputDimPos_ &&
           ((outTupleId_.is_null() && other.outTupleId_.is_null()) ||
            outTupleId_.get() == other.outTupleId_.get());
  }

  // Assuming the input space of all candidates is the same (e.g., schedule
  // space), we only need to keep the position in this space.
  int inputDimPos_;
  isl::id outTupleId_;
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
