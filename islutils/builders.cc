#include <isl/cpp.h>
#include <isl/id.h>
#include <islutils/builders.h>

#include <cassert>

namespace builders {

isl_union_set_list *
ScheduleNodeBuilder::collectChildFilters(isl::ctx ctx) const {
  if (children_.empty()) {
    assert(false && "no children of a sequence/set node");
    return nullptr;
  }

  isl_union_set_list *list =
      isl_union_set_list_alloc(ctx.get(), static_cast<int>(children_.size()));

  for (const auto &c : children_) {
    if (c.current_ != isl_schedule_node_filter) {
      assert(false && "children of sequence/set must be filters");
      return nullptr;
    }

    list = isl_union_set_list_add(list, c.uset_.copy());
  }
  return list;
}

isl::schedule_node
ScheduleNodeBuilder::insertSequenceOrSetAt(isl::schedule_node node,
                                           isl_schedule_node_type type) const {
  auto filterList = collectChildFilters(node.get_ctx());
  if (type == isl_schedule_node_sequence) {
    node = isl::manage(
        isl_schedule_node_insert_sequence(node.release(), filterList));
  } else if (type == isl_schedule_node_set) {
    node =
        isl::manage(isl_schedule_node_insert_set(node.release(), filterList));
  } else {
    assert(false && "unsupported node type");
  }
  // The function above inserted both the sequence and the filter child nodes,
  // so go to grandchildren directly. After collection took place, we know
  // children exist and they are all fitler types (probably lift that logic
  // here).
  for (size_t i = 0, ei = children_.size(); i < ei; ++i) {
    auto childNode = node.child(i);
    const auto &childBuilder = children_.at(i);
    if (childBuilder.children_.size() > 1) {
      assert(false && "more than one child of a filter node");
      return isl::schedule_node();
    } else if (childBuilder.children_.size() == 1) {
      auto grandChildNode = childNode.child(0);
      grandChildNode = childBuilder.children_.at(0).insertAt(grandChildNode);
      childNode = grandChildNode.parent();
    }
    node = childNode.parent();
  }
  return node;
}

isl::schedule_node ScheduleNodeBuilder::insertSingleChildTypeNodeAt(
    isl::schedule_node node, isl_schedule_node_type type) const {
  if (current_ == isl_schedule_node_band) {
    node = isl::manage(isl_schedule_node_insert_partial_schedule(node.release(),
                                                                 mupa_.copy()));
  } else if (current_ == isl_schedule_node_filter) {
    // TODO: if the current node is pointing to a filter, filters are merged
    // document this in builder construction:
    // or type it so that filter cannot have filter children
    node = isl::manage(
        isl_schedule_node_insert_filter(node.release(), uset_.copy()));
  } else if (current_ == isl_schedule_node_context) {
    node = isl::manage(
        isl_schedule_node_insert_context(node.release(), set_.copy()));
  } else if (current_ == isl_schedule_node_domain) {
    if (node.get()) {
      assert(false && "cannot insert domain at some node, only at root "
                      "represented as nullptr");
      return isl::schedule_node();
    }
    node = isl::manage(isl_schedule_node_from_domain(uset_.copy()));
  } else if (current_ == isl_schedule_node_guard) {
    node = isl::manage(
        isl_schedule_node_insert_guard(node.release(), set_.copy()));
  } else if (current_ == isl_schedule_node_mark) {
    node =
        isl::manage(isl_schedule_node_insert_mark(node.release(), id_.copy()));
  } else if (current_ == isl_schedule_node_extension) {
    // There is no way to directly insert an extension node in isl.
    // isl_schedule_node_graft_* functions insert an extension node followed by
    // a sequence with two filters (one for the original domain points and
    // another for the introduced points) and leave the node pointer at a leaf
    // below the filter with the original domain points.  Go back to the
    // introduced sequence node and remove it, letting any child subtree to be
    // constructed as usual.
    auto extensionRoot =
        isl::manage(isl_schedule_node_from_extension(umap_.copy()));
    node = isl::manage(isl_schedule_node_graft_before(node.release(),
                                                      extensionRoot.release()))
               .parent()
               .parent();
    node = node.cut();
    node = node.parent();
  }

  if (children_.size() > 1) {
    assert(false && "more than one child of non-set/sequence node");
    return isl::schedule_node();
  }
  // Because of CoW, node can change so we cannot return it directly, we
  // rather recurse to children, take what was returned and take its parent.
  return children_.empty() ? node
                           : children_.at(0).insertAt(node.child(0)).parent();
}

// Depth-first search through the tree, returning first match using vector as
// optional (empty vector means no match).
static std::vector<isl::schedule_node>
dfsFirst(isl::schedule_node root,
         std::function<bool(isl::schedule_node)> matcher) {
  if (matcher(root)) {
    return {root};
  }

  for (int i = 0, e = root.n_children(); i < e; ++i) {
    root = root.child(i);
    auto result = dfsFirst(root, matcher);
    if (!result.empty()) {
      return result;
    }
    root = root.parent();
  }

  return {};
}

// For a builder of expansion node, build a separate schedule tree starting at
// this node as domain and than attach it to the original tree at a leaf
// indicated by "node".
// Contraty to isl_schedule_node_group, this method does not modify the nodes
// on the way to root and therefore is insensitive to anchoring problems.
isl::schedule_node
ScheduleNodeBuilder::expandTree(isl::schedule_node node) const {
  if (current_ != isl_schedule_node_expansion) {
    assert(false && "only call expandTree on expansion builder");
  }

  // Construct the domain of the new subtree by applying the expansion map to
  // the set of domain points active at the given leaf.
  // note that .get_domain would not work for the "extended" parts of the tree
  auto parentDomain = node.get_domain();
  auto childDomain = parentDomain.apply(umap_);
  auto childRoot = isl::schedule_node::from_domain(childDomain);
  childRoot = children_.at(0).insertAt(childRoot.child(0)).parent();

  // Insert a mark node so that we can find the position in the transformed
  // tree (yes, this is quite ugly but seems to be the only way around CoW).
  auto markBuilder = mark(
      isl::id::alloc(node.get_ctx(), "__islutils_expand_builder", nullptr));
  node = markBuilder.insertAt(node);

  // Transform the entire schedule and find the corresponding location by
  // DFS-lookup of the mark node.  Remove the mark node and return its child
  // (the newly inserted expansion node) for continuation.
  auto schedule = node.get_schedule();
  schedule = isl::manage(isl_schedule_expand(
      schedule.release(), upma_.copy(), childRoot.get_schedule().release()));
  auto optionalNewNode =
      dfsFirst(schedule.get_root(), [markBuilder](isl::schedule_node n) {
        return isl_schedule_node_get_type(n.get()) == isl_schedule_node_mark &&
               isl_schedule_node_mark_get_id(n.get()) == markBuilder.id_.get();
      });
  if (optionalNewNode.empty()) {
    assert(false && "could not find mark node after expansion");
  }
  node = optionalNewNode.at(0);
  node = isl::manage(isl_schedule_node_delete(node.release()));
  return node;
}

// need to insert at child?
isl::schedule_node
ScheduleNodeBuilder::insertAt(isl::schedule_node node) const {
  if (current_ == isl_schedule_node_band ||
      current_ == isl_schedule_node_filter ||
      current_ == isl_schedule_node_mark ||
      current_ == isl_schedule_node_guard ||
      current_ == isl_schedule_node_context ||
      current_ == isl_schedule_node_domain ||
      current_ == isl_schedule_node_extension) {
    return insertSingleChildTypeNodeAt(node, current_);
  } else if (current_ == isl_schedule_node_sequence ||
             current_ == isl_schedule_node_set) {
    return insertSequenceOrSetAt(node, current_);
  } else if (current_ == isl_schedule_node_expansion) {
    return expandTree(node);
  } else if (current_ == isl_schedule_node_leaf) {
    // Leaf is a special type in isl that has no children, it gets added
    // automatically, i.e. there is no need to insert it. Builder with leaf
    // type can only be constructed from isl subtree.  Double-check that there
    // are no children and stop here.
    if (!children_.empty()) {
      assert(false && "leaf builder has children");
      return isl::schedule_node();
    }
    return node;
  }

  assert(false && "unsupported node type");
  return isl::schedule_node();
}

isl::schedule_node ScheduleNodeBuilder::build() const {
  if (current_ != isl_schedule_node_domain) {
    assert(false && "can only build trees with a domain node as root");
    return isl::schedule_node();
  }
  return insertAt(isl::schedule_node());
}

ScheduleNodeBuilder domain(isl::union_set uset) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_domain;
  builder.uset_ = uset;
  return builder;
}

