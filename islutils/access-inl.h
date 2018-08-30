#include <isl/cpp.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "islutils/locus.h"

namespace matchers {

void SingleInputDim::appendToCandidateList(
    isl::map singleOutDimMap, isl::map fullMap,
    Placeholder<SingleInputDim, SimpleAff> &placeholder) {
  singleOutDimMap = singleOutDimMap.coalesce();
  if (!singleOutDimMap.is_single_valued()) {
    return;
  }

  auto pma = isl::pw_multi_aff::from_map(singleOutDimMap);
  // Truly piece-wise access is not a single variable.
  if (pma.n_piece() != 1) {
    return;
  }
  auto pa = pma.get_pw_aff(0);
  isl::aff a;
  pa.foreach_piece([&a](isl::set, isl::aff aff) {
    if (!a.is_null()) {
      ISLUTILS_DIE("unexpected second piece");
    }
    a = aff;
  });

  int dim = singleOutDimMap.dim(isl::dim::in);
  auto space = singleOutDimMap.get_space();
  auto lspace = isl::local_space(space.domain());
  for (int i = 0; i < dim; ++i) {
    auto candidateAff = isl::aff::var_on_domain(lspace, isl::dim::set, i);
    candidateAff = candidateAff.scale(placeholder.pattern_.coefficient_);
    candidateAff =
        candidateAff.add_constant_val(placeholder.pattern_.constant_);
    auto candidatePwAff =
        isl::pw_aff(candidateAff).intersect_domain(pa.domain());
    if (pa.is_equal(candidatePwAff)) {
      placeholder.candidates_.emplace_back(SingleInputDim{i}, fullMap);
    }
  }
}

template <typename CandidatePayload, typename PatternPayload>
void appendToCandidateList(
    isl::map singleOutDimMap, isl::map fullMap,
    Placeholder<CandidatePayload, PatternPayload> &placeholder) {
  CandidatePayload::appendToCandidateList(singleOutDimMap, fullMap,
                                          placeholder);
}

// All placeholders should get different assignments, except those that belong
// to the same fold which should get equal assignments modulo matched map.
template <typename CandidatePayload, typename PatternPayload>
bool hasNoDuplicateAssignments(
    const std::vector<DimCandidate<CandidatePayload>> &combination,
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps) {
  // Algorithmically not the most efficient way of finding duplicates, but
  // removes the need to include hash-tables and/or perform additional
  // allocations.
  size_t size = combination.size();
  if (ps.placeholders_.size() != ps.placeholderFolds_.size()) {
    ISLUTILS_DIE("placeholder folds are not properly set up");
  }

  for (size_t i = 0; i < size; ++i) {
    for (size_t j = i + 1; j < size; ++j) {
      if (ps.placeholderFolds_[i] == ps.placeholderFolds_[j]) {
        if (!combination.at(i).isEqualModuloMap(combination.at(j))) {
          return false;
        } else {
          continue;
        }
      }
      if (combination.at(i).isEqualModuloMap(combination.at(j))) {
        return false;
      }
    }
  }
  return true;
}

// All placeholders in a group are either not yet matched, or matched the same
// map.  A map matched in the group is not matched in any previous group.
template <typename CandidatePayload, typename PatternPayload>
bool groupsAreProperlyFormed(
    const std::vector<DimCandidate<CandidatePayload>> &combination,
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps) {
  std::vector<isl::map> previouslyMatchedMaps;
  for (const auto &group : ps.placeholderGroups_) {
    isl::map matchedMap;
    // Ignore parts that are not yet matched.
    for (size_t pos : group) {
      if (pos >= combination.size()) {
        continue;
      }
      auto candidateMap = combination.at(pos).candidateMap_;
      if (matchedMap) { // A group has already matched a map.
        // If matched a different map, groups are not a match.
        if (matchedMap != candidateMap) {
          return false;
        }
      } else { // First time a map is matched in the group.
        matchedMap = candidateMap;
        auto it = std::find(previouslyMatchedMaps.begin(),
                            previouslyMatchedMaps.end(), matchedMap);
        // If the same map as one of the previously considered groups, groups
        // are not a match.
        if (it != previouslyMatchedMaps.end()) {
          return false;
        }
        previouslyMatchedMaps.push_back(matchedMap);
      }
    }
  }
  return true;
}

template <typename CandidatePayload, typename PatternPayload>
Match<CandidatePayload, PatternPayload>::Match(
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps,
    const std::vector<DimCandidate<CandidatePayload>> &combination) {
  if (ps.placeholders_.size() != combination.size()) {
    ISLUTILS_DIE("expected the same number of placeholders and candidates");
  }

  size_t idx = 0;
  for (const auto &candidate : combination) {
    placeholderValues_.emplace_back(ps.placeholders_[idx++].id_, candidate);
  }
}

template <typename CandidatePayload, typename PatternPayload, typename FilterTy>
void recursivelyCheckCombinations(
    const PlaceholderSet<CandidatePayload, PatternPayload> &ps,
    std::vector<DimCandidate<CandidatePayload>> partialCombination,
    FilterTy filter,
    Matches<CandidatePayload, PatternPayload> &suitableCandidates) {
  static_assert(
      std::is_same<
          decltype(filter(
              std::declval<std::vector<DimCandidate<CandidatePayload>>>())),
          bool>::value,
      "unexpected type of the callable filter");

  if (!filter(partialCombination)) {
    return;
  }

  // At this point, the partialCombination is full and has been checked to pass
  // the filter.
  if (partialCombination.size() == ps.placeholders_.size()) {
    suitableCandidates.emplace_back(ps, partialCombination);
    return;
  }

  auto pos = partialCombination.size();
  for (const auto &candidate : ps.placeholders_[pos].candidates_) {
    partialCombination.push_back(candidate);
    recursivelyCheckCombinations(ps, partialCombination, filter,
                                 suitableCandidates);
    partialCombination.pop_back();
  }
}

template <typename CandidatePayload, typename PatternPayload, typename FilterTy>
Matches<CandidatePayload, PatternPayload>
suitableCombinations(const PlaceholderSet<CandidatePayload, PatternPayload> &ps,
                     FilterTy filter) {
  Matches<CandidatePayload, PatternPayload> result;
  recursivelyCheckCombinations(ps, {}, filter, result);
  return result;
}

template <typename CandidatePayload, typename PatternPayload>
Matches<CandidatePayload, PatternPayload>
match(isl::union_map access,
      PlaceholderSet<CandidatePayload, PatternPayload> ps) {
  std::vector<isl::map> accesses;
  access.foreach_map([&accesses](isl::map m) { accesses.push_back(m); });

  // TODO: how do we separate maps?  ref ids?

  std::vector<std::vector<
      std::reference_wrapper<Placeholder<CandidatePayload, PatternPayload>>>>
      outDimPlaceholders;
  for (auto &ph : ps.placeholders_) {
    if (static_cast<size_t>(ph.outDimPos_) >= outDimPlaceholders.size()) {
      outDimPlaceholders.resize(ph.outDimPos_ + 1);
    }
    outDimPlaceholders[ph.outDimPos_].push_back(ph);
  }

  // Stage 1: collect candidate values for each placeholder.
  // This is a compact way of stroing a cross-product of all combinations of
  // values replacing the placeholders.
  for (auto acc : accesses) {
    for (size_t i = 0, ei = outDimPlaceholders.size(); i < ei; ++i) {
      const auto &dimPlaceholders = outDimPlaceholders[i];
      if (dimPlaceholders.empty()) {
        continue;
      }
      auto dim = acc.dim(isl::dim::out);
      if (i >= dim) {
        continue;
      }
      auto single = acc.project_out(isl::dim::out, i + 1, dim - (i + 1))
                        .project_out(isl::dim::out, 0, i);
      for (size_t j = 0, ej = dimPlaceholders.size(); j < ej; ++j) {
        // If there is a lot of placeholders with the same coefficient, we want
        // to also group placeholders by coefficient and only call the
        // aff-matching computation once per coefficient.  Punting for now.
        appendToCandidateList(single, acc, dimPlaceholders[j].get());
      }
    }
  }

  // Early exit if one of the placeholders has no candidates.
  for (const auto &ph : ps.placeholders_) {
    if (ph.candidates_.empty()) {
      return {};
    }
  }

  // Stage 2: generate all combinations of values replacing the placeholders
  // while filtering incompatible ones immediately.
  // TODO: customize the filter for acceptable combinations.
  // Note that the filter must work on incomplete candidates for the
  // branch-and-cut to work.  It can return "true" for incomplete candidates
  // and only actually check complete candidates, but would require enumerating
  // them all.
  // Note also that the filter might be doing duplicate work: in the
  // hasNoDuplicateAssignments example, there is not need to check all pairs in
  // the N-element list if we know that elements of (N-1) array are unique.
  // This algorithmic optimization requires some API changes and is left for
  // future work.
  return suitableCombinations<CandidatePayload, PatternPayload>(
      ps, [ps](const std::vector<DimCandidate<CandidatePayload>> &candidate) {
        return hasNoDuplicateAssignments(candidate, ps) &&
               groupsAreProperlyFormed(candidate, ps);
      });
}

static inline std::vector<isl::map> listOf1DMaps(isl::map map) {
  std::vector<isl::map> result;
  for (int dim = map.dim(isl::dim::out); dim > 0; --dim) {
    result.push_back(map.project_out(isl::dim::out, 1, dim - 1));
    map = map.project_out(isl::dim::out, 0, 1);
  }
  return result;
}

static inline isl::space addEmptyRange(isl::space space) {
  return space.product(space.params().set_from_params()).unwrap();
}

static inline isl::map mapFrom1DMaps(isl::space space,
                                     const std::vector<isl::map> &list) {
  auto zeroSpace = addEmptyRange(space.domain());
  auto result = isl::map::universe(zeroSpace);
  for (const auto &m : list) {
    result = result.flat_range_product(m);
  }
  result =
      result.set_tuple_id(isl::dim::out, space.get_tuple_id(isl::dim::out));
  return result;
}

isl::map SingleInputDim::make1DMap(
    const DimCandidate<SingleInputDim> &dimCandidate,
    const UnpositionedPlaceholder<SingleInputDim, SimpleAff> &placeholder,
    isl::space space) {
  auto lhs =
      isl::aff::var_on_domain(isl::local_space(space.domain()), isl::dim::set,
                              dimCandidate.payload_.inputDimPos_);
  lhs = lhs.scale(placeholder.pattern_.coefficient_)
            .add_constant_val(placeholder.pattern_.constant_);
  auto rhs = isl::aff::var_on_domain(isl::local_space(space.range()),
                                     isl::dim::set, 0);
  using map_maker::operator==;
  return lhs == rhs;
}

template <typename CandidatePayload, typename PatternPayload>
inline isl::map
make1DMap(const DimCandidate<CandidatePayload> &dimCandidate,
          const UnpositionedPlaceholder<CandidatePayload, PatternPayload>
              &placeholder,
          isl::space space) {
  return CandidatePayload::make1DMap(dimCandidate, placeholder, space);
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
    if (map.dim(isl::dim::out) == 0) {
      result = map;
      continue;
    }
    auto list = listOf1DMaps(map);
    auto space1D =
        addEmptyRange(map.get_space().domain()).add_dims(isl::dim::out, 1);
    for (const auto &plh : rep.replacement) {
      list[plh.outDimPos_] = make1DMap(oneMatch[plh], plh, space1D);
    }
    result = mapFrom1DMaps(map.get_space(), list);
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
      auto candidate = m[plh].candidateMap_;
      auto found = std::find_if(
          toTransform.begin(), toTransform.end(),
          [candidate](isl::map map) { return map.is_equal(candidate); });
      if (found != toTransform.end()) {
        continue;
      }
      toTransform.push_back(candidate);
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

//-----------------//
#if 0
class DimPlaceholder {
 public:
  DimPlaceholder(isl::space space, int dimPos, isl::val constant) :
    space_(space), dimPos_(dimPos),
    constant_(constant.is_null() ? isl::val::zero(space.get_ctx()) : constant) {}

  isl::aff asAff() const {
    if (dimPos_ == -1) {
      return isl::aff(isl::local_space(space_), constant);
    } else {
      return isl::aff::var_on_domain(isl::local_space(space_), isl::dim_type::set, dimPos_)
        .add_constant_si(constant_);
    }
  }

 private:
  isl::space space_ = isl::space();
  int dimPos_ = -1;
  isl::val constant_ = isl::constant();
};

class GroupDimPlaceholder {
public:
  /*implicit*/ GroupDimPlaceholder(const DimPlaceholder& dimPlaceholder) :
    candidates_({dimPlaceholder}) {}

  GroupDimPlaceholder(std::initializer_list<DimPlaceholder> list) :
    candidates_(list.begin(), list.end()) {}

  GroupDimPlaceholder(std::vector<DimPlaceholder>&& placeholders):
    candidates_(placeholders) {}

  void filter(std::function<bool(const DimPlaceholder&)>);

  inline bool empty() { return candidates_.empty(); }

private:
  std::vector<DimPlaceholder> candidates_;
};

void GroupDimPlaceholder::filter(std::function<bool(const DimPlaceholder)> condition) {
  for (size_t i = candidates_.size(); i > 0; --i) {
    if (!condition(candidates_.at(i - 1))) {
      candidates_.erase(candidates_.begin() + (i - 1));
    }
  }
}

// DimPlaceholderSet st;
// auto i = st(0);
// auto j = st(1);  // i != j

// DimPlaceholderSet st;
// auto i = st(0);  // must be same everywhere it is used

// auto i = DimPlaceholder();
// auto j = DimPlaceholder(); // may be i == j, may be not

// Define _1, _2, _3 similarly to std::bind?

class AccessDimMatcher {

private:
  std::function<bool(isl::map)> callback_;
};

enum class AccessType {
  Read,
  Write
};

class AccessMatcher {

private:
  std::vector<AccessDimMatcher> dimMatchers_;
  AccessType type_;
};

class AccessMatch {
  // combination of assignments to placeholders, i.e. non-null DimPlaceholders
  // that correspond to the matcher inputs (wildcards or bound)
  // note that this is semantically different from GroupDimPlaceholder, but
  // uses the same data structure, which is wierd.
  
  // candidate list should constain possible assignments (or be able to
  // generate them lazily) for _all_ variables, rather than individual
  // variables like it does now with GroupDimPlaceholder
  std::vector<DimPlaceholder> match;
}

class AccessMatches {

private:
  AccessMatch template_;
  std::vector<AccessMatch> matches_;

  // Lazy version
  // void addFilter(std::function<bool(const AccessMatch&)>);
  // std::vector<std::function<bool(const AccessMatch&)>> filters_;
  //
  // void foreach(std::function<void (const AccessMatch&)>) {
  //   generate next candidate
  //   if passes filters, call the callback
  //
  // for pruning, next candidate generation may be per-access-dim, and filters should be aware of that
  //
  // std::vector<AccessMatch> all(); // returns all active
}

std::vector<AccessMatch> match(const AccessMatcher& matcher, isl::map_list accesses) {
}

bool isVar(isl::map singleOutDimMap);

// TODO: two cases:
// - one, we want to capture the schedule dimension (can be a set of candidates)
// - two, we know what exactly to look for (can also be a set of candidates)
bool isVarConstImpl(isl::map singleOutDimMap, GroupDimPlaceholder& group) {
  singleOutDimMap = singleOutDimMap.coalesce();
  if (singleOutDimMap.dim(isl::dim_type::out) != 1) {
    assert(false && "expected one output dimension");
  }

  if (!singleOutDimMap.is_single_valued()) {
    return false;
  }

  auto pma = isl::pw_multi_aff::from_map(singleOutDimMap);
  // Truly piece-wise access is not a single variable.
  if (pma.n_piece() != 1) {
    return false;
  }
  auto pa = pma.get_pw_aff(0);
  isl::aff a;
  pa.foreach_piece([&a](isl::set, isl::aff aff) {
    if (!a.is_null()) {
      assert(false && "unexpected second piece");
    }
    a = aff;
  });

  // Matching any dim...
  if (var.space.is_null()) {
    auto space = singleOutDimMap.get_space();
    auto domain = singleOutDimMap.domain();
    auto localSpace = isl::local_space(space.domain());
    auto constant = a.get_constant_val();
    auto constantAff = isl::aff(localSpace, constant);
    int dim = space.dim(isl::dim_type::in);
    std::vector<DimPlaceholder> dimPlaceholders;
    for (int i = 0; i < dim; ++i) {
      auto candidate = DimPlaceholder(space, i, constant);
      auto candidateAff = isl::pw_aff(candidate.asAff()).intersect_domain(domain).add(isl::pw_aff(constantAff));
      if (pa.is_equal(candidateAff)) {
        dimPlaceholders.emplace(candidate);
      }
    }
    group = GroupDimPlaceholder(std::move(dimPlaceholders));
  } else {
    group.filter([pa,singleOutDimMap](const DimPlaceholder &var) {
      auto candidate = isl::pw_aff(var.asAff()).intersect_domain(singleOutDimMap.domain());
      return pa.is_equal(candidate);
    });
  }

  return !group.empty();
}
#endif

} // namespace matchers
