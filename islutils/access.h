#ifndef ACCESS_H
#define ACCESS_H

#include <isl/cpp.h>

#include <algorithm>
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

class UnpositionedPlaceholder {
public:
  explicit UnpositionedPlaceholder(isl::ctx ctx)
      : coefficient_(isl::val::one(ctx)), constant_(isl::val::zero(ctx)),
        candidates_({}), id_(nextId_++) {}
  UnpositionedPlaceholder(const UnpositionedPlaceholder &) = default;

  isl::val coefficient_;
  isl::val constant_;
  std::vector<DimCandidate> candidates_;

  const size_t id_;

private:
  static thread_local size_t nextId_;
};

class Placeholder : public UnpositionedPlaceholder {
public:
  explicit Placeholder(isl::ctx ctx, int pos)
      : UnpositionedPlaceholder(ctx), outDimPos_(pos) {}
  explicit Placeholder(const UnpositionedPlaceholder &other, int pos)
      : UnpositionedPlaceholder(other), outDimPos_(pos) {}

  int outDimPos_;
};

inline UnpositionedPlaceholder placeholder(isl::ctx ctx) {
  return UnpositionedPlaceholder(ctx);
}

inline Placeholder dim(int pos, UnpositionedPlaceholder ph) {
  return Placeholder(ph, pos);
}

inline UnpositionedPlaceholder operator*(int i, UnpositionedPlaceholder p) {
  p.coefficient_ = p.coefficient_.mul(isl::val(p.coefficient_.get_ctx(), i));
  return p;
}

inline UnpositionedPlaceholder operator+(UnpositionedPlaceholder p, int i) {
  p.constant_ = p.constant_.add(isl::val(p.constant_.get_ctx(), i));
  return p;
}

using PlaceholderList = std::vector<Placeholder>;

template <typename Arg, typename Arg0, typename... Args>
struct all_are
    : public std::integral_constant<bool, std::is_same<Arg, Arg0>::value &&
                                              all_are<Arg, Args...>::value> {};

template <typename Arg, typename Arg0>
struct all_are<Arg, Arg0>
    : public std::integral_constant<bool, std::is_same<Arg, Arg0>::value> {};

template <typename... Args>
typename std::enable_if<all_are<Placeholder, Args...>::value,
                        PlaceholderList>::type
access(Args... args) {
  return {args...};
}

template <typename... Args>
typename std::enable_if<all_are<UnpositionedPlaceholder, Args...>::value,
                        PlaceholderList>::type
access(Args... args) {
  PlaceholderList result;
  int pos = 0;
  for (const auto &arg : {args...}) {
    result.emplace_back(arg, pos++);
  }
  return result;
}

class PlaceholderSet;

template <typename... Args> PlaceholderSet allOf(Args... args);

class PlaceholderSet {
  template <typename... Args> friend PlaceholderSet allOf(Args... args);

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

/// Build an object used to match all of the access patterns provided as
/// arguments. Individual patterns can be constructed by calling "access(...)".
template <typename... Args> PlaceholderSet allOf(Args... args) {
  static_assert(std::is_same<typename std::common_type<Args...>::type,
                             PlaceholderList>::value,
                "can only make PlaceholderSet from lists of placeholders");

  std::vector<PlaceholderList> placeholderLists = {args...};
  std::vector<std::pair<size_t, size_t>> knownIds;
  PlaceholderSet ps;
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


class Match {
public:
  // Not the same semantic as the set of placeholders.
  std::vector<std::vector<DimCandidate>> matches_;
};

std::vector<std::vector<DimCandidate>> match(isl::union_map access,
                                             PlaceholderSet ps);

} // namespace matchers

#endif // ACCESS_H