ScheduleNodeBuilder domain(isl::union_set uset, ScheduleNodeBuilder &&child) {
  auto builder = domain(uset);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder band(isl::multi_union_pw_aff mupa) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_band;
  builder.mupa_ = mupa;
  return builder;
}

ScheduleNodeBuilder band(isl::multi_union_pw_aff mupa,
                         ScheduleNodeBuilder &&child) {
  auto builder = band(mupa);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder filter(isl::union_set uset) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_filter;
  builder.uset_ = uset;
  return builder;
}

ScheduleNodeBuilder filter(isl::union_set uset, ScheduleNodeBuilder &&child) {
  auto builder = filter(uset);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder extension(isl::union_map umap) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_extension;
  builder.umap_ = umap;
  return builder;
}

ScheduleNodeBuilder extension(isl::union_map umap,
                              ScheduleNodeBuilder &&child) {
  auto builder = extension(umap);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder expansion(isl::union_map umap) {
  if (umap.is_identity()) {
    assert(false && "dentity expansion map will not lead to an expansion node");
  }

  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_expansion;
  builder.umap_ = umap;
  builder.upma_ = isl::union_pw_multi_aff(umap.reverse());
  return builder;
}

ScheduleNodeBuilder expansion(isl::union_map umap,
                              ScheduleNodeBuilder &&child) {
  auto builder = expansion(umap);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder mark(isl::id id) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_mark;
  builder.id_ = id;
  return builder;
}

ScheduleNodeBuilder mark(isl::id id, ScheduleNodeBuilder &&child) {
  auto builder = mark(id);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder guard(isl::set set) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_guard;
  builder.set_ = set;
  return builder;
}

ScheduleNodeBuilder guard(isl::set set, ScheduleNodeBuilder &&child) {
  auto builder = guard(set);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder context(isl::set set) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_context;
  builder.set_ = set;
  return builder;
}

ScheduleNodeBuilder context(isl::set set, ScheduleNodeBuilder &&child) {
  auto builder = context(set);
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder sequence(std::vector<ScheduleNodeBuilder> &&children) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_sequence;
  builder.children_ = children;
  return builder;
}

ScheduleNodeBuilder set(std::vector<ScheduleNodeBuilder> &&children) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_set;
  builder.children_ = children;
  return builder;
}

