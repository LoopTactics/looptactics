#include <type_traits>

namespace {
template <typename Arg, typename... Args>
inline typename std::enable_if<sizeof...(Args) != 0, void>::type
appendVarargToVector(std::vector<Arg> &vec, Args... args) {
  vec.push_back(std::get<0>(std::tuple<Args...>(args...)));
  appendVarargToVector<Arg>(vec, args...);
}

template <typename Arg, typename... Args>
inline typename std::enable_if<sizeof...(Args) == 0, void>::type
appendVarargToVector(std::vector<Arg> &vec, Args...) {
  (void)vec;
}

template <typename R, typename... Args>
std::vector<R> varargToVector(Args... args) {
  std::vector<R> result;
  appendVarargToVector<R, Args...>(result, args...);
  return result;
}
} // namespace

/* Definitions for schedule tree matcher factory functions ********************/
#define DEF_TYPE_MATCHER(name, type)                                           \
  template <typename... Args> ScheduleNodeMatcher name(Args... args) {         \
    ScheduleNodeMatcher matcher;                                               \
    matcher.current_ = type;                                                   \
    matcher.children_ = varargToVector<ScheduleNodeMatcher>(args...);          \
    return matcher;                                                            \
  }

DEF_TYPE_MATCHER(band, isl_schedule_node_band)
DEF_TYPE_MATCHER(context, isl_schedule_node_context)
DEF_TYPE_MATCHER(domain, isl_schedule_node_domain)
DEF_TYPE_MATCHER(extension, isl_schedule_node_extension)
DEF_TYPE_MATCHER(filter, isl_schedule_node_filter)
DEF_TYPE_MATCHER(guard, isl_schedule_node_guard)
DEF_TYPE_MATCHER(mark, isl_schedule_node_mark)
DEF_TYPE_MATCHER(sequence, isl_schedule_node_sequence)
DEF_TYPE_MATCHER(set, isl_schedule_node_set)

#undef DEF_TYPE_MATCHER
