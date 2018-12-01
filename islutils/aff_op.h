#ifndef ISLUTILS_AFF_OP_H
#define ISLUTILS_AFF_OP_H

#include <isl/cpp.h>

namespace aff_op {
// Operations between affs.
isl::aff operator+(isl::aff lhs, isl::aff rhs);
isl::aff operator-(isl::aff lhs, isl::aff rhs);
isl::aff operator*(isl::aff lhs, isl::aff rhs);
isl::aff operator/(isl::aff lhs, isl::aff rhs);
isl::aff operator-(isl::aff aff);

// Operations between aff and val.
isl::aff operator+(isl::aff lhs, isl::val rhs);
isl::aff operator+(isl::val lhs, isl::aff rhs);
isl::aff operator-(isl::aff lhs, isl::val rhs);
isl::aff operator-(isl::val lhs, isl::aff rhs);
isl::aff operator*(isl::aff lhs, isl::val rhs);
isl::aff operator*(isl::aff val, isl::aff rhs);
isl::aff operator/(isl::aff lhs, isl::val rhs);
isl::aff operator%(isl::aff lhs, isl::val rhs);

// Operations between aff and long.
// "long" is used because isl C API uses it, for other types, perform integer
// conversions or build an isl::val.
isl::aff operator+(isl::aff lhs, long rhs);
isl::aff operator+(long lhs, isl::aff rhs);
isl::aff operator-(isl::aff lhs, long rhs);
isl::aff operator-(long lhs, isl::aff rhs);
isl::aff operator*(isl::aff lhs, long rhs);
isl::aff operator*(long val, isl::aff rhs);
isl::aff operator/(isl::aff lhs, long rhs);
isl::aff operator%(isl::aff lhs, long rhs);

inline isl::aff operator+(isl::aff lhs, isl::aff rhs) { return lhs.add(rhs); }

inline isl::aff operator-(isl::aff lhs, isl::aff rhs) { return lhs.sub(rhs); }

inline isl::aff operator*(isl::aff lhs, isl::aff rhs) { return lhs.mul(rhs); }

inline isl::aff operator/(isl::aff lhs, isl::aff rhs) { return lhs.div(rhs); }

inline isl::aff operator-(isl::aff aff) { return aff.neg(); }

inline isl::aff operator+(isl::aff lhs, isl::val rhs) {
  return lhs.add_constant_val(rhs);
}

inline isl::aff operator+(isl::val lhs, isl::aff rhs) {
  // Addition is associative.
  return rhs + lhs;
}

inline isl::aff operator-(isl::aff lhs, isl::val rhs) {
  return lhs + rhs.neg();
}

inline isl::aff operator-(isl::val lhs, isl::aff rhs) {
  return lhs + rhs.neg();
}

inline isl::aff operator*(isl::aff lhs, isl::val rhs) { return lhs.scale(rhs); }

inline isl::aff operator*(isl::val lhs, isl::aff rhs) {
  // Multiplication is associative.
  return rhs * lhs;
}

inline isl::aff operator/(isl::aff lhs, isl::val rhs) {
  return lhs.scale_down(rhs);
}

inline isl::aff operator%(isl::aff lhs, isl::val rhs) { return lhs.mod(rhs); }

inline isl::aff operator+(isl::aff lhs, long rhs) {
  return lhs + isl::val(lhs.get_ctx(), rhs);
}

inline isl::aff operator+(long lhs, isl::aff rhs) {
  return isl::val(rhs.get_ctx(), lhs) + rhs;
}

inline isl::aff operator-(isl::aff lhs, long rhs) {
  return lhs - isl::val(lhs.get_ctx(), rhs);
}

inline isl::aff operator-(long lhs, isl::aff rhs) { return lhs + rhs.neg(); }

inline isl::aff operator*(isl::aff lhs, long rhs) {
  return lhs.scale(isl::val(lhs.get_ctx(), rhs));
}

inline isl::aff operator*(long lhs, isl::aff rhs) {
  // Multiplication is associative.
  return rhs * lhs;
}

inline isl::aff operator/(isl::aff lhs, long rhs) {
  return lhs.scale_down(isl::val(lhs.get_ctx(), rhs));
}

// Operations between pw_affs.
isl::pw_aff operator+(isl::pw_aff lhs, isl::pw_aff rhs);
isl::pw_aff operator-(isl::pw_aff lhs, isl::pw_aff rhs);
isl::pw_aff operator*(isl::pw_aff lhs, isl::pw_aff rhs);
isl::pw_aff operator/(isl::pw_aff lhs, isl::pw_aff rhs);
isl::pw_aff operator-(isl::pw_aff pw_aff);

// Operations between pw_aff and val.
isl::pw_aff operator+(isl::pw_aff lhs, isl::val rhs);
isl::pw_aff operator+(isl::val lhs, isl::pw_aff rhs);
isl::pw_aff operator-(isl::pw_aff lhs, isl::val rhs);
isl::pw_aff operator-(isl::val lhs, isl::pw_aff rhs);
isl::pw_aff operator*(isl::pw_aff lhs, isl::val rhs);
isl::pw_aff operator*(isl::pw_aff val, isl::pw_aff rhs);
isl::pw_aff operator/(isl::pw_aff lhs, isl::val rhs);
isl::pw_aff operator%(isl::pw_aff lhs, isl::val rhs);

// Operations between pw_aff and long.
// "long" is used because isl C API uses it, for other types, perform integer
// conversions or build an isl::val.
isl::pw_aff operator+(isl::pw_aff lhs, long rhs);
isl::pw_aff operator+(long lhs, isl::pw_aff rhs);
isl::pw_aff operator-(isl::pw_aff lhs, long rhs);
isl::pw_aff operator-(long lhs, isl::pw_aff rhs);
isl::pw_aff operator*(isl::pw_aff lhs, long rhs);
isl::pw_aff operator*(isl::pw_aff val, isl::pw_aff rhs);
isl::pw_aff operator/(isl::pw_aff lhs, long rhs);
isl::pw_aff operator%(isl::pw_aff lhs, long rhs);

inline isl::pw_aff operator+(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.add(rhs);
}

inline isl::pw_aff operator-(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.sub(rhs);
}

inline isl::pw_aff operator*(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.mul(rhs);
}

inline isl::pw_aff operator/(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.div(rhs);
}

inline isl::pw_aff operator-(isl::pw_aff pw_aff) { return pw_aff.neg(); }

inline isl::pw_aff operator+(isl::pw_aff lhs, isl::val rhs) {
  return lhs.add(isl::pw_aff(lhs.domain(), rhs));
}

inline isl::pw_aff operator+(isl::val lhs, isl::pw_aff rhs) {
  // Addition is associative.
  return rhs + lhs;
}

inline isl::pw_aff operator-(isl::pw_aff lhs, isl::val rhs) {
  return lhs + rhs.neg();
}

inline isl::pw_aff operator-(isl::val lhs, isl::pw_aff rhs) {
  return lhs + rhs.neg();
}

inline isl::pw_aff operator*(isl::pw_aff lhs, isl::val rhs) {
  return lhs.scale(rhs);
}

inline isl::pw_aff operator*(isl::val lhs, isl::pw_aff rhs) {
  // Multiplication is associative.
  return rhs * lhs;
}

inline isl::pw_aff operator/(isl::pw_aff lhs, isl::val rhs) {
  return lhs.scale_down(rhs);
}

inline isl::pw_aff operator%(isl::pw_aff lhs, isl::val rhs) {
  return lhs.mod(rhs);
}

inline isl::pw_aff operator+(isl::pw_aff lhs, long rhs) {
  return lhs + isl::val(lhs.get_ctx(), rhs);
}

inline isl::pw_aff operator+(long lhs, isl::pw_aff rhs) {
  return isl::val(rhs.get_ctx(), lhs) + rhs;
}

inline isl::pw_aff operator-(isl::pw_aff lhs, long rhs) {
  return lhs - isl::val(lhs.get_ctx(), rhs);
}

inline isl::pw_aff operator-(long lhs, isl::pw_aff rhs) {
  return lhs + rhs.neg();
}

inline isl::pw_aff operator*(isl::pw_aff lhs, long rhs) {
  return lhs.scale(isl::val(lhs.get_ctx(), rhs));
}

inline isl::pw_aff operator*(long lhs, isl::pw_aff rhs) {
  // Multiplication is associative.
  return rhs * lhs;
}

inline isl::pw_aff operator/(isl::pw_aff lhs, long rhs) {
  return lhs.scale_down(isl::val(lhs.get_ctx(), rhs));
}
} // namespace aff_op

#endif // ISLUTILS_AFF_OP_H