ScheduleNodeBuilder subtree(isl::schedule_node node) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_get_type(node.get());

  if (builder.current_ == isl_schedule_node_domain) {
    builder.uset_ =
        isl::manage(isl_schedule_node_domain_get_domain(node.get()));
  } else if (builder.current_ == isl_schedule_node_filter) {
    builder.uset_ =
        isl::manage(isl_schedule_node_filter_get_filter(node.get()));
  } else if (builder.current_ == isl_schedule_node_context) {
    builder.set_ =
        isl::manage(isl_schedule_node_context_get_context(node.get()));
  } else if (builder.current_ == isl_schedule_node_guard) {
    builder.set_ = isl::manage(isl_schedule_node_guard_get_guard(node.get()));
  } else if (builder.current_ == isl_schedule_node_mark) {
    builder.id_ = isl::manage(isl_schedule_node_mark_get_id(node.get()));
  } else if (builder.current_ == isl_schedule_node_band) {
    builder.mupa_ =
        isl::manage(isl_schedule_node_band_get_partial_schedule(node.get()));
  } else if (builder.current_ == isl_schedule_node_extension) {
    builder.umap_ =
        isl::manage(isl_schedule_node_extension_get_extension(node.get()));
  } else if (builder.current_ == isl_schedule_node_expansion) {
    builder.umap_ =
        isl::manage(isl_schedule_node_expansion_get_expansion(node.get()));
    builder.upma_ =
        isl::manage(isl_schedule_node_expansion_get_contraction(node.get()));
  } else if (builder.current_ == isl_schedule_node_sequence ||
             builder.current_ == isl_schedule_node_set ||
             builder.current_ == isl_schedule_node_leaf) {
    /* no payload */
  } else {
    assert(false && "unhandled node type");
  }

  int nChildren = isl_schedule_node_n_children(node.get());
  for (int i = 0; i < nChildren; ++i) {
    builder.children_.push_back(subtree(node.child(i)));
  }

  return builder;
}

} // namespace builders
