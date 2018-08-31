#ifndef ISLUTILS_ACCESS_PATTERNS_H
#define ISLUTILS_ACCESS_PATTERNS_H

#include "islutils/access.h"

namespace matchers {

// Pattern payload class for placeholders that capture simple 1d affine
// expressions of the form
//   coefficient_ * X + constant_
// where X is the match candidate described by, e.g., SingleInputDim.
class SimpleAff {
public:
  explicit SimpleAff(isl::ctx ctx)
      : coefficient_(isl::val::one(ctx)), constant_(isl::val::zero(ctx)) {}

  isl::val coefficient_;
  isl::val constant_;
};

// Assuming the input space of all candidates is the same (e.g., schedule
// space), we only need to keep the position in this space.
//
// This is an example class for payloads.  Static member functions
// appendToCandidateList and make1DMap must be implemented for the matchers and
// the transformers, respectively, to work.
//
// This candidate payload class is aware of the patterns it is compatible with
// and provides static member functions to work with them.
class SingleInputDim {
public:
  // Overload these functions to make this candidate payload class compatible
  // with other patterns.
  static std::vector<SingleInputDim> candidates(isl::map singleOutDimMap,
                                                isl::map fullMap,
                                                const SimpleAff &pattern);
  static isl::map make1DMap(const SingleInputDim &candidate,
                            const SimpleAff &placeholder, isl::space space);

  int inputDimPos_;
};

inline bool operator==(const SingleInputDim &left,
                       const SingleInputDim &right) {
  return left.inputDimPos_ == right.inputDimPos_;
}

inline UnpositionedPlaceholder<SingleInputDim, SimpleAff>
placeholder(isl::ctx ctx) {
  return UnpositionedPlaceholder<SingleInputDim, SimpleAff>(SimpleAff(ctx));
}

inline UnpositionedPlaceholder<SingleInputDim, SimpleAff>
operator*(int i, UnpositionedPlaceholder<SingleInputDim, SimpleAff> p) {
  p.pattern_.coefficient_ = p.pattern_.coefficient_.mul(
      isl::val(p.pattern_.coefficient_.get_ctx(), i));
  return p;
}

inline UnpositionedPlaceholder<SingleInputDim, SimpleAff>
operator+(UnpositionedPlaceholder<SingleInputDim, SimpleAff> p, int i) {
  p.pattern_.constant_ =
      p.pattern_.constant_.add(isl::val(p.pattern_.constant_.get_ctx(), i));
  return p;
}

////////////////////

class StridePattern {
public:
  explicit StridePattern(isl::ctx ctx) : stride(isl::val::one(ctx)) {}

  isl::val stride;
};

class StrideCandidate {
public:
  static std::vector<StrideCandidate> candidates(isl::map singleOutDimMap,
                                                 isl::map fullMap,
                                                 const StridePattern &pattern);

  bool operator==(const StrideCandidate &) const { return true; }
};

inline UnpositionedPlaceholder<StrideCandidate, StridePattern>
stride(isl::ctx ctx, int s) {
  StridePattern pattern(ctx);
  pattern.stride = pattern.stride.mul_ui(std::abs(s));
  if (s < 0) {
    pattern.stride = pattern.stride.neg();
  }
  return UnpositionedPlaceholder<StrideCandidate, StridePattern>(pattern);
}

} // namespace matchers

#endif
