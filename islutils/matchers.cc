#include "islutils/matchers.h"

namespace matchers {

/*
void RelationMatcher::printMatcher(raw_ostream &OS,
  				   int indent) const {
  switch (current_) {
    case RelationKind::read:
      OS.indent(indent) << "Read access\n";
      break;
    case RelationKind::write:
      OS.indent(indent) << "Write access\n";
      break;
    case RelationKind::readOrWrite:
      OS.indent(indent) << "ReadOrWrite\n";
      break;
    default:
      OS.indent(indent) << "ND\n";
    }

  int n_children_ = Indexes_.size();
  for(int i=0; i < n_children_; ++i) {
    OS.indent(indent) << Indexes_[i] << "\n";
  }

  int n_dims_ = SetDim_.size();
  OS.indent(indent) << "Number of dims: =" << n_dims_ << "\n";

  for(size_t i=0; i<n_dims_; ++i) {
    auto payload = SetDim_[i];
    for(size_t j=0; j<payload.size(); ++j) {
      OS.indent(indent +2) << payload[j] << "\n";
    }
  }
}


// returns the number of literals assigned to the matcher.
int RelationMatcher::getIndexesSize() const {
  return indexes_.size();
}

// returns literal at position i
char RelationMatcher::getIndex(unsigned i) const {
  return indexes_[i];
}

// is a write matcher?
bool RelationMatcher::isWrite() const {
  if((type_ == RelationKind::write) ||
     (type_ == RelationKind::readOrWrite))
    return true;
  else return false;
}

// is a read matcher?
bool RelationMatcher::isRead() const {
  if((type_ == RelationKind::read) ||
     (type_ == RelationKind::readOrWrite))
    return true;
  else return false;
}


// print the structure of the tree matcher.
void ScheduleNodeMatcher::printMatcher(raw_ostream &OS,
                                       const ScheduleNodeMatcher &matcher,
                                       int indent) const {

  switch (matcher.current_) {
  case ScheduleNodeKind::sequence:
    OS.indent(indent) << "Sequence Node\n";
    break;
  case ScheduleNodeKind::set:
    OS.indent(indent) << "Set Node\n";
    break;
  case ScheduleNodeKind::band:
    OS.indent(indent) << "Band Node\n";
    break;
  case ScheduleNodeKind::context:
    OS.indent(indent) << "Context Node\n";
    break;
  case ScheduleNodeKind::domain:
    OS.indent(indent) << "Domain Node\n";
    break;
  case ScheduleNodeKind::extension:
    OS.indent(indent) << "Extension Node\n";
    break;
  case ScheduleNodeKind::filter:
    OS.indent(indent) << "Filter Node\n";
    break;
  case ScheduleNodeKind::guard:
    OS.indent(indent) << "Guard Node\n";
    break;
  case ScheduleNodeKind::mark:
    OS.indent(indent) << "Mark Node\n";
    break;
  case ScheduleNodeKind::leaf:
    OS.indent(indent) << "Leaf Node\n";
    break;
  default:
    OS.indent(indent) << "ND\n";
  }

  if (matcher.children_.empty()) {
    return;
  }

  int n_children_ = matcher.children_.size();
  for (int i = 0; i < n_children_; ++i) {
    printMatcher(OS, matcher.children_[i], indent + 2);
  }

  OS << "\n";
}
*/

bool ScheduleNodeMatcher::isMatching(const ScheduleNodeMatcher &matcher,
                                     isl::schedule_node node) {
  if (!node.get()) {
    return false;
  }

  if (matcher.current_ == ScheduleNodeType::Any) {
    return true;
  }

  if (toIslType(matcher.current_) != isl_schedule_node_get_type(node.get())) {
    return false;
  }

  if (matcher.nodeCallback_ && !matcher.nodeCallback_(node)) {
    return false;
  }

  size_t nChildren =
      static_cast<size_t>(isl_schedule_node_n_children(node.get()));
  if (matcher.children_.size() != nChildren) {
    return false;
  }

  for (size_t i = 0; i < nChildren; ++i) {
    if (!isMatching(matcher.children_.at(i), node.child(i))) {
      return false;
    }
  }
  return true;
}

static bool hasPreviousSiblingImpl(isl::schedule_node node,
                                   const ScheduleNodeMatcher &siblingMatcher) {
  while (isl_schedule_node_has_previous_sibling(node.get()) == isl_bool_true) {
    node = isl::manage(isl_schedule_node_previous_sibling(node.release()));
    if (ScheduleNodeMatcher::isMatching(siblingMatcher, node)) {
      return true;
    }
  }
  return false;
}

static bool hasNextSiblingImpl(isl::schedule_node node,
                               const ScheduleNodeMatcher &siblingMatcher) {
  while (isl_schedule_node_has_next_sibling(node.get()) == isl_bool_true) {
    node = isl::manage(isl_schedule_node_next_sibling(node.release()));
    if (ScheduleNodeMatcher::isMatching(siblingMatcher, node)) {
      return true;
    }
  }
  return false;
}

std::function<bool(isl::schedule_node)>
hasPreviousSibling(const ScheduleNodeMatcher &siblingMatcher) {
  return std::bind(hasPreviousSiblingImpl, std::placeholders::_1,
                   siblingMatcher);
}

std::function<bool(isl::schedule_node)>
hasNextSibling(const ScheduleNodeMatcher &siblingMatcher) {
  return std::bind(hasNextSiblingImpl, std::placeholders::_1, siblingMatcher);
}

std::function<bool(isl::schedule_node)>
hasSibling(const ScheduleNodeMatcher &siblingMatcher) {
  return [siblingMatcher](isl::schedule_node node) {
    return hasPreviousSiblingImpl(node, siblingMatcher) ||
           hasNextSiblingImpl(node, siblingMatcher);
  };
}

std::function<bool(isl::schedule_node)>
hasDescendant(const ScheduleNodeMatcher &descendantMatcher) {
  isl::schedule_node n;
  return [descendantMatcher](isl::schedule_node node) {
    // Cannot use capturing lambdas as C function pointers.
    struct Data {
      bool found;
      const ScheduleNodeMatcher &descendantMatcher;
    };
    Data data{false, descendantMatcher};

    auto r = isl_schedule_node_foreach_descendant_top_down(
        node.get(),
        [](__isl_keep isl_schedule_node *cn, void *user) -> isl_bool {
          auto data = static_cast<Data *>(user);
          if (data->found) {
            return isl_bool_false;
          }

          auto n = isl::manage_copy(cn);
          data->found =
              ScheduleNodeMatcher::isMatching(data->descendantMatcher, n);
          return data->found ? isl_bool_false : isl_bool_true;
        },
        &data);
    return r == isl_stat_ok && data.found;
  };
}

} // namespace matchers

