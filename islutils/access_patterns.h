#ifndef ISLUTILS_ACCESS_PATTERNS_H
#define ISLUTILS_ACCESS_PATTERNS_H

#include "islutils/access.h"
#include "islutils/die.h"

#include <climits>

namespace matchers {

// Wrapper pattern payload class that fixes the pattern to the specified output
// dimension.
template <typename PatternTy> class FixedOutDimPattern : public PatternTy {
public:
  FixedOutDimPattern(const PatternTy &t, int pos)
      : PatternTy(t), outDimPos(pos) {}

  template <typename CandidateTy>
  static std::vector<CandidateTy>
  candidates(isl::map access, const FixedOutDimPattern<PatternTy> &pattern);

  template <typename CandidateTy>
  static isl::map transformMap(isl::map map, const CandidateTy &candidate,
                               const FixedOutDimPattern<PatternTy> &pattern);

  int outDimPos;
};

// Marker class for FixedOutDimPattern with not-yet-specified output dimension.
template <typename PatternTy> class UnfixedOutDimPattern : public PatternTy {
  template <typename TargetPatternPayload_, typename CandidatePayload_,
            typename SourcePatternPayload_>
  friend Placeholder<CandidatePayload_, TargetPatternPayload_> pattern_cast(
      Placeholder<CandidatePayload_, SourcePatternPayload_> placeholder);

public:
  explicit UnfixedOutDimPattern(const PatternTy &t) : PatternTy(t) {}

private:
  explicit operator FixedOutDimPattern<PatternTy>();
};

template <typename PatternTy>
UnfixedOutDimPattern<PatternTy>::operator FixedOutDimPattern<PatternTy>() {
  return FixedOutDimPattern<PatternTy>(*this, INT_MAX);
}

template <typename PatternTy>
template <typename CandidateTy>
std::vector<CandidateTy> FixedOutDimPattern<PatternTy>::candidates(
    isl::map access, const FixedOutDimPattern<PatternTy> &pattern) {
  int pos = pattern.outDimPos;
  if (pos == INT_MAX) {
    ISLUTILS_DIE("no out dimension specified for FixedOutDimPattern");
  }
  int dim = access.dim(isl::dim::out);
  // Treat negative indexes as 1-based starting from the end of the output
  // space of the accessess relation.
  if (pos < 0) {
    pos = dim + pos;
  }
  if (pos >= dim || pos < 0) {
    return {};
  }
  auto single = access.project_out(isl::dim::out, pos + 1, dim - (pos + 1))
                    .project_out(isl::dim::out, 0, pos);
  return CandidateTy::candidates(single,
                                 static_cast<const PatternTy &>(pattern));
}

template <typename CandidateTy, typename T>
Placeholder<CandidateTy, FixedOutDimPattern<T>>
dim(int pos, Placeholder<CandidateTy, UnfixedOutDimPattern<T>> placeholder) {
  auto p = pattern_cast<FixedOutDimPattern<T>>(placeholder);
  p.pattern_.outDimPos = pos;
  return p;
}

template <typename CandidateTy, typename PatternTy, typename... Args>
typename std::enable_if<
    all_are<Placeholder<CandidateTy, UnfixedOutDimPattern<PatternTy>>,
            Placeholder<CandidateTy, UnfixedOutDimPattern<PatternTy>>,
            Args...>::value,
    PlaceholderList<CandidateTy, FixedOutDimPattern<PatternTy>>>::type
access(Placeholder<CandidateTy, UnfixedOutDimPattern<PatternTy>> arg,
       Args... args) {
  PlaceholderList<CandidateTy, FixedOutDimPattern<PatternTy>> result;
  int pos = 0;
  for (const auto &pl : {arg, args...}) {
    result.emplace_back(dim(pos++, pl));
  }
  return result;
}

template <typename CandidateTy, typename PatternTy, typename... Args>
typename std::enable_if<
    all_are<Placeholder<CandidateTy, UnfixedOutDimPattern<PatternTy>>,
            Placeholder<CandidateTy, UnfixedOutDimPattern<PatternTy>>,
            Args...>::value,
    ArrayPlaceholderList<CandidateTy, FixedOutDimPattern<PatternTy>>>::type
access(ArrayPlaceholder array,
       Placeholder<CandidateTy, UnfixedOutDimPattern<PatternTy>> arg,
       Args... args) {
  int pos = 0;
  return {array, {dim(pos++, arg), dim(pos++, args)...}};
}

std::vector<isl::map> listOf1DMaps(isl::map map);
isl::space addEmptyRange(isl::space space);
isl::map mapFrom1DMaps(isl::space, const std::vector<isl::map> &list);

template <typename PatternTy>
template <typename CandidateTy>
isl::map FixedOutDimPattern<PatternTy>::transformMap(
    isl::map map, const CandidateTy &candidate,
    const FixedOutDimPattern<PatternTy> &pattern) {
  auto list = listOf1DMaps(map);
  auto space1D =
      addEmptyRange(map.get_space().domain()).add_dims(isl::dim::out, 1);
  int pos = pattern.outDimPos;
  int dim = map.dim(isl::dim::out);
  if (dim == 0) {
    return map;
  }

  // Treat negative positions as starting from the end.
  if (pos < 0) {
    pos = dim + pos;
  }
  list[pos] = CandidateTy::transformMap(
      list[pos], candidate, static_cast<const PatternTy &>(pattern));
  return mapFrom1DMaps(map.get_space(), list);
}

////////////////////

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
                                                const SimpleAff &pattern);
  static std::vector<SingleInputDim>
  candidates(isl::map map, const FixedOutDimPattern<SimpleAff> &pattern) {
    return FixedOutDimPattern<SimpleAff>::candidates<SingleInputDim>(map,
                                                                     pattern);
  }

  static isl::map transformMap(isl::map, const SingleInputDim &candidate,
                               const SimpleAff &pattern);

  static isl::map transformMap(isl::map map, const SingleInputDim &candidate,
                               const FixedOutDimPattern<SimpleAff> &pattern) {
    return FixedOutDimPattern<SimpleAff>::transformMap<SingleInputDim>(
        map, candidate, pattern);
  }

  int inputDimPos_;
};

