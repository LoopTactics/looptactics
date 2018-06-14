namespace set_maker {
/* isl::aff operators ********************************************************/
inline isl::set operator==(isl::aff lhs, isl::aff rhs) {
  return lhs.eq_set(rhs);
}

inline isl::set operator!=(isl::aff lhs, isl::aff rhs) {
  return lhs.eq_set(rhs);
}

inline isl::set operator<=(isl::aff lhs, isl::aff rhs) {
  return lhs.le_set(rhs);
}

inline isl::set operator<(isl::aff lhs, isl::aff rhs) {
  return lhs.lt_set(rhs);
}

inline isl::set operator>=(isl::aff lhs, isl::aff rhs) {
  return lhs.ge_set(rhs);
}

inline isl::set operator>(isl::aff lhs, isl::aff rhs) {
  return lhs.gt_set(rhs);
}

/* isl::pw_aff operators *****************************************************/
inline isl::set operator==(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.eq_set(rhs);
}

inline isl::set operator!=(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.eq_set(rhs);
}

inline isl::set operator<=(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.le_set(rhs);
}

inline isl::set operator<(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.lt_set(rhs);
}

inline isl::set operator>=(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.ge_set(rhs);
}

inline isl::set operator>(isl::pw_aff lhs, isl::pw_aff rhs) {
  return lhs.gt_set(rhs);
}
} // namespace set_maker