/*
namespace constraint {

struct MatcherConstraints;

// build a set of constraints for a given matcher and the given
// memory accesses. It returns a struct containing the dimensions involved
// (size of Indexes_ in the matcher) and the list of constraints introduced
// by the matcher.
MatcherConstraints buildMatcherConstraints(const matchers::RelationMatcher &m,
                                            llvm::SmallVector<polly::MemoryAccess*, 32> &a) {
  MatcherConstraints matcherConstraints;
  for(auto *MemA = a.begin(); MemA != a.end(); MemA++) {
    polly::MemoryAccess *MemAccessPtr = *MemA;
    if(m.isMatching(*MemAccessPtr)) {
      auto AccMap = MemAccessPtr->getLatestAccessRelation();
      isl::pw_multi_aff MultiAff = isl::pw_multi_aff::from_map(AccMap);
      for(unsigned u = 0; u < AccMap.dim(isl::dim::out); ++u) {
        isl::pw_aff PwAff = MultiAff.get_pw_aff(u);
        auto a = std::make_tuple(m.getIndex(u), PwAff);
        matcherConstraints.constraints.push_back(a);
      }
    }
  }
  matcherConstraints.dimsInvolved = m.getIndexesSize();
  return matcherConstraints;
}

// check if two singleConstraint are equal.
bool isEqual(singleConstraint &a, singleConstraint &b) {
  if((std::get<0>(a) == std::get<0>(b)) &&
        (std::get<1>(a).to_str().compare(std::get<1>(b).to_str()) == 0))
    return true;
  else return false;
}

// remove duplicate singleConstraint
// from a list of singleConstraints
void removeDuplicate(MultipleConstraints &c) {
  for(int i=0; i<c.size(); ++i) {
    for(int j=i+1; j<c.size(); ++j) {
      if(isEqual(c[i],c[j])) {
        //LLVM_DEBUG(dbgs() << "removing\n");
        //LLVM_DEBUG(dbgs() << c[j] << "\n");
        //LLVM_DEBUG(dbgs() << c[i] << "\n");
        //LLVM_DEBUG(dbgs() << "j=" << j << "\n");
        //LLVM_DEBUG(dbgs() << "i=" << i << "\n");
        c.erase(c.begin()+j);
      }
    }
  }
}

// new constrain list generated by comparing two
// different matchers. We combine the constraints of
// two matchers and we output a new list of constraints.
// i starting index for valid constraint matcher one
// j starting index for valid constraint matcher two
// TODO: notice that for now we are assuming a single
// matching.
MatcherConstraints createNewConstrainList(int i, int j,
                                         MatcherConstraints &mOne, MatcherConstraints &mTwo) {

  MatcherConstraints result;
  int dimsInvolvedOne = mOne.dimsInvolved;
  int dimsInvolvedTwo = mTwo.dimsInvolved;

  MultipleConstraints newConstraints;
  for(int ii=i; ii<dimsInvolvedOne+i; ++ii) {
    auto a = std::make_tuple(std::get<0>(mOne.constraints[ii]),
                             std::get<1>(mOne.constraints[ii]));
    //LLVM_DEBUG(dbgs() << a << "\n");
    newConstraints.push_back(a);
  }
  //LLVM_DEBUG(dbgs() << "#####\n");
  for(int jj=j; jj<dimsInvolvedTwo+j; ++jj) {
    auto a = std::make_tuple(std::get<0>(mTwo.constraints[jj]),
                             std::get<1>(mTwo.constraints[jj]));
    //LLVM_DEBUG(dbgs() << a << "\n");
    newConstraints.push_back(a);
  }

  //LLVM_DEBUG(dbgs() << newConstraints.size() << "\n");
  removeDuplicate(newConstraints);
  result.constraints = newConstraints;
  result.dimsInvolved = newConstraints.size();
  //LLVM_DEBUG(dbgs() << newConstraints.size() << "\n");
  //LLVM_DEBUG(dbgs() << "result : =" << result << "\n");
  return result;
}

// TODO: for now we assume a single match.
// brute force: we compare all the possibilities and we try
// to find out one that satisfies the constraints for the two input lists.
// the if checks the following conditions:
// 1. (B, i1) and (c, i1) [this should be rejected since i1 is assigned both to B and C]
// 2. (A, i0) and (A, i2) [this should be rejected since A is assigned both to i0 and i2]
MatcherConstraints  compareLists(MatcherConstraints &mOne, MatcherConstraints &mTwo) {
  //int dummyDebug = 0;
  //LLVM_DEBUG(dbgs() << mOne.constraints << "\n");
  //LLVM_DEBUG(dbgs() << mTwo.constraints << "\n");
  MatcherConstraints result;
  int sizeListOne = mOne.constraints.size();
  int sizeListTwo = mTwo.constraints.size();
  int dimsInvolvedOne = mOne.dimsInvolved;
  int dimsInvolvedTwo = mTwo.dimsInvolved;
  for(int i=0; i<sizeListOne; i+=dimsInvolvedOne) {
    for(int j=0; j<sizeListTwo; j+=dimsInvolvedTwo) {
      bool isPossible = true;
      for(int ii=i; ii<i+dimsInvolvedOne; ++ii) {
        for(int jj=j; jj<j+dimsInvolvedTwo; ++jj) {
          //LLVM_DEBUG(dbgs() << "mOne label : " << std::get<0>(mOne.constraints[ii]) << "\n");
          //LLVM_DEBUG(dbgs() << "mTwo label : " << std::get<0>(mTwo.constraints[jj]) << "\n");
          //LLVM_DEBUG(dbgs() << "mOne PW : " << std::get<1>(mOne.constraints[ii]).to_str() << "\n");
          //LLVM_DEBUG(dbgs() << "mTwo PW : " << std::get<1>(mTwo.constraints[jj]).to_str() << "\n");
          //LLVM_DEBUG(dbgs() << "ii" << ii << "\n");
          //LLVM_DEBUG(dbgs() << "*********** " << "\n");
          if((std::get<0>(mOne.constraints[ii]) != std::get<0>(mTwo.constraints[jj]) &&
             (std::get<1>(mOne.constraints[ii]).to_str().compare(std::get<1>(mTwo.constraints[jj]).to_str()) == 0)) ||
             (std::get<0>(mOne.constraints[ii]) == std::get<0>(mTwo.constraints[jj]) &&
             (std::get<1>(mOne.constraints[ii]).to_str().compare(std::get<1>(mTwo.constraints[jj]).to_str()) != 0))) {
            isPossible = false;
            //LLVM_DEBUG(dbgs() << "not pass\n");
          }
          else {
            //bool cond = std::get<0>(mOne.constraints[ii]) == std::get<0>(mTwo.constraints[jj]);
            //bool cond1 = (std::get<1>(mOne.constraints[ii]).to_str().compare(std::get<1>(mTwo.constraints[jj]).to_str()) == 0);
            //LLVM_DEBUG(dbgs() << "cond 1 " << cond << "\n");
            //LLVM_DEBUG(dbgs() << "cond 2 " << cond1 << "\n");
            //LLVM_DEBUG(dbgs() << "pass" << "\n");
          }
        }
      }
      //LLVM_DEBUG(dbgs() << "TUPLE =" << isPossible << "\n");
      //LLVM_DEBUG(dbgs() << "DummyDebug =" << ++dummyDebug << "\n");
      if(isPossible) {
        //TODO: for now we assume a single match.
        result = createNewConstrainList(i, j, mOne, mTwo);
        //LLVM_DEBUG(dbgs() << "index i = " << i << "\n");
        //LLVM_DEBUG(dbgs() << "index j = " << j << "\n");
        //LLVM_DEBUG(dbgs() << "possible comb " << "\n");
        //for(int ii=i; ii<dimsInvolvedOne+i; ++ii) {
        //  LLVM_DEBUG(dbgs() << std::get<0>(mOne.constraints[ii]) << "\n");
        //  LLVM_DEBUG(dbgs() << std::get<1>(mOne.constraints[ii]).to_str() << "\n");
        //}
        //for(int jj=j; jj<dimsInvolvedTwo+j; ++jj) {
        //  LLVM_DEBUG(dbgs() << std::get<0>(mTwo.constraints[jj]) << "\n");
        //  LLVM_DEBUG(dbgs() << std::get<1>(mTwo.constraints[jj]).to_str() << "\n");
        //}
      }
    }
  }
  return result;
}

} // namespace constraint
*/
