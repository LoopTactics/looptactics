#include <isl/cpp.h>
#include <isl/schedule_node.h>

#include <vector>

/** \defgroup Matchers Matchers
 * \brief Structural matchers on schedule trees.
 *
 * A matcher is an object that captures the structure of schedule trees.
 * Conceptually, a matcher is a tree itself where every node is assigned a node
 * type.  The matcher class provides functionality to detect if a subtree in
 * the schedule tree has the same structure, that is the same types of nodes
 * and parent/child relationships.  Contrary to regular trees, matchers can be
 * constructed using nested call syntax omitting the details about the content
 * of nodes.  For example,
 *
 * ```
 * auto m = domain(
 *            context(
 *              sequence(
 *                filter(),
 *                filter())));
 * ```
 *
 * matches a subtree that starts at a domain node, having context as only
 * child, which in turn has a sequence as only child node, and the latter has
 * two filter children.  The structure is not anchored at any position in the
 * tree: the first node is not necessarily the tree root, and the innermost
 * node may have children of their own.
 */

/** \ingroup Matchers */
namespace matchers {

class ScheduleNodeMatcher;

/** \defgroup MatchersStructuralCstr Structural Matcher Constructors.
 * \ingroup Matchers
 * These functions construct a structural matcher on the schedule tree by
 * specifying the type of the node (indicated by the function name).  They take
 * other matchers as arguments to describe the children of the node.  Depending
 * on the node type, functions take a single child matcher or an arbitrary
 * number thereof.  Sequence and set matcher builders take multiple children as
 * these types of node are the only ones that can have more than one child.
 * Additionally, all constructors are overloaded with an extra leading argument
 * to store a callback function for finer-grain matching.  This function is
 * called on the node before attempting to match its children.  It is passed
 * the node itself and returns true if the matching may continue and false if
 * it should fail immediately without processing the children.  When no child
 * matchers are provided, the node is allowed to have zero or more children.
 */
/** \{ */
ScheduleNodeMatcher sequence();

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher sequence(Arg, Args... args);

template <typename... Args>
ScheduleNodeMatcher sequence(std::function<bool(isl::schedule_node)> callback,
                             Args... args);

ScheduleNodeMatcher set();

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher set(Arg, Args... args);

template <typename... Args>
ScheduleNodeMatcher set(std::function<bool(isl::schedule_node)> callback,
                        Args... args);

ScheduleNodeMatcher band();
ScheduleNodeMatcher band(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher band(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher band(std::function<bool(isl::schedule_node)> callback,
                         ScheduleNodeMatcher &&child);

ScheduleNodeMatcher context();
ScheduleNodeMatcher context(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher context(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher context(std::function<bool(isl::schedule_node)> callback,
                            ScheduleNodeMatcher &&child);

ScheduleNodeMatcher domain();
ScheduleNodeMatcher domain(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher domain(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher domain(std::function<bool(isl::schedule_node)> callback,
                           ScheduleNodeMatcher &&child);

ScheduleNodeMatcher extension();
ScheduleNodeMatcher extension(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher extension(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher extension(std::function<bool(isl::schedule_node)> callback,
                              ScheduleNodeMatcher &&child);

ScheduleNodeMatcher filter();
ScheduleNodeMatcher filter(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher filter(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher filter(std::function<bool(isl::schedule_node)> callback,
                           ScheduleNodeMatcher &&child);

ScheduleNodeMatcher guard();
ScheduleNodeMatcher guard(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher guard(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher guard(std::function<bool(isl::schedule_node)> callback,
                          ScheduleNodeMatcher &&child);

ScheduleNodeMatcher mark();
ScheduleNodeMatcher mark(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher mark(std::function<bool(isl::schedule_node)> callback);
ScheduleNodeMatcher mark(std::function<bool(isl::schedule_node)> callback,
                         ScheduleNodeMatcher &&child);
/** \} */

/** Node type matcher class for isl schedule trees.
 * \ingroup Matchers
 */
class ScheduleNodeMatcher {
#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  friend ScheduleNodeMatcher name();                                           \
  template <typename... Args>                                                  \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  Args...);                                    \
  template <typename Arg, typename... Args, typename>                          \
  friend ScheduleNodeMatcher name(Arg, Args...);
  DECL_FRIEND_TYPE_MATCH(sequence)
  DECL_FRIEND_TYPE_MATCH(set)

#undef DECL_FRIEND_TYPE_MATCH

#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  friend ScheduleNodeMatcher name();                                           \
  friend ScheduleNodeMatcher name(ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>);    \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  ScheduleNodeMatcher &&);

  DECL_FRIEND_TYPE_MATCH(band)
  DECL_FRIEND_TYPE_MATCH(context)
  DECL_FRIEND_TYPE_MATCH(domain)
  DECL_FRIEND_TYPE_MATCH(extension)
  DECL_FRIEND_TYPE_MATCH(filter)
  DECL_FRIEND_TYPE_MATCH(guard)
  DECL_FRIEND_TYPE_MATCH(mark)

#undef DECL_FRIEND_TYPE_MATCH

public:
  static bool isMatching(const ScheduleNodeMatcher &matcher,
                         isl::schedule_node node);

private:
  isl_schedule_node_type current_;
  std::vector<ScheduleNodeMatcher> children_;
  std::function<bool(isl::schedule_node)> nodeCallback_;
};

#include "matchers-inl.h"

} // namespace matchers
