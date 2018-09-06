#ifndef BUILDERS_H
#define BUILDERS_H

#include <isl/cpp.h>
#include <isl/id.h>

#include <vector>

/** \defgroup Builders Schedule Tree Builders
 */
namespace builders {

/** \ingroup Builders
 * \brief Declarative description of a schedule tree node to build.
 *
 * Data storage class for the nested-call schedule tree building API.  One can
 * think of it as essentially a different implementation of schedule trees
 * convertible to the one provided by isl.
 *
 * Instead of storing the data members directly, the class stores function
 * objects that create the properties of individual tree nodes.  This choice is
 * motivated by the declarative lazy-evaluation API around schedule tree
 * builders: when a builder is constructed, it may serve as a template for
 * multiple trees, and the data members for these trees may not exist yet.
 * This is, for example, the case when builders are used to reconstruct
 * (sub)trees captured by schedule tree matchers in an iterative fashion.
 *
 * Although each property-creation function object will be called at most once
 * during tree construction, one should take care when using non-idempotent
 * function objects with this class.  The C++ standard (as of C++17) does not
 * guarantee any order of evaluation on function arguments other that they are
 * not interleaved.
 *
 * An arbitrary schedule (sub)tree may appear at the leaves of the builder.
 * This (sub)tree is created by first creating the builder that would build it
 * and then running it to insert nodes at a leaf.  Following the
 * lazy-evaluation principle, ScheduleNodeBuilder stores a function object that
 * is called to create the subtree builder.
 */
class ScheduleNodeBuilder {
private:
  isl_union_set_list *collectChildFilters(isl::ctx) const;
  isl::schedule_node insertSequenceOrSetAt(isl::schedule_node,
                                           isl_schedule_node_type type) const;
  isl::schedule_node
  insertSingleChildTypeNodeAt(isl::schedule_node,
                              isl_schedule_node_type type) const;

  isl::schedule_node expandTree(isl::schedule_node) const;

public:
  ScheduleNodeBuilder() : current_(isl_schedule_node_leaf) {}

  isl::schedule_node insertAt(isl::schedule_node node) const;
  isl::schedule_node build() const;

public:
  isl_schedule_node_type current_;
  std::vector<ScheduleNodeBuilder> children_;

  // XXX: Cannot use a union because C++ isl types have non-trivial
  // constructors.  Cannot use std::variant because no C++17.
  std::function<isl::multi_union_pw_aff()> mupaBuilder_;
  std::function<isl::set()> setBuilder_;
  std::function<isl::union_set()> usetBuilder_;
  std::function<isl::union_map()> umapBuilder_;
  std::function<isl::union_pw_multi_aff()> upmaBuilder_;
  std::function<isl::id()> idBuilder_;

  std::function<ScheduleNodeBuilder()> subBuilder_;
};

/** \defgroup BuildersCstr Builder Constructors *
 * \ingroup Builders
 * These functions construct a tree builder by specifying the type of each
 * node, indicated by function name, and the functions providing the data
 * members of each node type.
 *
 * For convenience, overloads of these functions taking plain properties
 * instead of function objects are provided.  These overloads wrap the
 * properties into a lambda capturing them by-copy (capturing in lambdas does
 * not extend the lifetime of an object, so capturing function arguments
 * supplied by-copy by-reference leads to undefined behavior once the function
 * completes; changing the function to take arguments by-reference would
 * require the user to maintain the lifetime of the passed object at least as
 * long as the builder is alive, prohibiting temporaries, and creating
 * hard-to-detect runtime failures).
 *
 * \{
 */
ScheduleNodeBuilder domain(std::function<isl::union_set()> callback,
                           ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
domain(isl::union_set uset,
       ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return domain([uset]() { return uset; }, std::move(child));
}

ScheduleNodeBuilder band(std::function<isl::multi_union_pw_aff()> callback,
                         ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
band(isl::multi_union_pw_aff mupa,
     ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return band([mupa]() { return mupa; }, std::move(child));
}

ScheduleNodeBuilder filter(std::function<isl::union_set()> callback,
                           ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
filter(isl::union_set uset,
       ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return filter([uset]() { return uset; }, std::move(child));
}

ScheduleNodeBuilder
extension(std::function<isl::union_map()> callback,
          ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
extension(isl::union_map umap,
          ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return extension([umap]() { return umap; }, std::move(child));
}

ScheduleNodeBuilder
expansion(std::function<isl::union_map()> callback,
          ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
ScheduleNodeBuilder inline expansion(
    isl::union_map umap, ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return expansion([umap]() { return umap; }, std::move(child));
}

ScheduleNodeBuilder mark(std::function<isl::id()> callback,
                         ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
mark(isl::id id, ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return mark([id]() { return id; }, std::move(child));
}

ScheduleNodeBuilder guard(std::function<isl::set()> callback,
                          ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
guard(isl::set set, ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return guard([set]() { return set; }, std::move(child));
}

ScheduleNodeBuilder
context(std::function<isl::set()> callback,
        ScheduleNodeBuilder &&child = ScheduleNodeBuilder());
inline ScheduleNodeBuilder
context(isl::set set, ScheduleNodeBuilder &&child = ScheduleNodeBuilder()) {
  return context([set]() { return set; });
}

template <typename... Args, typename = typename std::enable_if<std::is_same<
                                typename std::common_type<Args...>::type,
                                ScheduleNodeBuilder>::value>::type>
ScheduleNodeBuilder set(Args... children) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_set;
  builder.children_ = {children...};
  return builder;
}

ScheduleNodeBuilder set(std::vector<ScheduleNodeBuilder> &&children);

template <typename T,
          typename = typename std::enable_if<
              std::is_same<T, ScheduleNodeBuilder>::value ||
              std::is_same<T, std::vector<ScheduleNodeBuilder>>::value>::type>
std::vector<ScheduleNodeBuilder> varargToVector(T t) {
  return {t};
}

template <typename T, class... Args>
std::vector<ScheduleNodeBuilder> varargToVector(T t, Args... args) {
  std::vector<ScheduleNodeBuilder> rest = varargToVector(args...);
  std::vector<ScheduleNodeBuilder> result = {t};
  result.insert(std::end(result), std::make_move_iterator(rest.begin()),
                std::make_move_iterator(rest.end()));
  return result;
}

template <class... Args> ScheduleNodeBuilder sequence(Args... args) {
  std::vector<ScheduleNodeBuilder> children = varargToVector(args...);
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_sequence;
  builder.children_ = children;
  return builder;
}

/** Create a schedule node builder that replicates the given schedule node.
 */
ScheduleNodeBuilder subtreeBuilder(isl::schedule_node node);

/** Construct a lazily-evaluated schedule tree builder that reconstructs the
 * subtree rooted at the given node. */
ScheduleNodeBuilder subtree(isl::schedule_node node);

/** Construct a lazily-evaluated schedule tree builder that forwards control
 * over the subtree construction to the another builder returned by the
 * callback.  Typically used with subtreeBuilder(). */
ScheduleNodeBuilder subtree(std::function<ScheduleNodeBuilder()> callback);

} // namespace builders

#endif // BUILDERS_H
