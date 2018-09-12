#ifndef ISLUTILS_OPERATORS
#define ISLUTILS_OPERATORS

#include "islutils/type_traits.h"

#include <isl/cpp.h>

namespace isl {
/// Equality comparison operator for isl objects.  Two null objects compare
/// equal, null objects don't compare equal to non-null objects but the
/// comparison does not throw.  If both objects are non-null, they are compared
/// using the corresponding ::is_equal method.
/// This must live in the isl namespace to enable ADL.
template <typename T,
          typename = typename std::enable_if<is_isl_type<T>::value>::type>
inline bool operator==(const T &left, const T &right) {
  if (left.is_null() && right.is_null()) {
    return true;
  }
  if (left.is_null() || right.is_null()) {
    return false;
  }
  return left.is_equal(right);
}

// Specialization for isl ids, which are pointer-comparable.
template <>
inline bool operator==<isl::id>(const isl::id &left, const isl::id &right) {
  return left.get() == right.get();
}

/// Inequality comparison operator for isl objects.
/// This must live in the isl namespace to enable ADL.
template <typename T,
          typename = typename std::enable_if<is_isl_type<T>::value>::type>
inline bool operator!=(const T &left, const T &right) {
  return !(left == right);
}
} // namespace isl

#endif
