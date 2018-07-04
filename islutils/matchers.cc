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

  if (matcher.nodeCallback_ && !matcher.nodeCallback_(node)) {
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

static bool hasPreviousSiblingImpl(isl::schedule_node node,
                                   const ScheduleNodeMatcher &siblingMatcher) {
  while (isl_schedule_node_has_previous_sibling(node.get()) == isl_bool_true) {
    node = isl::manage(isl_schedule_node_previous_sibling(node.release()));
    if (ScheduleNodeMatcher::isMatching(siblingMatcher, node)) {
      return true;
    }
  }
  return false;
}

static bool hasNextSiblingImpl(isl::schedule_node node,
                               const ScheduleNodeMatcher &siblingMatcher) {
  while (isl_schedule_node_has_next_sibling(node.get()) == isl_bool_true) {
    node = isl::manage(isl_schedule_node_next_sibling(node.release()));
    if (ScheduleNodeMatcher::isMatching(siblingMatcher, node)) {
      return true;
    }
  }
  return false;
}

std::function<bool(isl::schedule_node)>
hasPreviousSibling(const ScheduleNodeMatcher &siblingMatcher) {
  return std::bind(hasPreviousSiblingImpl, std::placeholders::_1,
                   siblingMatcher);
}

std::function<bool(isl::schedule_node)>
hasNextSibling(const ScheduleNodeMatcher &siblingMatcher) {
  return std::bind(hasNextSiblingImpl, std::placeholders::_1, siblingMatcher);
}

std::function<bool(isl::schedule_node)>
hasSibling(const ScheduleNodeMatcher &siblingMatcher) {
  return [siblingMatcher](isl::schedule_node node) {
    return hasPreviousSiblingImpl(node, siblingMatcher) ||
           hasNextSiblingImpl(node, siblingMatcher);
  };
}

std::function<bool(isl::schedule_node)>
hasDescendant(const ScheduleNodeMatcher &descendantMatcher) {
  isl::schedule_node n;
  return [descendantMatcher](isl::schedule_node node) {
    // Cannot use capturing lambdas as C function pointers.
    struct Data {
      bool found;
      const ScheduleNodeMatcher &descendantMatcher;
    };
    Data data{false, descendantMatcher};

    auto r = isl_schedule_node_foreach_descendant_top_down(
        node.get(),
        [](__isl_keep isl_schedule_node *cn, void *user) -> isl_bool {
          auto data = static_cast<Data *>(user);
          if (data->found) {
            return isl_bool_false;
          }

          auto n = isl::manage_copy(cn);
          data->found =
              ScheduleNodeMatcher::isMatching(data->descendantMatcher, n);
          return data->found ? isl_bool_false : isl_bool_true;
        },
        &data);
    return r == isl_stat_ok && data.found;
  };
}

} // namespace matchers
