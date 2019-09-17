#include <isl/isl-noexceptions.h>
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
ScheduleNodeMatcher sequence(bool, isl::schedule_node &node, Arg, Args... args);

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

/// Create a matcher of type band.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher band(isl::schedule_node &capture,
                         ScheduleNodeMatcher &&child);
/// Create a matcher of type band.
///
/// @param child: Child matcher.
ScheduleNodeMatcher band(ScheduleNodeMatcher &&child);
/// Create a matcher of type band.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher band(std::function<bool(isl::schedule_node)> callback,
                         ScheduleNodeMatcher &&child);
/// Create a matcher of type band.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher band(std::function<bool(isl::schedule_node)> callback,
                         isl::schedule_node &capture,
                         ScheduleNodeMatcher &&child);
/// Create a context matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher context(isl::schedule_node &capture,
                            ScheduleNodeMatcher &&child);
/// Create a context matcher.
///
/// @param child: Child matcher.
ScheduleNodeMatcher context(ScheduleNodeMatcher &&child);
/// Create a context matcher.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param child: Child matcher
ScheduleNodeMatcher context(std::function<bool(isl::schedule_node)> callback,
                            ScheduleNodeMatcher &&child);
/// Create a domain matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher domain(isl::schedule_node &capture,
                           ScheduleNodeMatcher &&child);
/// Create a domain matcher.
///
/// @param capture: It will contain the captured node.
ScheduleNodeMatcher domain(ScheduleNodeMatcher &&child);
/// Create a domain matcher.
///
/// @param callback: Boolean callback funtion for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher domain(std::function<bool(isl::schedule_node)> callback,
                           ScheduleNodeMatcher &&child);
/// Create a domain mathcer.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher domain(std::function<bool(isl::schedule_node)> callback,
                           isl::schedule_node &capture,
                           ScheduleNodeMatcher &&child);
/// Create an extension matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher extension(isl::schedule_node &capture,
                              ScheduleNodeMatcher &&child);
/// Create an extension matcher.
///
/// @param child: Child matcher.
ScheduleNodeMatcher extension(ScheduleNodeMatcher &&child);
/// Create an extension matcher.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher extension(std::function<bool(isl::schedule_node)> callback,
                              ScheduleNodeMatcher &&child);
/// Create an extension matcher.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher extension(std::function<bool(isl::schedule_node)> callback,
                              isl::schedule_node &capture,
                              ScheduleNodeMatcher &&child);
/// Create a filter matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher filter(isl::schedule_node &capture,
                           ScheduleNodeMatcher &&child);
/// Create a filter matcher.
///
/// @param child: Child matcher.
ScheduleNodeMatcher filter(ScheduleNodeMatcher &&child);
/// Create a filter matcher.
///
/// @param callback: Boolean callback funtion for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher filter(std::function<bool(isl::schedule_node)> callback,
                           ScheduleNodeMatcher &&child);
/// Create a filter matcher.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher
ScheduleNodeMatcher filter(std::function<bool(isl::schedule_node)> callback,
                           isl::schedule_node &capture,
                           ScheduleNodeMatcher &&child);
/// Create a guard matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher guard(isl::schedule_node &capture,
                          ScheduleNodeMatcher &&child);
/// Create a guard matcher.
///
/// @param child: Child matcher.
ScheduleNodeMatcher guard(ScheduleNodeMatcher &&child);
/// Create a guard matcher.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher guard(std::function<bool(isl::schedule_node)> callback,
                          ScheduleNodeMatcher &&child);
/// Create a guard matcher.
///
/// @param callback: Boolean callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher guard(std::function<bool(isl::schedule_node)> callback,
                          isl::schedule_node &capture,
                          ScheduleNodeMatcher &&child);
/// Create a mark matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher mark(isl::schedule_node &capture,
                         ScheduleNodeMatcher &&child);
/// Create a mark matcher.
///
/// @param child: Child matcher.
ScheduleNodeMatcher mark(ScheduleNodeMatcher &&child);
/// Create a mark matcher.
///
/// @param callback: Callback function for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher mark(std::function<bool(isl::schedule_node)> callback,
                         ScheduleNodeMatcher &&child);
/// Create a mark matcher.
///
/// @param callback: Callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher mark(std::function<bool(isl::schedule_node)> callback,
                         isl::schedule_node &capture,
                         ScheduleNodeMatcher &&child);
/// Create an expansion matcher.
///
/// @param capture: It will contain the captured node.
/// @param child: Child matcher.
ScheduleNodeMatcher expansion(isl::schedule_node &capture,
                              ScheduleNodeMatcher &&child);