inline bool operator==(const SingleInputDim &left,
                       const SingleInputDim &right) {
  return left.inputDimPos_ == right.inputDimPos_;
}

inline Placeholder<SingleInputDim, UnfixedOutDimPattern<SimpleAff>>
placeholder(isl::ctx ctx) {
  return Placeholder<SingleInputDim, UnfixedOutDimPattern<SimpleAff>>(
      UnfixedOutDimPattern<SimpleAff>(SimpleAff(ctx)));
}

inline Placeholder<SingleInputDim, UnfixedOutDimPattern<SimpleAff>>
operator*(int i,
          Placeholder<SingleInputDim, UnfixedOutDimPattern<SimpleAff>> p) {
  p.pattern_.coefficient_ = p.pattern_.coefficient_.mul(
      isl::val(p.pattern_.coefficient_.get_ctx(), i));
  return p;
}

inline Placeholder<SingleInputDim, UnfixedOutDimPattern<SimpleAff>>
operator+(Placeholder<SingleInputDim, UnfixedOutDimPattern<SimpleAff>> p,
          int i) {
  p.pattern_.constant_ =
      p.pattern_.constant_.add(isl::val(p.pattern_.constant_.get_ctx(), i));
  return p;
}

////////////////////

/**
 * Pattern class to detect strides in an access relation.
 * By stride, we understand the constant offset in number of elements between
 * elements of an array accessed by subsequent iterations of the innermost
 * (relative to the partially applied schedule) loop.
 * For example, vectorization often requires that all accesses are either
 * stride-zero (the same element is accessed in all iterations) or stride-one
 * (subsequent iterations access subsequent array elements).
 *
 * If the offset is not constant between loop iterations, the offset is
 * considered to be undefined and is not matched by any stride.
 *
 * The pattern includes the value of the stride and, optionally, the set of
 * relevant points in schedule space.  The latter is useful in cases where not
 * every point in the schedule space performs the access, for example in case
 * of non-unit iterator increments or modular if-conditions.
 *
 * Candidates for this pattern may be captured by the StrideCandidate class.
 *
 * This class is designed for one-dimensional arrays.  It can be wrapped into
 * FixedOutDimPattern to operate on any dimension of a multi-dimensional array.
 */
class StridePattern {
public:
  explicit StridePattern(isl::ctx ctx) : stride(isl::val::one(ctx)) {}

  isl::val stride;                 ///< Expected stirde.
  isl::set nonEmptySchedulePoints; ///< Schedule points to check, if not empty.
};

class StrideCandidate {
public:
  static std::vector<StrideCandidate> candidates(isl::map singleOutDimMap,
                                                 const StridePattern &pattern);

  static std::vector<StrideCandidate>
  candidates(isl::map map, const FixedOutDimPattern<StridePattern> &pattern) {
    return FixedOutDimPattern<StridePattern>::candidates<StrideCandidate>(
        map, pattern);
  }

  bool operator==(const StrideCandidate &) const { return true; }
};

inline Placeholder<StrideCandidate, UnfixedOutDimPattern<StridePattern>>
stride(isl::ctx ctx, int s) {
  StridePattern pattern(ctx);
  pattern.stride = pattern.stride.mul_ui(std::abs(s));
  if (s < 0) {
    pattern.stride = pattern.stride.neg();
  }
  return Placeholder<StrideCandidate, UnfixedOutDimPattern<StridePattern>>(
      UnfixedOutDimPattern<StridePattern>(StridePattern(pattern)));
}

} // namespace matchers

#endif
