#ifndef ACCESS_H
#define ACCESS_H

#include <isl/cpp.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "islutils/die.h"
#include "islutils/operators.h"

namespace matchers {

// Assuming the input space of all candidates is the same (e.g., schedule
// space), we only need to keep the position in this space.
// TODO: we may want to bring coefficient_, constant_ and even space_ here,
// even though they are shared across all candidates.  Optionally, we may want
// to separate the "pattern payload" from "match payload".
class SingleInputDim {
public:
  int inputDimPos_;
};

bool operator==(const SingleInputDim &left, const SingleInputDim &right) {
  return left.inputDimPos_ == right.inputDimPos_;
}

// Candidates are parameterized by the type of Payload they carry.  Only one
// type of payload is allowed within a PlaceholderSet.
// Instances of payload must be comparable to each other.
// TODO: provide functions to extract the payload from the access relation
// projected onto a single output dimension.
template <typename Payload> class DimCandidate {
public:
  DimCandidate(const Payload &payload, isl::map candidateMap)
      : payload_(payload), candidateMap_(candidateMap) {}

  bool isEqualModuloMap(const DimCandidate &other) const {
    return payload_ == other.payload_;
  }

  bool operator==(const DimCandidate &other) const {
    return isEqualModuloMap(other) && candidateMap_ == other.candidateMap_;
  }

  Payload payload_;

  // TODO: Not sure we need the entire map, or the space would suffice.
  isl::map candidateMap_;
};

template <typename CandidatePayload> class UnpositionedPlaceholder {
public:
  explicit UnpositionedPlaceholder(isl::ctx ctx)
      : coefficient_(isl::val::one(ctx)), constant_(isl::val::zero(ctx)),
        candidates_({}), id_(nextId_++) {}
  UnpositionedPlaceholder(const UnpositionedPlaceholder &) = default;

  isl::val coefficient_;
  isl::val constant_;
  std::vector<DimCandidate<CandidatePayload>> candidates_;

  const size_t id_;

private:
  static thread_local size_t nextId_;
};

template <typename CandidatePayload>
thread_local size_t UnpositionedPlaceholder<CandidatePayload>::nextId_ = 0;

template <typename CandidatePayload>
class Placeholder : public UnpositionedPlaceholder<CandidatePayload> {
public:
  explicit Placeholder(isl::ctx ctx, int pos)
      : UnpositionedPlaceholder<CandidatePayload>(ctx), outDimPos_(pos) {}
  explicit Placeholder(const UnpositionedPlaceholder<CandidatePayload> &other,
                       int pos)
      : UnpositionedPlaceholder<CandidatePayload>(other), outDimPos_(pos) {}

