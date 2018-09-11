#include "islutils/access_patterns.h"
#include "islutils/access.h"

namespace matchers {

std::vector<SingleInputDim>
SingleInputDim::candidates(isl::map singleOutDimMap, const SimpleAff &pattern) {
  std::vector<SingleInputDim> result = {};
  singleOutDimMap = singleOutDimMap.coalesce();
  if (!singleOutDimMap.is_single_valued()) {
    return {};
  }

  auto pma = isl::pw_multi_aff::from_map(singleOutDimMap);
  // Truly piece-wise access is not a single variable.
  if (pma.n_piece() != 1) {
    return {};
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
    candidateAff = candidateAff.scale(pattern.coefficient_);
    candidateAff = candidateAff.add_constant_val(pattern.constant_);
    auto candidatePwAff =
        isl::pw_aff(candidateAff).intersect_domain(pa.domain());
    if (pa.is_equal(candidatePwAff)) {
      result.emplace_back(SingleInputDim{i});
    }
  }

  return result;
}

isl::map SingleInputDim::transformMap(isl::map map,
                                      const SingleInputDim &candidate,
                                      const SimpleAff &pattern) {
  auto space = map.get_space();
  auto lhs = isl::aff::var_on_domain(isl::local_space(space.domain()),
                                     isl::dim::set, candidate.inputDimPos_);
  lhs = lhs.scale(pattern.coefficient_).add_constant_val(pattern.constant_);
  auto rhs = isl::aff::var_on_domain(isl::local_space(space.range()),
                                     isl::dim::set, 0);
  using map_maker::operator==;
  return lhs == rhs;
}

///////////////////////////////

// Create a relation between a point in the given space and one
// (if "all" == false) or multiple (otherwise) points in the same space
// such that the value along the last dimension of the space in the range is
// strictly greater than the value along the same dimension in the domain, and
// all other values are mutually equal.
static isl::map mapToNext(isl::space space, bool all = false) {
  using map_maker::operator==;
  using map_maker::operator<;
  int dim = space.dim(isl::dim::set);

  auto result = isl::map::universe(space.map_from_set());
  if (dim == 0) {
    return result;
  }

  for (int i = 0; i < dim - 1; ++i) {
    auto aff =
        isl::aff::var_on_domain(isl::local_space(space), isl::dim::set, i);
    result = result.intersect(aff == aff);
  }
  auto aff =
      isl::aff::var_on_domain(isl::local_space(space), isl::dim::set, dim - 1);
  auto next = aff.add_constant_si(1);
  return all ? result.intersect(aff < aff) : result.intersect(next == aff);
}

std::vector<StrideCandidate>
StrideCandidate::candidates(isl::map singleOutDimMap,
                            const StridePattern &pattern) {
  // Construct a relation between a point in space representing loops (e.g.,
  // partial schedule space) and is immediate successor in the innermost loop
  // (the last dimension).
  // By default, just add 1 to the last dimension to get the next iteration.
  // If the space is not dense, i.e. not all points in the space correspond to
  // actual loop iterations, which happens for loops with non-unit step, the set
  // of active schedule points must be provided in the pattern.  In this case,
  // take the lexicographically smallest point that is active.
  auto map = mapToNext(singleOutDimMap.get_space().domain(),
                       !pattern.nonEmptySchedulePoints.is_null());
  if (pattern.nonEmptySchedulePoints) {
    map = map.intersect_domain(pattern.nonEmptySchedulePoints)
              .intersect_range(pattern.nonEmptySchedulePoints);
    map = map.lexmin();
  }
  auto delta =
      map.apply_domain(singleOutDimMap).apply_range(singleOutDimMap).deltas();
  // TODO: also match parametric strides
  auto strideAff =
      isl::aff(isl::local_space(delta.get_space()), pattern.stride);
  auto varAff = isl::aff::var_on_domain(isl::local_space(delta.get_space()),
                                        isl::dim::set, 0);
  // Since the empty set is a subset of any set, empty deltas set (caused,
  // e.g., by empty input set) would indicate a match.  However, it does not
  // make sense to say that accessed that is not performed has any meaningful
  // stride.  Consider empty deltas as an absence of match.
  using set_maker::operator==;
  if (!delta.is_empty() && delta.is_subset(strideAff == varAff))
    return {StrideCandidate{}};
  return {};
}

///////////////////
// Utility functions for FixedOutDimPattern::transformMap

std::vector<isl::map> listOf1DMaps(isl::map map) {
  std::vector<isl::map> result;
  for (int dim = map.dim(isl::dim::out); dim > 0; --dim) {
    result.push_back(map.project_out(isl::dim::out, 1, dim - 1));
    map = map.project_out(isl::dim::out, 0, 1);
  }
  return result;
}

isl::space addEmptyRange(isl::space space) {
  return space.product(space.params().set_from_params()).unwrap();
}

isl::map mapFrom1DMaps(isl::space space, const std::vector<isl::map> &list) {
  auto zeroSpace = addEmptyRange(space.domain());
  auto result = isl::map::universe(zeroSpace);
  for (const auto &m : list) {
    result = result.flat_range_product(m);
  }
  result =
      result.set_tuple_id(isl::dim::out, space.get_tuple_id(isl::dim::out));
  return result;
}

} // namespace matchers
