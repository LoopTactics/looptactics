#ifndef SCOP_H
#define SCOP_H

#include <isl/isl-noexceptions.h>
#include <vector>

/// context holds constraints on the parameter that ensure that
/// this array has a valid (i.e., non-negative) size.
/// extent holds constraints on the indices
/// value_bounds holds constraints on the elements of the array
/// element_size is the size in bytes of each array element
/// element type is the type of the array elements.
/// element_is_record is set if this type is a record type
/// live_out is set if the arry appears in a live-out pragma
/// uniquely_defined is set if the array is known to be assigned 
/// only once before the read.
/// declared is set if the array was declared somewhere inside the scop.
/// exposed is set if the declared array is visible outside the scop.
/// outer is set if the type of the arry elements is a record and 
/// the fields of this record are represented by separate pet_array structures.

class ScopArray {
public:
  isl::set context;
  isl::set extent;
  //isl::set value_bounds;
  std::string element_type;
  int element_is_record;
  int element_size;
  int live_out;
  int uniquely_defined;
  int declared;
  int exposed;
  int outer;
};  

/// Minimalist container for a static control part (SCoP).
/// Contains domain, schedule and access information, where the domain is
/// encoded only as a part of schedule.
class Scop {
public:
  /// Convenience function to extract the iteration domain of a Scop from its
  /// schedule tree as a union_set.
  /// Returns the domain contained by the root node of the schedule tree, which
  /// is assumed to be a domain node (it is one for valid Scops).
  inline isl::union_set domain() const;

  /// Context of the Scop, i.e. the constraints on the parameters.
  isl::set context;
  /// Schedule of the Scop, defined over its domain.
  isl::schedule schedule;
  /// \{
  /// Access relations of different types.
  isl::union_map reads;
  isl::union_map mayWrites;
  isl::union_map mustWrites;
  /// \}
  /// Arrays description
  int n_array;
  std::vector<ScopArray> arrays;

  inline void dump() const;
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

void Scop::dump() const {
  isl_schedule_dump(schedule.get());
  isl_union_map_dump(reads.get());
  isl_union_map_dump(mayWrites.get());
  isl_union_map_dump(mustWrites.get());
}

#endif // SCOP_H
