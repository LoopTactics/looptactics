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

// It describes the type of matcher.
// read - read access
// write - write access
// readAndWrite - read and write access

enum class RelationKind { read, write, readAndWrite };

class RelationMatcher;

// TODO: change std::string.
typedef std::vector<std::string> matchingDims;

// TODO: extend to use variadic template
class RelationMatcher {
#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  friend RelationMatcher name(char a, char b);                                 \
  friend RelationMatcher name(char a);
  DECL_FRIEND_TYPE_MATCH(read)
  DECL_FRIEND_TYPE_MATCH(write)
#undef DECL_FRIEND_TYPE_MATCH

public:
  // is a read access?
  bool isRead() const;
  // is a write access?
  bool isWrite() const;
  // return literal at index i
  char getIndex(unsigned i) const;
  // get number of literals
  int getIndexesSize() const;
  ~RelationMatcher() = default;

private:
  // type (read, write or readAndWrite)
  RelationKind type_;
  // describe how the indexes should look like. Indexes layout.
  std::vector<char> indexes_;
  // once we figured out a combination that
  // satisfy all the matcher we "fixed" the
  // dimensions.
  std::vector<matchingDims> setDim_;
};

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

template <typename... Args>
ScheduleNodeMatcher sequence(std::function<bool(isl::schedule_node)> callback,
                             Args... args);

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

template <typename... Args>
ScheduleNodeMatcher set(std::function<bool(isl::schedule_node)> callback,
                        Args... args);

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

ScheduleNodeMatcher leaf();

ScheduleNodeMatcher any(isl::schedule_node &capture);
ScheduleNodeMatcher any();
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

  Any
};

inline isl_schedule_node_type toIslType(ScheduleNodeType type);
inline ScheduleNodeType fromIslType(isl_schedule_node_type type);

/** Node type matcher class for isl schedule trees.
 * \ingroup Matchers
 */
class ScheduleNodeMatcher {
#define DECL_FRIEND_TYPE_MATCH(name)                                           \
  template <typename... Args>                                                  \
  friend ScheduleNodeMatcher name(std::function<bool(isl::schedule_node)>,     \
                                  Args...);                                    \
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

#undef DECL_FRIEND_TYPE_MATCH

  friend ScheduleNodeMatcher leaf();
  friend ScheduleNodeMatcher any();
  friend ScheduleNodeMatcher any(isl::schedule_node &);

private:
  explicit ScheduleNodeMatcher(isl::schedule_node &capture)
      : capture_(capture) {}

public:
  static bool isMatching(const ScheduleNodeMatcher &matcher,
                         isl::schedule_node node);

private:
  ScheduleNodeType current_;
  std::vector<ScheduleNodeMatcher> children_;
  std::function<bool(isl::schedule_node)> nodeCallback_;
  isl::schedule_node &capture_;
};

std::function<bool(isl::schedule_node)>
hasPreviousSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasNextSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasSibling(const ScheduleNodeMatcher &siblingMatcher);

std::function<bool(isl::schedule_node)>
hasDescendant(const ScheduleNodeMatcher &descendantMatcher);

} // namespace matchers

#include "matchers-inl.h"

// A constraint is introduced by an access and a matcher.
// In more details, a constraint looks like (A, i0). Meaning that
// we have assigned dimension i0 to literal A.
/*
namespace constraint {

// represents single constraint.
typedef std::tuple<char, isl::pw_aff> singleConstraint;
// represents collection of constraints.
typedef std::vector<singleConstraint> MultipleConstraints;

// TODO: check if we can avoid int dimsInvolved.
// decouple matcher from constraint list.
struct MatcherConstraints {
  int dimsInvolved = -1;
  MultipleConstraints constraints;
};

// helper function for printing single constraint.
inline void print_single_constraint(raw_ostream &OS,
                                    const singleConstraint &c) {
  OS << std::get<0>(c) << "," << std::get<1>(c).to_str();
}

// overloading << for printing single constraint.
inline auto& operator<<(raw_ostream &OS, const singleConstraint &c) {
  OS << "(";
  print_single_constraint(OS, c);
  return OS << ")";
}

// helper function for multiple constraints.
inline void print_multiple_constraints(raw_ostream &OS,
                                       const MultipleConstraints &mc) {
  for(std::size_t i = 0; i < mc.size()-1; ++i) {
    OS << mc[i] << ",";
  }
  OS << mc[mc.size()-1];
}

// overloading << for multiple constraints.
inline auto& operator<<(raw_ostream &OS, const MultipleConstraints &mc) {
  OS << "[";
  print_multiple_constraints(OS, mc);
  return OS << "]";
}

// overloading << for MatcherConstraints
inline auto& operator<<(raw_ostream &OS, const MatcherConstraints &mc) {
  OS << "{";
  OS << "\n";
  OS << "Involved Dims = " << mc.dimsInvolved << "\n";
  if(mc.dimsInvolved == -1) {
    OS << "Constraints = empty";
    OS << "\n";
    return OS << "}";
  }
  OS << "Constraints = " << mc.constraints;
  OS << "\n";
  return OS << "}";
}

} // namespace constraint
*/
