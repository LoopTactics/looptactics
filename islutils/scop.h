#ifndef SCOP_H
#define SCOP_H

#include <isl/cpp.h>

/// Minimalist container for a static control part (SCoP).
/// Contains domain, schedule and access information, where the domain is
//encoded only as a part of schedule.
class Scop {
 public:
  /// Convenience function to extract the iteration domain of a Scop from its
  /// schedule tree as a union_set.
  /// Returns the domain contained by the root node of the schedule tree, which
  //is assumed to be a domain node (it is one for valid Scops).
  inline isl::union_set domain() const;

  /// Schedule of the Scop, defined over its domain.
  isl::schedule schedule;  
  /// \{
  /// Access relations of different types.
  isl::union_map reads;
  isl::union_map mayWrites;
  isl::union_map mustWrites;
  /// \}
};

isl::union_set Scop::domain() const {
  if (!schedule.get()) {
    return isl::union_set();
  }

  isl::schedule_node root = schedule.get_root();
  if (isl_schedule_node_get_type(root.get()) != isl_schedule_node_domain) {
    return isl::union_set();
  }
  return isl::manage(isl_schedule_node_domain_get_domain(root.get()));
}

#endif //SCOP_H
