#ifndef ISLUTILS_LOCUS_H
#define ISLUTILS_LOCUS_H

#include <isl/cpp.h>

namespace set_maker {

/// \defgroup AffSetMaker Constructing isl::set from isl::aff
/// \{
/// Overloaded operators building a set in the (common) space of the given
/// affine functions.  The set is constrained by the selected binary operation
/// between the given affine functions.
isl::set operator==(isl::aff lhs, isl::aff rhs);
isl::set operator!=(isl::aff lhs, isl::aff rhs);
isl::set operator>=(isl::aff lhs, isl::aff rhs);
isl::set operator>(isl::aff lhs, isl::aff rhs);
isl::set operator<=(isl::aff lhs, isl::aff rhs);
isl::set operator<(isl::aff lhs, isl::aff rhs);
/// \}

/// \defgroup PwAffSetMaker Constructing isl::set from isl::pw_aff
/// \{
/// Overloaded operators building a set in the (common) space of the given
/// piecewise affine functions.  The set is constrained by the selected binary
/// operation between the given piecewise affine functions.
isl::set operator==(isl::pw_aff lhs, isl::pw_aff rhs);
isl::set operator!=(isl::pw_aff lhs, isl::pw_aff rhs);
isl::set operator>=(isl::pw_aff lhs, isl::pw_aff rhs);
isl::set operator>(isl::pw_aff lhs, isl::pw_aff rhs);
isl::set operator<=(isl::pw_aff lhs, isl::pw_aff rhs);
isl::set operator<(isl::pw_aff lhs, isl::pw_aff rhs);
/// \}
} // namespace set_maker

namespace map_maker {

/// \defgroup AffMapMaker Constructing isl::map from isl::aff
/// \{
/// Overloaded operators building a map in the (common) space of the given
/// affine functions.  The map is constrained by the selected binary operation
/// between the given affine functions.
isl::map operator==(isl::aff lhs, isl::aff rhs);
isl::map operator!=(isl::aff lhs, isl::aff rhs);
isl::map operator>=(isl::aff lhs, isl::aff rhs);
isl::map operator>(isl::aff lhs, isl::aff rhs);
isl::map operator<=(isl::aff lhs, isl::aff rhs);
isl::map operator<(isl::aff lhs, isl::aff rhs);
/// \}

/// \defgroup PwAffMapMaker Constructing isl::map from isl::pw_aff
/// \{
/// Overloaded operators building a map in the (common) space of the given
/// piecewise affine functions.  The map is constrained by the selected binary
/// operation between the given piecewise affine functions.
isl::map operator==(isl::pw_aff lhs, isl::pw_aff rhs);
isl::map operator!=(isl::pw_aff lhs, isl::pw_aff rhs);
isl::map operator>=(isl::pw_aff lhs, isl::pw_aff rhs);
isl::map operator>(isl::pw_aff lhs, isl::pw_aff rhs);
isl::map operator<=(isl::pw_aff lhs, isl::pw_aff rhs);
isl::map operator<(isl::pw_aff lhs, isl::pw_aff rhs);
/// \}
} // namespace map_maker

#include "locus-inl.h"

#endif // ISLUTILS_LOCUS_H
