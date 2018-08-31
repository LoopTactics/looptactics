#include "islutils/access_patterns.h"
#include "islutils/access.h"

namespace matchers {

std::vector<SingleInputDim>
SingleInputDim::candidates(isl::map singleOutDimMap, isl::map fullMap,
                           const SimpleAff &pattern) {
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

isl::map SingleInputDim::make1DMap(const SingleInputDim &candidate,
                                   const SimpleAff &pattern, isl::space space) {
  auto lhs = isl::aff::var_on_domain(isl::local_space(space.domain()),
                                     isl::dim::set, candidate.inputDimPos_);
  lhs = lhs.scale(pattern.coefficient_).add_constant_val(pattern.constant_);
  auto rhs = isl::aff::var_on_domain(isl::local_space(space.range()),
                                     isl::dim::set, 0);
  using map_maker::operator==;
  return lhs == rhs;
}

///////////////////////////////

static isl::map mapToNext(isl::space space) {
  using map_maker::operator==;
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
  return result.intersect(next == aff);
}

std::vector<StrideCandidate>
StrideCandidate::candidates(isl::map singleOutDimMap, isl::map fullMap,
                            const StridePattern &pattern) {
  auto map = mapToNext(singleOutDimMap.get_space().domain());
  auto delta =
      map.apply_domain(singleOutDimMap).apply_range(singleOutDimMap).deltas();
  // TODO: also match parametric strides
  auto strideAff =
      isl::aff(isl::local_space(delta.get_space()), pattern.stride);
  auto varAff = isl::aff::var_on_domain(isl::local_space(delta.get_space()),
                                        isl::dim::set, 0);
  using set_maker::operator==;
  if (delta.is_subset(strideAff == varAff))
    return {StrideCandidate{}};
  return {};
}

} // namespace matchers