/// Create an expansion matcher.
///
/// @param child: Child matcher.
ScheduleNodeMatcher expansion(ScheduleNodeMatcher &&child);
/// Create an expansion matcher.
///
/// @param callback: Callback function for finer-grain matching.
/// @param child: Child matcher.
ScheduleNodeMatcher expansion(std::function<bool(isl::schedule_node)> callback,
                              ScheduleNodeMatcher &&child);
/// Create an expansion matcher.
///
/// @param callback: Callback function for finer-grain matching.
/// @param capture: It will contain the captured node.
/// @param child: Child matcher
ScheduleNodeMatcher expansion(std::function<bool(isl::schedule_node)> callback,
                              isl::schedule_node &capture,
                              ScheduleNodeMatcher &&child);
/// Create a matcher of type leaf.
ScheduleNodeMatcher leaf();
/// Create a matcher of type leaf. The matcher will
/// capture the target node
///
/// @param capture Capture will contain the captured node.
ScheduleNodeMatcher leaf(isl::schedule_node &capture);
/// Create a matcher that will match any node type.
///
/// @param capture Capture will contain the captured node.
ScheduleNodeMatcher anyTree(isl::schedule_node &capture);
/// Create a matcher that will match any node type.
ScheduleNodeMatcher anyTree();
/// Create a matcher that will match any node type based on some conditions.
// ScheduleNodeMatcher anyTree(std::function<bool(isl::schedule_node)> callback,
//                            isl::schedule_node &capture);
/// Create a matcher that will capture any sequence of node type.
///
/// @param captures Captures will contain the sequence of captured nodes.
ScheduleNodeMatcher anyForest(std::vector<isl::schedule_node> &captures);
///
/// Create a matcher that will capture a sequence of filter types.
/// For example:
/// ~~~~
/// filter1
///   leaf
/// filter2
///   leaf
/// ~~~~
/// @param captures Captures will contain the sequence of captured filters.
ScheduleNodeMatcher filterForest(std::vector<isl::schedule_node> &captures);

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
  AnyForest,
  FilterForest,
};

inline isl_schedule_node_type toIslType(ScheduleNodeType type);
inline ScheduleNodeType fromIslType(isl_schedule_node_type type);
inline std::string fromTypeToString(ScheduleNodeType type);

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
  friend ScheduleNodeMatcher name(bool, isl::schedule_node &, Arg, Args...);   \
  template <typename Arg, typename... Args, typename>                          \
  friend ScheduleNodeMatcher name(Arg, Args...);
  DECL_FRIEND_TYPE_MATCH(sequence)
  DECL_FRIEND_TYPE_MATCH(set)

#undef DECL_FRIEND_TYPE_MATCH

#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  friend ScheduleNodeMatcher name(ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(isl::schedule_node &,                        \
                                  ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(bool, isl::schedule_node &,                  \
                                  ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  ScheduleNodeMatcher &&);                     \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  isl::schedule_node &,                        \
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
  // friend ScheduleNodeMatcher
  // anyTree(std::function<bool(isl::schedule_node)> callback,
  //        isl::schedule_node &);
  friend ScheduleNodeMatcher anyForest();
  friend ScheduleNodeMatcher anyForest(std::vector<isl::schedule_node> &);
  friend ScheduleNodeMatcher filterForest();
  friend ScheduleNodeMatcher filterForest(std::vector<isl::schedule_node> &);

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
  // is the matcher suppose to capture a node?
  bool needToCapture_ = false;
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
hasImmediateNextSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasDescendant(const ScheduleNodeMatcher &descendantMatcher);

std::function<bool(isl::schedule_node)>
hasChild(const ScheduleNodeMatcher &descendantMatcher);

/// And between callbacks.
/// Can be used as follow:
/// ~~~~~
/// band(_and([](isl::schedule_node n) {
///             /* callback 1 */
///             return true;
///           },
///           [](isl::schedule_node n) {
///             /* callback 2 */
///             return true;
///           }), leaf());
/// ~~~~~

template <typename... Args>
std::function<bool(isl::schedule_node)> _and(Args... args) {

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

/// Or between callbacks.

template <typename... Args>
std::function<bool(isl::schedule_node)> _or(Args... args) {

  std::vector<std::function<bool(isl::schedule_node)>> vec = {args...};

  return [vec](isl::schedule_node node) {
    std::function<bool(isl::schedule_node)> tmp = vec[0];
    bool result = tmp(node);
    for (size_t i = 1; vec.size(); i++) {
      tmp = vec[i];
      result = result or tmp(node);
    }
    return result;
  };
}
} // namespace matchers

#include "matchers-inl.h"
