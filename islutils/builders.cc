#include <isl/cpp.h>
#include <isl/id.h>
#include <islutils/builders.h>

#include <cassert>

namespace builders {

BandDescriptor::BandDescriptor(isl::schedule_node band) {
  partialSchedule = band.band_get_partial_schedule();
  int n = partialSchedule.dim(isl::dim::set);
  for (int i = 0; i < n; ++i) {
    coincident.push_back(band.band_member_get_coincident(i));
  }
  permutable =
      isl_schedule_node_band_get_permutable(band.get()) == isl_bool_true;
}

isl::schedule_node
BandDescriptor::applyPropertiesToBandNode(isl::schedule_node node) {
  int size = coincident.size();
  for (int i = 0; i < size; ++i) {
    node = node.band_member_set_coincident(i, coincident[i]);
  }
  node = isl::manage(
      isl_schedule_node_band_set_permutable(node.release(), permutable));
  return node;
}

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

    isl_union_set *uset = c.usetBuilder_().release();
    list = isl_union_set_list_add(list, uset);
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
    auto bandDescriptor = bandBuilder_();
    node = node.insert_partial_schedule(bandDescriptor.partialSchedule);
    node = bandDescriptor.applyPropertiesToBandNode(node);
  } else if (current_ == isl_schedule_node_filter) {
    // TODO: if the current node is pointing to a filter, filters are merged
    // document this in builder construction:
    // or type it so that filter cannot have filter children
    node = node.insert_filter(usetBuilder_());
  } else if (current_ == isl_schedule_node_context) {
    node = node.insert_context(setBuilder_());
  } else if (current_ == isl_schedule_node_domain) {
    if (node.get()) {
      assert(false && "cannot insert domain at some node, only at root "
                      "represented as nullptr");
      return isl::schedule_node();
    }
    node = isl::schedule_node::from_domain(usetBuilder_());
  } else if (current_ == isl_schedule_node_guard) {
    node = node.insert_guard(setBuilder_());
  } else if (current_ == isl_schedule_node_mark) {
    node = node.insert_mark(idBuilder_());
  } else if (current_ == isl_schedule_node_extension) {
    // There is no way to directly insert an extension node in isl.
    // isl_schedule_node_graft_* functions insert an extension node followed by
    // a sequence with two filters (one for the original domain points and
    // another for the introduced points) and leave the node pointer at a leaf
    // below the filter with the original domain points.  Go back to the
    // introduced sequence node and remove it, letting any child subtree to be
    // constructed as usual.
    auto extensionRoot = isl::schedule_node::from_extension(umapBuilder_());
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

  isl::union_map expansion;
  isl::union_pw_multi_aff contraction;
  if (umapBuilder_ && upmaBuilder_) {
    expansion = umapBuilder_();
    contraction = upmaBuilder_();
  } else if (umapBuilder_) {
    expansion = umapBuilder_();
    contraction = isl::union_pw_multi_aff(expansion.reverse());
  } else if (upmaBuilder_) {
    contraction = upmaBuilder_();
    expansion = isl::union_map(contraction).reverse();
  } else {
    assert(false && "neither expansion nor contraction builder provided");
  }

  if (expansion.is_identity()) {
    assert(false && "dentity expansion map will not lead to an expansion node");
  }

  // Construct the domain of the new subtree by applying the expansion map to
  // the set of domain points active at the given leaf.
  // note that .get_domain would not work for the "extended" parts of the tree
  auto parentDomain = node.get_domain();
  auto childDomain = parentDomain.apply(expansion);
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
  schedule =
      isl::manage(isl_schedule_expand(schedule.release(), contraction.release(),
                                      childRoot.get_schedule().release()));
  auto optionalNewNode =
      dfsFirst(schedule.get_root(), [markBuilder](isl::schedule_node n) {
        return isl_schedule_node_get_type(n.get()) == isl_schedule_node_mark &&
               isl_schedule_node_mark_get_id(n.get()) ==
                   markBuilder.idBuilder_().get();
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
    // automatically, i.e. there is no need to insert it. Double-check that
    // there are no children and stop here.
    if (!children_.empty()) {
      assert(false && "leaf builder has children");
      return isl::schedule_node();
    }
    // If lazy-evaluation subtree builder is provided for the leaf node, call
    // it, otherwise just return the current node.
    if (subBuilder_) {
      return subBuilder_().insertAt(node);
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

ScheduleNodeBuilder domain(std::function<isl::union_set()> callback,
                           ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_domain;
  builder.usetBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder band(std::function<BandDescriptor()> callback,
                         ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_band;
  builder.bandBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder filter(std::function<isl::union_set()> callback,
                           ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_filter;
  builder.usetBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder extension(std::function<isl::union_map()> callback,
                              ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_extension;
  builder.umapBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder expansion(std::function<isl::union_map()> callback,
                              ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_expansion;
  builder.umapBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder mark(std::function<isl::id()> callback,
                         ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_mark;
  builder.idBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder guard(std::function<isl::set()> callback,
                          ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_guard;
  builder.setBuilder_ = callback;
  builder.children_.emplace_back(child);
  return builder;
}

ScheduleNodeBuilder context(std::function<isl::set()> callback,
                            ScheduleNodeBuilder &&child) {
  ScheduleNodeBuilder builder;
  builder.current_ = isl_schedule_node_context;
  builder.setBuilder_ = callback;
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

ScheduleNodeBuilder subtreeBuilder(isl::schedule_node node) {
  ScheduleNodeBuilder builder;
  auto type = isl_schedule_node_get_type(node.get());

  if (type == isl_schedule_node_domain) {
    builder = domain([node]() { return node.domain_get_domain(); });
  } else if (type == isl_schedule_node_filter) {
    builder = filter([node]() { return node.filter_get_filter(); });
  } else if (type == isl_schedule_node_context) {
    builder = context([node]() { return node.context_get_context(); });
  } else if (type == isl_schedule_node_guard) {
    builder = guard([node]() { return node.guard_get_guard(); });
  } else if (type == isl_schedule_node_mark) {
    builder = mark([node]() { return node.mark_get_id(); });
  } else if (type == isl_schedule_node_band) {
    builder = band(
        [node]() { return BandDescriptor(node.band_get_partial_schedule()); });
  } else if (type == isl_schedule_node_extension) {
    builder = extension([node]() { return node.extension_get_extension(); });
  } else if (type == isl_schedule_node_expansion) {
    builder.current_ = type;
    builder.umapBuilder_ = [node]() { return node.expansion_get_expansion(); };
    builder.upmaBuilder_ = [node]() {
      return node.expansion_get_contraction();
    };
  } else if (type == isl_schedule_node_sequence ||
             type == isl_schedule_node_set || type == isl_schedule_node_leaf) {
    /* no payload */
  } else {
    assert(false && "unhandled node type");
  }

  int nChildren = isl_schedule_node_n_children(node.get());
  builder.children_.clear();
  for (int i = 0; i < nChildren; ++i) {
    builder.children_.push_back(subtree(node.child(i)));
  }

  return builder;
}

ScheduleNodeBuilder subtree(std::function<ScheduleNodeBuilder()> callback) {
  ScheduleNodeBuilder builder;
  builder.subBuilder_ = callback;
  return builder;
}

ScheduleNodeBuilder subtree(isl::schedule_node node) {
  return subtreeBuilder(node);
}

} // namespace builders
