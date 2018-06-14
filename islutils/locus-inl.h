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

namespace map_maker {

/* isl::aff operators ********************************************************/
// map-producing functions are not available for affs, raise to pw_aff first
inline isl::map operator==(isl::aff lhs, isl::aff rhs) {
  return isl::pw_aff(lhs) == isl::pw_aff(rhs);
}

inline isl::map operator!=(isl::aff lhs, isl::aff rhs) {
  return isl::pw_aff(lhs) != isl::pw_aff(rhs);
}

inline isl::map operator>=(isl::aff lhs, isl::aff rhs) {
  return isl::pw_aff(lhs) >= isl::pw_aff(rhs);
}

inline isl::map operator>(isl::aff lhs, isl::aff rhs) {
  return isl::pw_aff(lhs) > isl::pw_aff(rhs);
}

inline isl::map operator<=(isl::aff lhs, isl::aff rhs) {
  return isl::pw_aff(lhs) <= isl::pw_aff(rhs);
}

inline isl::map operator<(isl::aff lhs, isl::aff rhs) {
  return isl::pw_aff(lhs) < isl::pw_aff(rhs);
}

/* isl::pw_aff operators *****************************************************/
inline isl::map operator==(isl::pw_aff lhs, isl::pw_aff rhs) {
  // not exported
  return isl::manage(isl_pw_aff_eq_map(lhs.release(), rhs.release()));
}

inline isl::map operator!=(isl::pw_aff lhs, isl::pw_aff rhs) {
  // not available directly
  return (lhs > rhs).unite(lhs < rhs);
}

inline isl::map operator<=(isl::pw_aff lhs, isl::pw_aff rhs) {
  // not available directly
  return (lhs == rhs).unite(lhs < rhs);
}

inline isl::map operator<(isl::pw_aff lhs, isl::pw_aff rhs) {
  // not exported
  return isl::manage(isl_pw_aff_lt_map(lhs.release(), rhs.release()));
}

inline isl::map operator>=(isl::pw_aff lhs, isl::pw_aff rhs) {
  // not available directly
  return (lhs == rhs).unite(lhs > rhs);
}

inline isl::map operator>(isl::pw_aff lhs, isl::pw_aff rhs) {
  // not exported
  return isl::manage(isl_pw_aff_gt_map(lhs.release(), rhs.release()));
}

} // namespace map_maker
