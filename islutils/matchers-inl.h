#include <cassert>
#include <thread>
#include <type_traits>

namespace matchers {

namespace {
template <typename... Args>
std::vector<typename std::common_type<Args...>::type>
varargToVector(Args... args) {
  std::vector<typename std::common_type<Args...>::type> result;
  result.reserve(sizeof...(Args));
  for (const auto &a :
       {static_cast<typename std::common_type<Args...>::type>(args)...}) {
    result.emplace_back(a);
  }
  return result;
}
} // namespace

isl_schedule_node_type toIslType(ScheduleNodeType type) {
  switch (type) {
  case ScheduleNodeType::Band:
    return isl_schedule_node_band;
  case ScheduleNodeType::Context:
    return isl_schedule_node_context;
  case ScheduleNodeType::Domain:
    return isl_schedule_node_domain;
  case ScheduleNodeType::Extension:
    return isl_schedule_node_extension;
  case ScheduleNodeType::Filter:
    return isl_schedule_node_filter;
  case ScheduleNodeType::Guard:
    return isl_schedule_node_guard;
  case ScheduleNodeType::Mark:
    return isl_schedule_node_mark;
  case ScheduleNodeType::Leaf:
    return isl_schedule_node_leaf;
  case ScheduleNodeType::Sequence:
    return isl_schedule_node_sequence;
  case ScheduleNodeType::Set:
    return isl_schedule_node_set;
  default:
    assert(false && "cannot convert the given node type");
    return isl_schedule_node_leaf;
  }
}

ScheduleNodeType fromIslType(isl_schedule_node_type type) {
  switch (type) {
  case isl_schedule_node_band:
    return ScheduleNodeType::Band;
  case isl_schedule_node_context:
    return ScheduleNodeType::Context;
  case isl_schedule_node_domain:
    return ScheduleNodeType::Domain;
  case isl_schedule_node_extension:
    return ScheduleNodeType::Extension;
  case isl_schedule_node_filter:
    return ScheduleNodeType::Filter;
  case isl_schedule_node_guard:
    return ScheduleNodeType::Guard;
  case isl_schedule_node_mark:
    return ScheduleNodeType::Mark;
  case isl_schedule_node_leaf:
    return ScheduleNodeType::Leaf;
  case isl_schedule_node_sequence:
    return ScheduleNodeType::Sequence;
  case isl_schedule_node_set:
    return ScheduleNodeType::Set;
  default:
    assert(false && "cannot convert the given node type");
    return ScheduleNodeType::Leaf;
  }
}

std::string fromTypeToString(ScheduleNodeType type) {
  switch (type) {
  case ScheduleNodeType::Band:
    return "MATCHER_BAND";
  case ScheduleNodeType::Context:
    return "MATCHER_CONTEXT";
  case ScheduleNodeType::Domain:
    return "MATCHER_DOMAIN";
  case ScheduleNodeType::Extension:
    return "MATCHER_EXTENSION";
  case ScheduleNodeType::Filter:
    return "MATCHER_FILTER";
  case ScheduleNodeType::Guard:
    return "MATCHER_GUARD";
  case ScheduleNodeType::Mark:
    return "MATCHER_MARK";
  case ScheduleNodeType::Leaf:
    return "MATCHER_LEAF";
  case ScheduleNodeType::Sequence:
    return "MATCHER_SEQUENCE";
  case ScheduleNodeType::Set:
    return "MATCHER_SET";
  case ScheduleNodeType::AnyTree:
    return "MATCHER_ANYTREE";
  case ScheduleNodeType::AnyForest:
    return "MATCHER_ANYFOREST";
  case ScheduleNodeType::FilterForest:
    return "MACTHER_FILTERFOREST";
  default:
    return "NOT IMPLEMENTED";
  }
}

/* Definitions for schedule tree matcher factory functions ********************/
#define DEF_TYPE_MATCHER(name, type)                                           \
  template <typename Arg, typename... Args, typename>                          \
  inline ScheduleNodeMatcher name(isl::schedule_node &capture, Arg arg,        \
                                  Args... args) {                              \
    ScheduleNodeMatcher matcher(capture);                                      \
    matcher.current_ = type;                                                   \
    matcher.needToCapture_ = true;                                             \
    matcher.children_ = varargToVector<ScheduleNodeMatcher>(arg, args...);     \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  template <typename Arg, typename... Args, typename>                          \
  inline ScheduleNodeMatcher name(bool x, isl::schedule_node &capture,         \
                                  Arg arg, Args... args) {                     \
    ScheduleNodeMatcher matcher(capture);                                      \
    matcher.current_ = type;                                                   \
    matcher.needToCapture_ = x;                                                \
    matcher.children_ = varargToVector<ScheduleNodeMatcher>(arg, args...);     \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  template <typename Arg, typename... Args, typename>                          \
  inline ScheduleNodeMatcher name(Arg arg, Args... args) {                     \
    static isl::schedule_node dummyCapture;                                    \
    return name(false, dummyCapture, std::forward<Arg>(arg),                   \
                std::forward<Args>(args)...);                                  \
  }                                                                            \
                                                                               \
  template <typename Arg, typename... Args, typename>                          \
  inline ScheduleNodeMatcher name(                                             \
      std::function<bool(isl::schedule_node)> callback, Arg arg,               \
      Args... args) {                                                          \
    ScheduleNodeMatcher matcher =                                              \
        name(std::forward<Arg>(arg), std::forward<Args>(args)...);             \
    matcher.nodeCallback_ = callback;                                          \
    return matcher;                                                            \
  }

DEF_TYPE_MATCHER(sequence, ScheduleNodeType::Sequence)
DEF_TYPE_MATCHER(set, ScheduleNodeType::Set)

#undef DEF_TYPE_MATCHER

#define DEF_TYPE_MATCHER(name, type)                                           \
  inline ScheduleNodeMatcher name(isl::schedule_node &capture,                 \
                                  ScheduleNodeMatcher &&child) {               \
    ScheduleNodeMatcher matcher(capture);                                      \
    matcher.current_ = type;                                                   \
    matcher.needToCapture_ = true;                                             \
    matcher.children_.emplace_back(child);                                     \
    matcher.capture_ = capture;                                                \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  inline ScheduleNodeMatcher name(bool x, isl::schedule_node &capture,         \
                                  ScheduleNodeMatcher &&child) {               \
    ScheduleNodeMatcher matcher(capture);                                      \
    matcher.current_ = type;                                                   \
    matcher.needToCapture_ = x;                                                \
    matcher.children_.emplace_back(child);                                     \
    matcher.capture_ = capture;                                                \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  inline ScheduleNodeMatcher name(ScheduleNodeMatcher &&child) {               \
    static isl::schedule_node dummyCapture;                                    \
    return name(false, dummyCapture, std::move(child));                        \
  }                                                                            \
                                                                               \
  inline ScheduleNodeMatcher name(                                             \
      std::function<bool(isl::schedule_node)> callback,                        \
      ScheduleNodeMatcher &&child) {                                           \
    ScheduleNodeMatcher matcher = name(std::move(child));                      \
    matcher.nodeCallback_ = callback;                                          \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  inline ScheduleNodeMatcher name(                                             \
      std::function<bool(isl::schedule_node)> callback,                        \
      isl::schedule_node &capture, ScheduleNodeMatcher &&child) {              \
    ScheduleNodeMatcher matcher(capture);                                      \
    matcher.current_ = type;                                                   \
    matcher.needToCapture_ = true;                                             \
    matcher.children_.emplace_back(child);                                     \
    matcher.capture_ = capture;                                                \
    matcher.nodeCallback_ = callback;                                          \
    return matcher;                                                            \
  }

DEF_TYPE_MATCHER(band, ScheduleNodeType::Band)
DEF_TYPE_MATCHER(context, ScheduleNodeType::Context)
DEF_TYPE_MATCHER(domain, ScheduleNodeType::Domain)
DEF_TYPE_MATCHER(extension, ScheduleNodeType::Extension)
DEF_TYPE_MATCHER(filter, ScheduleNodeType::Filter)
DEF_TYPE_MATCHER(guard, ScheduleNodeType::Guard)
DEF_TYPE_MATCHER(mark, ScheduleNodeType::Mark)
DEF_TYPE_MATCHER(expansion, ScheduleNodeType::Expansion)

#undef DEF_TYPE_MATCHER

inline ScheduleNodeMatcher leaf() {
  static isl::schedule_node dummyCapture;
  ScheduleNodeMatcher matcher(dummyCapture);
  matcher.current_ = ScheduleNodeType::Leaf;
  return matcher;
}

inline ScheduleNodeMatcher leaf(isl::schedule_node &capture) {
  ScheduleNodeMatcher matcher(capture);
  matcher.needToCapture_ = true;
  matcher.current_ = ScheduleNodeType::Leaf;
  return matcher;
}

inline ScheduleNodeMatcher anyTree(isl::schedule_node &capture) {
  ScheduleNodeMatcher matcher(capture);
  matcher.current_ = ScheduleNodeType::AnyTree;
  matcher.needToCapture_ = true;
  return matcher;
}

inline ScheduleNodeMatcher anyTree() {
  static isl::schedule_node dummyCapture;
  ScheduleNodeMatcher matcher(dummyCapture);
  matcher.current_ = ScheduleNodeType::AnyTree;
  return matcher;
}

// inline ScheduleNodeMatcher
// anyTree(std::function<bool(isl::schedule_node)> callback,
//        isl::schedule_node &capture) {
//  ScheduleNodeMatcher matcher = anyTree(capture);
//  matcher.nodeCallback_ = callback;
//  return matcher;
//}

inline ScheduleNodeMatcher anyForest() {
  static isl::schedule_node dummyCapture;
  ScheduleNodeMatcher matcher(dummyCapture);
  matcher.current_ = ScheduleNodeType::AnyForest;
  return matcher;
}

inline ScheduleNodeMatcher
anyForest(std::vector<isl::schedule_node> &multiCapture) {
  static isl::schedule_node dummyCapture;
  ScheduleNodeMatcher matcher(dummyCapture, multiCapture);
  matcher.current_ = ScheduleNodeType::AnyForest;
  matcher.needToCapture_ = true;
  return matcher;
}

inline ScheduleNodeMatcher
filterForest(std::vector<isl::schedule_node> &multiCapture) {
  static isl::schedule_node dummyCapture;
  ScheduleNodeMatcher matcher(dummyCapture, multiCapture);
  matcher.current_ = ScheduleNodeType::FilterForest;
  matcher.needToCapture_ = true;
  return matcher;
}

} // namespace matchers
