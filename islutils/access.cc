#include <isl/cpp.h>

#include <algorithm>
#include <functional>
#include <vector>

#ifdef ISLUTILS_EXCEPTIONS
#include <stdexcept>
#else
#include <cassert>
#endif

#ifdef ISLUTILS_EXCEPTIONS
#define ISLUTILS_DIE(message) throw std::logic_error(message);
#else
#define ISLUTILS_DIE(message) assert(false && (message));
#endif

#include "islutils/access.h"

namespace matchers {

static void appendToCandidateList(isl::map singleOutDimMap, isl::map fullMap,
                                  Placeholder &placeholder) {
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
    candidateAff = candidateAff.scale(placeholder.coefficient_);
    candidateAff = candidateAff.add_constant_val(placeholder.constant_);
    auto candidatePwAff =
        isl::pw_aff(candidateAff).intersect_domain(pa.domain());
    if (pa.is_equal(candidatePwAff)) {
      placeholder.candidates_.emplace_back(i, fullMap);
    }
  }
}

// All placeholders should get different assignments, except those that belong
// to the same fold which should get equal assignments modulo matched map.
static bool
hasNoDuplicateAssignments(const std::vector<DimCandidate> &combination,
                          const PlaceholderSet &ps) {
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
      if (combination.at(i) == combination.at(j)) {
        return false;
      }
    }
  }
  return true;
}

// All placeholders in a group are either not yet matched, or matched the same
// map.  A map matched in the group is not matched in any previous group.
static bool
groupsAreProperlyFormed(const std::vector<DimCandidate> &combination,
                        const PlaceholderSet &ps) {
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

static void recursivelyCheckCombinations(
    const PlaceholderSet &ps, std::vector<DimCandidate> partialCombination,
    std::function<bool(const std::vector<DimCandidate> &)> filter,
    std::vector<std::vector<DimCandidate>> &suitableCandidates) {
  if (!filter(partialCombination)) {
    return;
  }

  // At this point, the partialCombination is full and has been checked to pass
  // the filter.
  if (partialCombination.size() == ps.placeholders_.size()) {
    suitableCandidates.push_back(partialCombination);
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

static std::vector<std::vector<DimCandidate>> suitableCombinations(
    const PlaceholderSet &ps,
    std::function<bool(const std::vector<DimCandidate> &)> filter) {
  std::vector<std::vector<DimCandidate>> result;
  recursivelyCheckCombinations(ps, {}, filter, result);
  return result;
}

std::vector<std::vector<DimCandidate>> match(isl::union_map access,
                                             PlaceholderSet &ps) {
  std::vector<isl::map> accesses;
  access.foreach_map([&accesses](isl::map m) { accesses.push_back(m); });

  // TODO: how do we separate maps?  ref ids?

  std::vector<std::vector<std::reference_wrapper<Placeholder>>>
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
  // TODO: customize the filter for acceptable individual candidates.
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
  return suitableCombinations(
      ps, [ps](const std::vector<DimCandidate> &candidate) {
        return hasNoDuplicateAssignments(candidate, ps) &&
               groupsAreProperlyFormed(candidate, ps);
      });
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
