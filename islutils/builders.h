#ifndef BUILDERS_H
#define BUILDERS_H

#include <isl/cpp.h>
#include <isl/id.h>

#include <vector>

namespace builders {

class ScheduleNodeBuilder {
private:
  isl_union_set_list *collectChildFilters(isl::ctx) const;
  isl::schedule_node insertSequenceOrSetAt(isl::schedule_node,
                                           isl_schedule_node_type type) const;
  isl::schedule_node
  insertSingleChildTypeNodeAt(isl::schedule_node,
                              isl_schedule_node_type type) const;

public:
  isl::schedule_node insertAt(isl::schedule_node node) const;
  isl::schedule_node build() const;

public:
  isl_schedule_node_type current_;
  std::vector<ScheduleNodeBuilder> children_;

  // XXX: Cannot use a union because C++ isl types have non-trivial
  // constructors.
  isl::set set_;
  isl::union_set uset_;
  isl::multi_union_pw_aff mupa_;
  isl::union_map umap_;
  isl_id *id_;
};

ScheduleNodeBuilder domain(isl::union_set uset,
                           std::vector<ScheduleNodeBuilder> &&children =
                               std::vector<ScheduleNodeBuilder>());

ScheduleNodeBuilder band(isl::multi_union_pw_aff mupa,
                         std::vector<ScheduleNodeBuilder> &&children =
                             std::vector<ScheduleNodeBuilder>());

ScheduleNodeBuilder filter(isl::union_set uset,
                           std::vector<ScheduleNodeBuilder> &&children =
                               std::vector<ScheduleNodeBuilder>());

ScheduleNodeBuilder sequence(std::vector<ScheduleNodeBuilder> &&children =
                                 std::vector<ScheduleNodeBuilder>());

ScheduleNodeBuilder set(std::vector<ScheduleNodeBuilder> &&children =
                            std::vector<ScheduleNodeBuilder>());

} // namespace builders

#endif // BUILDERS_H
