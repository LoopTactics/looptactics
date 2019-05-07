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
 *                filter(
 *                  anyTree()),
 *                filter(
 *                  anyTree()))));
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
 * it should fail immediately without processing the children.
 *
 * Type-based matchers must always have child matchers.  These are either
 * (lists of) type-based matchers or special matchers leaf(), anyTree() or
 * anyForest().
 *
 * The special matcher leaf() indicates that the tree node containing it should
 * be the leaf in the schedule tree.  The matcher anyTree() indicates that the
 * tree node may contain exactly one child of the given type.  The matcher
 * anyForest() indicates that the node may contain an arbitrary number of
 * children (useful for sequence or set nodes).
 *
 */
/** \{ */
template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher sequence(Arg, Args... args);

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher sequence(isl::schedule_node &node, Arg, Args... args);

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher sequence(std::function<bool(isl::schedule_node)> callback,
                             Arg arg, Args... args);

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher set(Arg, Args... args);

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher set(isl::schedule_node &node, Arg, Args... args);

template <typename Arg, typename... Args,
          typename = typename std::enable_if<
              std::is_same<typename std::remove_reference<Arg>::type,
                           ScheduleNodeMatcher>::value>::type>
ScheduleNodeMatcher set(std::function<bool(isl::schedule_node)> callback,
                        Arg arg, Args... args);

ScheduleNodeMatcher band(isl::schedule_node &capture,
                         ScheduleNodeMatcher &&child);