  int outDimPos_;
};

inline UnpositionedPlaceholder<SingleInputDim> placeholder(isl::ctx ctx) {
  return UnpositionedPlaceholder<SingleInputDim>(ctx);
}

inline Placeholder<SingleInputDim>
dim(int pos, UnpositionedPlaceholder<SingleInputDim> ph) {
  return Placeholder<SingleInputDim>(ph, pos);
}

inline UnpositionedPlaceholder<SingleInputDim>
operator*(int i, UnpositionedPlaceholder<SingleInputDim> p) {
  p.coefficient_ = p.coefficient_.mul(isl::val(p.coefficient_.get_ctx(), i));
  return p;
}

inline UnpositionedPlaceholder<SingleInputDim>
operator+(UnpositionedPlaceholder<SingleInputDim> p, int i) {
  p.constant_ = p.constant_.add(isl::val(p.constant_.get_ctx(), i));
  return p;
}

template <typename CandidatePayload>
using PlaceholderList = std::vector<Placeholder<CandidatePayload>>;

template <typename Arg, typename Arg0, typename... Args>
struct all_are
    : public std::integral_constant<bool, std::is_same<Arg, Arg0>::value &&
                                              all_are<Arg, Args...>::value> {};

template <typename Arg, typename Arg0>
struct all_are<Arg, Arg0>
    : public std::integral_constant<bool, std::is_same<Arg, Arg0>::value> {};

// Placeholder<CandidatePayload> is used twice in all_are to properly handle
// the case of sizeof...(Args) == 0 (all_are is not defined for 1 argument).
template <typename CandidatePayload, typename... Args>
typename std::enable_if<all_are<Placeholder<CandidatePayload>,
                                Placeholder<CandidatePayload>, Args...>::value,
                        PlaceholderList<CandidatePayload>>::type
access(Placeholder<CandidatePayload> arg, Args... args) {
  return {arg, args...};
}

template <typename CandidatePayload, typename... Args>
typename std::enable_if<
    all_are<UnpositionedPlaceholder<CandidatePayload>,
            UnpositionedPlaceholder<CandidatePayload>, Args...>::value,
    PlaceholderList<CandidatePayload>>::type
access(UnpositionedPlaceholder<CandidatePayload> arg, Args... args) {
  PlaceholderList<CandidatePayload> result;
  int pos = 0;
  for (const auto &a : {arg, args...}) {
    result.emplace_back(a, pos++);
  }
  return result;
}

template <typename CandidatePayload> class PlaceholderSet;

template <typename CandidatePayload, typename... Args>
PlaceholderSet<CandidatePayload> allOf(Args... args);

template <typename CandidatePayload> class PlaceholderSet {
  // TODO: Check if we can friend a partial specialization
  template <typename CPayload, typename... Args>
  friend PlaceholderSet allOf(Args... args);

public:
  std::vector<Placeholder<CandidatePayload>> placeholders_;

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

/// Build an object used to match all of the access patterns provided as
/// arguments. Individual patterns can be constructed by calling "access(...)".
template <typename CandidatePayload, typename... Args>
PlaceholderSet<CandidatePayload> allOf(PlaceholderList<CandidatePayload> arg,
                                       Args... args) {
  static_assert(all_are<PlaceholderList<CandidatePayload>,
                        PlaceholderList<CandidatePayload>, Args...>::value,
                "can only make PlaceholderSet from PlaceholderLists "
                "with the same payload types");

  std::vector<PlaceholderList<CandidatePayload>> placeholderLists = {arg,
                                                                     args...};
  std::vector<std::pair<size_t, size_t>> knownIds;
  PlaceholderSet<CandidatePayload> ps;
  for (const auto &pl : placeholderLists) {
    if (pl.empty()) {
      continue;
    }

    size_t index = ps.placeholders_.size();
    ps.placeholderGroups_.emplace_back();
    for (const auto &p : pl) {
      ps.placeholders_.push_back(p);
      ps.placeholderGroups_.back().push_back(index);
      auto namePos = std::find_if(knownIds.begin(), knownIds.end(),
                                  [p](const std::pair<size_t, size_t> &pair) {
                                    return pair.first == p.id_;
                                  });
      if (namePos == knownIds.end()) {
        knownIds.emplace_back(p.id_, index);
        ps.placeholderFolds_.emplace_back(index);
      } else {
        ps.placeholderFolds_.emplace_back(namePos->second);
      }
      ++index;
    }
  }

  return ps;
}

template <typename CandidatePayload> class Match {
public:
  Match(const PlaceholderSet<CandidatePayload> &ps,
        const std::vector<DimCandidate<CandidatePayload>> &combination);

  DimCandidate<CandidatePayload>
  operator[](const UnpositionedPlaceholder<CandidatePayload> &pl) const {
    auto result = std::find_if(
        placeholderValues_.begin(), placeholderValues_.end(),
        [pl](const std::pair<size_t, DimCandidate<CandidatePayload>> &kvp) {
          return kvp.first == pl.id_;
        });
    if (result == placeholderValues_.end()) {
      ISLUTILS_DIE("no match for the placeholder although matches found");
    }
    return result->second;
  }

private:
  std::vector<std::pair<size_t, DimCandidate<CandidatePayload>>>
      placeholderValues_;
};

template <typename CandidatePayload>
using Matches = std::vector<Match<CandidatePayload>>;

template <typename CandidatePayload>
Matches<CandidatePayload> match(isl::union_map access,
                                PlaceholderSet<CandidatePayload> ps);

template <typename CandidatePayload> struct Replacement {
  Replacement(PlaceholderList<CandidatePayload> &&pattern_,
              PlaceholderList<CandidatePayload> &&replacement_)
      : pattern(pattern_), replacement(replacement_) {}

  PlaceholderList<CandidatePayload> pattern;
  PlaceholderList<CandidatePayload> replacement;
};

template <typename CandidatePayload>
Replacement<CandidatePayload>
replace(PlaceholderList<CandidatePayload> &&pattern,
        PlaceholderList<CandidatePayload> &&replacement) {
  return {std::move(pattern), std::move(replacement)};
}

// Calls like this do not fully make sense: different replacements for
// essentially the same pattern.
// makePSR(replace(access(_1, _2), access(_2, _1)),
//         replace(access(_3, _4), access(_3, _4)))
// They could become useful if access is further constrained to specific arrays
// or statements/schedule points.
//
// Calls like this contain redundant information and should be disallowed.
// makePSR(replace(access(_1, _2), access(_2, _1)),
//         replace(access(_1, _2), access(_2, _1)))
//
// Generally, we should disallow such transformations that affect the same
// relation more than once during the same call to transform.
// For example, access(_1, *), access(_1, _2).
//
// As the first approximation, marking this as undefined behavior and ignoring.

template <typename CandidatePayload, typename... Args>
isl::union_map findAndReplace(isl::union_map umap,
                              Replacement<CandidatePayload> arg, Args... args);

} // namespace matchers

#include "access-inl.h"

#endif // ACCESS_H
