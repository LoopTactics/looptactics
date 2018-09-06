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

  isl::schedule_node expandTree(isl::schedule_node) const;

public:
  ScheduleNodeBuilder() : current_(isl_schedule_node_leaf) {}

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

  isl::union_pw_multi_aff upma_;
  isl::id id_;
};

ScheduleNodeBuilder domain(isl::union_set uset,
                           ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder band(isl::multi_union_pw_aff mupa,
                         ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder filter(isl::union_set uset,
                           ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder
extension(isl::union_map umap,
          ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder
expansion(isl::union_map expansion,
          ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder mark(isl::id id,
                         ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder guard(isl::set set,
                          ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

ScheduleNodeBuilder
context(isl::set set, ScheduleNodeBuilder &&child = ScheduleNodeBuilder());

template <typename... Args, typename = typename std::enable_if<std::is_same<
                                typename std::common_type<Args...>::type,
                                ScheduleNodeBuilder>::value>::type>
ScheduleNodeBuilder set(Args... children) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_set;
  builder.children_ = {children...};
  return builder;
}

ScheduleNodeBuilder set(std::vector<ScheduleNodeBuilder> &&children);

ScheduleNodeBuilder subtree(isl::schedule_node node);

template <typename T,
          typename = typename std::enable_if<
              std::is_same<T, ScheduleNodeBuilder>::value ||
              std::is_same<T, std::vector<ScheduleNodeBuilder>>::value>::type>
std::vector<ScheduleNodeBuilder> varargToVector(T t) {
  return {t};
}

template <typename T, class... Args>
std::vector<ScheduleNodeBuilder> varargToVector(T t, Args... args) {
  std::vector<ScheduleNodeBuilder> rest = varargToVector(args...);
  std::vector<ScheduleNodeBuilder> result = {t};
  result.insert(std::end(result), std::make_move_iterator(rest.begin()),
                std::make_move_iterator(rest.end()));
  return result;
}

template <class... Args> ScheduleNodeBuilder sequence(Args... args) {
  std::vector<ScheduleNodeBuilder> children = varargToVector(args...);
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_sequence;
  builder.children_ = children;
  return builder;
}

} // namespace builders

#endif // BUILDERS_H
