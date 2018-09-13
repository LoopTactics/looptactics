#include <isl/cpp.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "islutils/locus.h"

namespace matchers {

template <typename Container> size_t containerSize(Container &&c) {
  return std::distance(c.cbegin(), c.cend());
}

// Check that, if two elements in "combination" correspond to the same values
// in "folds", they are equal, and that they are unique within "combination"
// otherwise.  Comparison is performed by calling the function objects
// "eqCompare" and "neCompare" for equality and non-equality respectively.
// While these operations are often reciprocal, this is not always the case,
// for example in tri-state logic.
// "folds" must be at least as large as "combination".
template <typename T, typename EqComparator, typename NeComparator>
bool areFoldsValid(const std::vector<T> &combination,
                   const std::vector<size_t> &folds, EqComparator eqCompare,
                   NeComparator neCompare) {
  // Algorithmically not the most efficient way of finding duplicates, but
  // removes the need to include hash-tables and/or perform additional
  // allocations.
  size_t size = combination.size();
  if (size > folds.size()) {
    ISLUTILS_DIE("folds are not properly set up");
  }

  for (size_t i = 0; i < size; ++i) {
    for (size_t j = i + 1; j < size; ++j) {
      if (folds[i] == folds[j]) {
        if (neCompare(combination.at(i), combination.at(j))) {
          return false;
        } else {
          continue;
        }
      }
      if (eqCompare(combination.at(i), combination.at(j))) {
        return false;
      }
    }
  }
  return true;
}

// All placeholders should get different assignments, except those that belong
// to the same fold which should get equal assignments modulo matched map.
template <typename CandidatePayload, typename PatternPayload>
inline bool hasNoDuplicateAssignments(
    const std::vector<DimCandidate<CandidatePayload>> &combination,
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps) {
  return areFoldsValid(combination, ps.placeholderFolds_,
                       [](const DimCandidate<CandidatePayload> &left,
                          const DimCandidate<CandidatePayload> &right) {
                         return left.isEqualModuloMap(right);
                       },
                       [](const DimCandidate<CandidatePayload> &left,
                          const DimCandidate<CandidatePayload> &right) {
                         return !left.isEqualModuloMap(right);
                       });
}

// All placeholders in a group are either not yet matched, or matched the same
// map.  A map matched in the group is not matched in any previous group.
template <typename CandidatePayload, typename PatternPayload>
bool groupsAreProperlyFormed(
    const std::vector<DimCandidate<CandidatePayload>> &combination,
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps) {
  std::vector<isl::space> previouslyMatchedSpaces;
  for (const auto &group : ps.placeholderGroups_) {
    isl::space matchedSpace;
    // Ignore parts that are not yet matched.
    for (size_t pos : group) {
      if (pos >= combination.size()) {
        continue;
      }
      auto candidateSpace = combination.at(pos).candidateMapSpace_;
      if (matchedSpace) { // A group has already matched a map.
        // If matched a different map, groups are not a match.
        if (!matchedSpace.is_equal(candidateSpace)) {
          return false;
        }
      } else { // First time a map is matched in the group.
        matchedSpace = candidateSpace;
        auto it = std::find(previouslyMatchedSpaces.begin(),
                            previouslyMatchedSpaces.end(), matchedSpace);
        // If the same map as one of the previously considered groups, groups
        // are not a match.
        if (it != previouslyMatchedSpaces.end()) {
          return false;
        }
        previouslyMatchedSpaces.push_back(matchedSpace);
      }
    }
  }
  return true;
}

template <typename CandidatePayload, typename PatternPayload>
bool PlaceholderSet<CandidatePayload, PatternPayload>::isSuitableCombination(
    const std::vector<DimCandidate<CandidatePayload>> &combination) const {
  return hasNoDuplicateAssignments(combination, *this) &&
         groupsAreProperlyFormed(combination, *this);
}

template <typename CandidatePayload>
static inline isl::space
findSpace(const std::vector<size_t> &group,
          const std::vector<DimCandidate<CandidatePayload>> &combination) {
  for (auto idx : group) {
    if (idx >= combination.size()) {
      continue;
    }
    return combination.at(idx).candidateMapSpace_;
  }
  return isl::space();
}

// Handle both right-tagged and untagged access relation spaces,
// [] -> [__ref_tagX[] -> arrayID[]]
// [] -> arrayID[]
// and return arrayID.  Return a null isl::id if there is no tuple tuple id at
// the expected location.
static inline isl::id extractArrayId(isl::space accessSpace) {
  auto rangeSpace = accessSpace.range();
  if (rangeSpace.is_wrapping()) {
    rangeSpace = rangeSpace.unwrap().range();
  }
  if (!rangeSpace.has_tuple_id(isl::dim::set)) {
    return isl::id();
  }
  return rangeSpace.get_tuple_id(isl::dim::set);
}

// Compare if two groups (containing indexes of candidates in "combination")
// matched the same array (if "equality" is set) or different arrays (if
// "equality" is not set).  If it is impossible to determine the array that
// matched at least one of the group, e.g., in case of partial combination,
// return false for both equality and inequality checks.
// Array ids are expected to be tuple ids of the rightmost set space in the
// matched space.  Two absent ids are interpreted as equal since the
// corresponding spaces are considered equal.  All groups are expected to match
// exactly one space.
template <typename CandidatePayload>
bool compareGroupsBelongToSameArray(
    const std::vector<size_t> &group1, const std::vector<size_t> &group2,
    const std::vector<DimCandidate<CandidatePayload>> &combination,
    bool equality) {
  // By this point, we should know that any placeholder in the group matched
  // the same space.
  isl::space space1 = findSpace(group1, combination);
  isl::space space2 = findSpace(group2, combination);
  // If one of the groups has no placeholder with assigned candidates, consider
  // as different.
  if (space1.is_null() || space2.is_null()) {
    return false;
  }
  isl::id id1 = extractArrayId(space1);
  isl::id id2 = extractArrayId(space2);
  return (id1 == id2) ^ !equality;
}

// In addition to PlaceholderSet::isSuitableCombination checks for
// candidate/placeholder uniqueness and group formation, check that groups that
// belong to the same group fold have matched the same array while gruops that
// belong to different group folds matched different arrays.
template <typename CandidatePayload, typename PatternPayload>
bool PlaceholderGroupedSet<CandidatePayload, PatternPayload>::
    isSuitableCombination(
        const std::vector<DimCandidate<CandidatePayload>> &combination) const {
  return static_cast<const PlaceholderSet<CandidatePayload, PatternPayload> &>(
             *this)
             .isSuitableCombination(combination) &&
         areFoldsValid(this->placeholderGroups_, placeholderGroupFolds_,
                       [&combination](const std::vector<size_t> &group1,
                                      const std::vector<size_t> &group2) {
                         return compareGroupsBelongToSameArray(
                             group1, group2, combination, true);
                       },
                       [&combination](const std::vector<size_t> &group1,
                                      const std::vector<size_t> &group2) {
                         return compareGroupsBelongToSameArray(
                             group1, group2, combination, false);
                       });
}

template <typename CandidatePayload, typename PatternPayload>
Match<CandidatePayload, PatternPayload>::Match(
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps,
    const std::vector<DimCandidate<CandidatePayload>> &combination) {
  if (containerSize(ps) != combination.size()) {
    ISLUTILS_DIE("expected the same number of placeholders and candidates");
  }

  size_t idx = 0;
  for (const auto &candidate : combination) {
    placeholderValues_.emplace_back(ps.placeholders_[idx++].id_, candidate);
  }
}

template <typename PlaceholderCollectionTy>
void recursivelyCheckCombinations(
    const PlaceholderCollectionTy &ps,
    std::vector<DimCandidate<typename PlaceholderCollectionTy::CandidateTy>>
        partialCombination,
    Matches<typename PlaceholderCollectionTy::CandidateTy,
            typename PlaceholderCollectionTy::PatternTy> &suitableCandidates) {

  if (!ps.isSuitableCombination(partialCombination)) {
    return;
  }

  // The partial combination is known to be suitable. If it is also full, add
  // it to the list and be done.
  if (partialCombination.size() == containerSize(ps)) {
    suitableCandidates.emplace_back(ps, partialCombination);
    return;
  }

  // Otherwise, try adding one element to the combination and recurse.
  auto pos = partialCombination.size();
  for (const auto &candidate : ps.placeholders_[pos].candidates_) {
    partialCombination.push_back(candidate);
    recursivelyCheckCombinations(ps, partialCombination, suitableCandidates);
    partialCombination.pop_back();
  }
}

template <typename PlaceholderCollectionTy>
Matches<typename PlaceholderCollectionTy::CandidateTy,
        typename PlaceholderCollectionTy::PatternTy>
suitableCombinations(const PlaceholderCollectionTy &ps) {
  Matches<typename PlaceholderCollectionTy::CandidateTy,
          typename PlaceholderCollectionTy::PatternTy>
      result;
  recursivelyCheckCombinations(ps, {}, result);
  return result;
}

template <typename PlaceholderCollectionTy>
Matches<typename PlaceholderCollectionTy::CandidateTy,
        typename PlaceholderCollectionTy::PatternTy>
match(isl::union_map access, PlaceholderCollectionTy ps) {
  std::vector<isl::map> accesses;
  access.foreach_map([&accesses](isl::map m) { accesses.push_back(m); });

  // Stage 1: fill in the candidate lists for all placeholders.
  for (auto &ph : ps) {
    for (auto acc : accesses) {
      for (auto &&c :
           PlaceholderCollectionTy::CandidateTy::candidates(acc, ph.pattern_)) {
        ph.candidates_.emplace_back(c, acc.get_space());
      }
    }
    // Early exit if one of the placeholders has no candidates.
    if (ph.candidates_.empty()) {
      return {};
    }
  }

  // Stage 2: generate all combinations of values replacing the placeholders
  // while filtering incompatible ones immediately.
  return suitableCombinations(ps);
}

template <typename CandidatePayload, typename PatternPayload, typename... Args>
isl::map transformOneMap(
    isl::map map, const Match<CandidatePayload, PatternPayload> &oneMatch,
    Replacement<CandidatePayload, PatternPayload> arg, Args... args) {
  static_assert(
      std::is_same<
          typename std::common_type<
              Replacement<CandidatePayload, PatternPayload>, Args...>::type,
          Replacement<CandidatePayload, PatternPayload>>::value,
      "");

  isl::map result;
  for (const auto &rep : {arg, args...}) {
    // separability of matches is important!
    // if we match here something that we would not have matched with the whole
    // set, it's bad!  But here we know that the map has already matched with
    // one of the groups in the set, we just don't know which one.  If it
    // matches two groups, this means the transformation would happen twice,
    // which we expicitly disallow.
    if (match(isl::union_map(map), allOf(rep.pattern)).empty()) {
      continue;
    }
    if (!result.is_null()) {
      ISLUTILS_DIE("one relation matched multiple patterns\n"
                   "the transformation is undefined");
    }
    // Actual transformation.
    result = map;
    for (const auto &plh : rep.replacement) {
      result = CandidatePayload::transformMap(result, oneMatch[plh].payload(),
                                              plh.pattern_);
    }
  }
  return result;
}

template <typename CandidatePayload, typename PatternPayload, typename... Args>
isl::union_map findAndReplace(isl::union_map umap,
                              Replacement<CandidatePayload, PatternPayload> arg,
                              Args... args) {
  static_assert(
      std::is_same<
          typename std::common_type<
              Replacement<CandidatePayload, PatternPayload>, Args...>::type,
          Replacement<CandidatePayload, PatternPayload>>::value,
      "");

  // make a vector of maps
  // for each match,
  //   find all corresponding maps,
  //     if not found, the map was deleted already meaning there was an attempt
  //     of double transformation
  //   remove them from vector,
  //   transform them and
  //   add them to the resulting vector
  // finally, copy all the remaining original maps as is into result

  std::vector<isl::map> originalMaps, transformedMaps;
  umap.foreach_map([&originalMaps](isl::map m) { originalMaps.push_back(m); });

  auto getPattern =
      [](const Replacement<CandidatePayload, PatternPayload> &replacement) {
        return replacement.pattern;
      };

  auto ps = allOf(getPattern(arg), getPattern(args)...);
  auto matches = match(umap, ps);

  for (const auto &m : matches) {
    std::vector<isl::map> toTransform;
    for (const auto &plh : ps.placeholders_) {
      auto spaces = m[plh].candidateSpaces();
      for (auto candidate : spaces) {
        auto found = std::find_if(toTransform.begin(), toTransform.end(),
                                  [candidate](isl::map map) {
                                    return map.get_space().is_equal(candidate);
                                  });
        if (found != toTransform.end()) {
          continue;
        }
        toTransform.push_back(umap.extract_map(candidate));
      }
    }

    for (const auto &candidate : toTransform) {
      auto found = std::find_if(
          originalMaps.begin(), originalMaps.end(),
          [candidate](isl::map map) { return map.is_equal(candidate); });
      if (found == originalMaps.end()) {
        ISLUTILS_DIE("could not find the matched map\n"
                     "this typically means a map was matched more than once\n"
                     "in which case the transformation is undefined");
        continue;
      }
      originalMaps.erase(found);

      auto r = transformOneMap<CandidatePayload, PatternPayload>(candidate, m,
                                                                 arg, args...);
      transformedMaps.push_back(r);
    }
  }

  for (const auto &map : originalMaps) {
    transformedMaps.push_back(map);
  }

  if (transformedMaps.empty()) {
    return isl::union_map::empty(umap.get_space());
  }
  auto result = isl::union_map(transformedMaps.front());
  for (const auto &map : transformedMaps) {
    result = result.unite(isl::union_map(map));
  }
  return result;
}

template <typename TargetPatternPayload, typename CandidatePayload,
          typename SourcePatternPayload>
Placeholder<CandidatePayload, TargetPatternPayload>
pattern_cast(Placeholder<CandidatePayload, SourcePatternPayload> placeholder) {
  return Placeholder<CandidatePayload, TargetPatternPayload>(
      static_cast<TargetPatternPayload>(placeholder.pattern_), placeholder.id_);
}

/// Counter used to create per-thread unique-ish placeholder ids for the
/// purpose of folding.
template <typename CandidatePayload, typename PatternPayload>
thread_local size_t Placeholder<CandidatePayload, PatternPayload>::nextId_ = 0;

/// Build an object used to match all of the access patterns provided as
/// arguments. Individual patterns can be constructed by calling "access(...)".
template <typename CandidatePayload, typename PatternPayload, typename... Args>
PlaceholderSet<CandidatePayload, PatternPayload>
allOf(PlaceholderList<CandidatePayload, PatternPayload> arg, Args... args) {
  static_assert(all_are<PlaceholderList<CandidatePayload, PatternPayload>,
                        PlaceholderList<CandidatePayload, PatternPayload>,
                        Args...>::value,
                "can only make PlaceholderSet from PlaceholderLists "
                "with the same payload types");

  std::vector<PlaceholderList<CandidatePayload, PatternPayload>>
      placeholderLists = {arg, args...};
  std::vector<std::pair<size_t, size_t>> knownIds;
  PlaceholderSet<CandidatePayload, PatternPayload> ps;
  for (const auto &pl : placeholderLists) {
    if (pl.empty()) {
      continue;
    }

    size_t index = containerSize(ps);
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

template <typename CandidatePayload, typename PatternPayload>
template <typename PPayload>
MatchCandidates<CandidatePayload> Match<CandidatePayload, PatternPayload>::
operator[](const Placeholder<CandidatePayload, PPayload> &pl) const {
  // If pattern_cast from PatterPayload to PPayload cannot be instantiated,
  // Placeholder<CandidatePayload, PPayload> cannot be a valid key to lookup
  // match results.
  static_assert(
      std::is_same<
          decltype(pattern_cast<PPayload>(
              std::declval<Placeholder<CandidatePayload, PatternPayload>>())),
          Placeholder<CandidatePayload, PPayload>>::value,
      "incompatible pattern types");

  auto result = MatchCandidates<CandidatePayload>();

  for (const auto &kvp : placeholderValues_) {
    if (kvp.first == pl.id_) {
      if (result.candidateSpaces_.empty()) {
        result.payload_ = kvp.second.payload_;
      } else if (!(result.payload_ == kvp.second.payload_)) {
        ISLUTILS_DIE("different payloads for the same placeholder");
      }
      if (std::find(
              result.candidateSpaces_.begin(), result.candidateSpaces_.end(),
              kvp.second.candidateMapSpace_) != result.candidateSpaces_.end()) {
        continue;
      }
      result.candidateSpaces_.push_back(kvp.second.candidateMapSpace_);
    }
  }

  if (result.candidateSpaces_.empty()) {
    ISLUTILS_DIE("no match for the placeholder although matches found");
  }
  return result;
}

} // namespace matchers