ScheduleNodeMatcher band(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher band(std::function<bool(isl::schedule_node)> callback,
                         ScheduleNodeMatcher &&child);

ScheduleNodeMatcher context(isl::schedule_node &capture,
                            ScheduleNodeMatcher &&child);
ScheduleNodeMatcher context(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher context(std::function<bool(isl::schedule_node)> callback,
                            ScheduleNodeMatcher &&child);

ScheduleNodeMatcher domain(isl::schedule_node &capture,
                           ScheduleNodeMatcher &&child);
ScheduleNodeMatcher domain(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher domain(std::function<bool(isl::schedule_node)> callback,
                           ScheduleNodeMatcher &&child);

ScheduleNodeMatcher extension(isl::schedule_node &capture,
                              ScheduleNodeMatcher &&child);
ScheduleNodeMatcher extension(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher extension(std::function<bool(isl::schedule_node)> callback,
                              ScheduleNodeMatcher &&child);

ScheduleNodeMatcher filter(isl::schedule_node &capture,
                           ScheduleNodeMatcher &&child);
ScheduleNodeMatcher filter(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher filter(std::function<bool(isl::schedule_node)> callback,
                           ScheduleNodeMatcher &&child);

ScheduleNodeMatcher guard(isl::schedule_node &capture,
                          ScheduleNodeMatcher &&child);
ScheduleNodeMatcher guard(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher guard(std::function<bool(isl::schedule_node)> callback,
                          ScheduleNodeMatcher &&child);

ScheduleNodeMatcher mark(isl::schedule_node &capture,
                         ScheduleNodeMatcher &&child);
ScheduleNodeMatcher mark(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher mark(std::function<bool(isl::schedule_node)> callback,
                         ScheduleNodeMatcher &&child);

ScheduleNodeMatcher expansion(isl::schedule_node &capture,
                              ScheduleNodeMatcher &&child);
ScheduleNodeMatcher expansion(ScheduleNodeMatcher &&child);
ScheduleNodeMatcher expansion(std::function<bool(isl::schedule_node)> callback,
                              ScheduleNodeMatcher &&child);

ScheduleNodeMatcher leaf();
ScheduleNodeMatcher leaf(isl::schedule_node &capture);

ScheduleNodeMatcher anyTree(isl::schedule_node &capture);
ScheduleNodeMatcher anyTree();

ScheduleNodeMatcher anyForest(std::vector<isl::schedule_node> &captures);
/** \} */

enum class ScheduleNodeType {
  Band,
  Context,
  Domain,
  Extension,
  Filter,
  Guard,
  Mark,
  Leaf,
  Sequence,
  Set,
  Expansion,

  AnyTree,
  AnyForest
};

inline isl_schedule_node_type toIslType(ScheduleNodeType type);
inline ScheduleNodeType fromIslType(isl_schedule_node_type type);

/** Node type matcher class for isl schedule trees.
 * \ingroup Matchers
 */
class ScheduleNodeMatcher {
#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  template <typename Arg, typename... Args, typename>                          \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  Arg, Args...);                               \
  template <typename Arg, typename... Args, typename>                          \
  friend ScheduleNodeMatcher name(isl::schedule_node &, Arg, Args...);         \
  template <typename Arg, typename... Args, typename>                          \
  friend ScheduleNodeMatcher name(Arg, Args...);
  DECL_FRIEND_TYPE_MATCH(sequence)
  DECL_FRIEND_TYPE_MATCH(set)

#undef DECL_FRIEND_TYPE_MATCH

#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  friend ScheduleNodeMatcher name(ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(isl::schedule_node &,                        \
                                  ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  ScheduleNodeMatcher &&);

  DECL_FRIEND_TYPE_MATCH(band)
  DECL_FRIEND_TYPE_MATCH(context)
  DECL_FRIEND_TYPE_MATCH(domain)
  DECL_FRIEND_TYPE_MATCH(extension)
  DECL_FRIEND_TYPE_MATCH(filter)
  DECL_FRIEND_TYPE_MATCH(guard)
  DECL_FRIEND_TYPE_MATCH(mark)
  DECL_FRIEND_TYPE_MATCH(expansion)

#undef DECL_FRIEND_TYPE_MATCH

  friend ScheduleNodeMatcher leaf();
  friend ScheduleNodeMatcher leaf(isl::schedule_node &);
  friend ScheduleNodeMatcher anyTree();
  friend ScheduleNodeMatcher anyTree(isl::schedule_node &);
  friend ScheduleNodeMatcher anyForest();
  friend ScheduleNodeMatcher anyForest(std::vector<isl::schedule_node> &);

private:
  explicit ScheduleNodeMatcher(isl::schedule_node &capture)
      : capture_(capture), multiCapture_(dummyMultiCaptureData_) {}
  explicit ScheduleNodeMatcher(isl::schedule_node &capture,
                               std::vector<isl::schedule_node> &multiCapture)
      : capture_(capture), multiCapture_(multiCapture) {}

public:
  static bool isMatching(const ScheduleNodeMatcher &matcher,
                         isl::schedule_node node);

private:
  ScheduleNodeType current_;
  std::vector<ScheduleNodeMatcher> children_;
  std::function<bool(isl::schedule_node)> nodeCallback_;
  isl::schedule_node &capture_;
  std::vector<isl::schedule_node> &multiCapture_;

  static thread_local std::vector<isl::schedule_node> dummyMultiCaptureData_;
};

std::function<bool(isl::schedule_node)>
hasPreviousSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasNextSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasDescendant(const ScheduleNodeMatcher &descendantMatcher);

/// And between callbacks function.
/// You can use it as below:
/// ~~~~~
/// band(_and([](isl::schedule_node n) { 
///               / * callback 1 */      
///               return true; 
///           },
///           [](isl::schedule_node n) {
///               /* callback 2 */
///               return true;
///           }),leaf());
/// ~~~~~~
template <typename... Args>
std::function<bool(isl::schedule_node)>
_and(Args... args) {

  std::vector<std::function<bool(isl::schedule_node)>> vec = {args...};

  return [vec](isl::schedule_node node) {
    std::function<bool(isl::schedule_node)> tmp = vec[0];
    bool result = tmp(node);
    for (size_t i = 1; i < vec.size(); i++) {
      tmp = vec[i];
      result = result and tmp(node);
    }
    return result;
  };
}

/// Or between callbacks functions.
template <typename... Args>
std::function<bool(isl::schedule_node)>
_or(Args... args) {

  std::vector<std::function<bool(isl::schedule_node)>> vec = {args...};

  return [vec](isl::schedule_node node) {
    std::function<bool(isl::schedule_node)> tmp = vec[0];
    bool result = tmp(node);
    for (size_t i = 1; i < vec.size(); i++) {
      tmp = vec[i];
      result = result or tmp(node);
    }
    return result;
  };
}
  
} // namespace matchers

#include "matchers-inl.h"
