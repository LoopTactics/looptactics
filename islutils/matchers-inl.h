#include <type_traits>

#include <cassert>

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

// TODO: use variadic template.
/* Definitions for relation matcher factory functions *************************/
#define DEF_TYPE_MATCHER_RELATION(name, type)                                  \
  inline RelationMatcher name(char a, char b) {                                \
    RelationMatcher matcher;                                                   \
    matcher.type_ = type;                                                      \
    matcher.indexes_.push_back(a);                                             \
    matcher.indexes_.push_back(b);                                             \
    matcher.setDim_.reserve(2);                                                \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  inline RelationMatcher name(char a) {                                        \
    RelationMatcher matcher;                                                   \
    matcher.type_ = type;                                                      \
    matcher.indexes_.push_back(a);                                             \
    matcher.setDim_.reserve(1);                                                \
    return matcher;                                                            \
  }

DEF_TYPE_MATCHER_RELATION(read, RelationKind::read)
DEF_TYPE_MATCHER_RELATION(write, RelationKind::write)

/* Definitions for schedule tree matcher factory functions ********************/
#define DEF_TYPE_MATCHER(name, type)                                           \
  template <typename Arg, typename... Args, typename>                          \
  inline ScheduleNodeMatcher name(isl::schedule_node &capture, Arg arg,        \
                                  Args... args) {                              \
    ScheduleNodeMatcher matcher(capture);                                      \
    matcher.current_ = type;                                                   \
    matcher.children_ = varargToVector<ScheduleNodeMatcher>(arg, args...);     \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  template <typename Arg, typename... Args, typename>                          \
  inline ScheduleNodeMatcher name(Arg arg, Args... args) {                     \
    static isl::schedule_node dummyCapture;                                    \
    return name(dummyCapture, std::forward<Arg>(arg),                          \
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
    matcher.children_.emplace_back(child);                                     \
    matcher.capture_ = capture;                                                \
    return matcher;                                                            \
  }                                                                            \
                                                                               \
  inline ScheduleNodeMatcher name(ScheduleNodeMatcher &&child) {               \
    static isl::schedule_node dummyCapture;                                    \
    return name(dummyCapture, std::move(child));                               \
  }                                                                            \
                                                                               \
  inline ScheduleNodeMatcher name(                                             \
      std::function<bool(isl::schedule_node)> callback,                        \
      ScheduleNodeMatcher &&child) {                                           \
    ScheduleNodeMatcher matcher = name(std::move(child));                      \
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

inline ScheduleNodeMatcher any(isl::schedule_node &capture) {
  ScheduleNodeMatcher matcher(capture);
  matcher.current_ = ScheduleNodeType::Any;
  return matcher;
}

inline ScheduleNodeMatcher any() {
  static isl::schedule_node dummyCapture;
  return any(dummyCapture);
}

} // namespace matchers
