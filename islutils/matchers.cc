#include "islutils/matchers.h"

namespace matchers {

bool ScheduleNodeMatcher::isMatching(const ScheduleNodeMatcher &matcher,
                                     isl::schedule_node node) {
  if (!node.get()) {
    return false;
  }

  if (matcher.current_ != isl_schedule_node_get_type(node.get())) {
    return false;
  }

  if (matcher.children_.size() == 0) {
    return true;
  }

  size_t nChildren =
      static_cast<size_t>(isl_schedule_node_n_children(node.get()));
  if (matcher.children_.size() != nChildren) {
    return false;
  }

  for (size_t i = 0; i < nChildren; ++i) {
    if (!isMatching(matcher.children_.at(i), node.child(i))) {
      return false;
    }
  }
  return true;
}

} // namespace matchers
