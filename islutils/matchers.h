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

/** Node type matcher class for isl schedule trees.
 * \ingroup Matchers
 */
class ScheduleNodeMatcher {
#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  template <typename... Args> friend ScheduleNodeMatcher name(Args...);
  DECL_FRIEND_TYPE_MATCH(sequence)
  DECL_FRIEND_TYPE_MATCH(set)

#undef DECL_FRIEND_TYPE_MATCH

#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  friend ScheduleNodeMatcher name();                                           \
  friend ScheduleNodeMatcher name(ScheduleNodeMatcher &&);

  DECL_FRIEND_TYPE_MATCH(band)
  DECL_FRIEND_TYPE_MATCH(context)
  DECL_FRIEND_TYPE_MATCH(domain)
  DECL_FRIEND_TYPE_MATCH(extension)
  DECL_FRIEND_TYPE_MATCH(filter)
  DECL_FRIEND_TYPE_MATCH(guard)
  DECL_FRIEND_TYPE_MATCH(mark)

#undef DECL_FRIEND_TYPE_MATCH

public:
  bool isMatching(const ScheduleNodeMatcher &matcher, isl::schedule_node node);

private:
  isl_schedule_node_type current_;
  std::vector<ScheduleNodeMatcher> children_;
};

/** \defgroup MatchersStructuralCstr Structural Matcher Constructors
 * \ingroup Matchers
 * These functions construct a structural matcher on the schedule tree by
 * specifying the type of the node (indicated by the function name).  They take
 * an arbitary number of other matchers as arguments.  These arguments indicate
 * the types of the children and may also take arguments recursively.  If no
 * argument are provided, the node is allowed to have any number of children,
 * including none.
 * \{
 */
ScheduleNodeMatcher band();
ScheduleNodeMatcher band(ScheduleNodeMatcher &&child);

ScheduleNodeMatcher context();
ScheduleNodeMatcher context(ScheduleNodeMatcher &&child);

ScheduleNodeMatcher domain();
ScheduleNodeMatcher domain(ScheduleNodeMatcher &&child);

ScheduleNodeMatcher extension();
ScheduleNodeMatcher extension(ScheduleNodeMatcher &&child);

ScheduleNodeMatcher filter();
ScheduleNodeMatcher filter(ScheduleNodeMatcher &&child);

ScheduleNodeMatcher guard();
ScheduleNodeMatcher guard(ScheduleNodeMatcher &&child);

ScheduleNodeMatcher mark();
ScheduleNodeMatcher mark(ScheduleNodeMatcher &&child);

template <typename... Args> ScheduleNodeMatcher sequence(Args... args);
template <typename... Args> ScheduleNodeMatcher set(Args... args);
/** \} */

#include "matchers-inl.h"

} // namespace matchers
