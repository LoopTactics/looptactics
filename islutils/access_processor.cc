#include "islutils/access_processor.h"
#include "islutils/common.h"
#include "islutils/access_patterns.h"

bool walkTree(const matchers::ScheduleNodeMatcher matcher, isl::schedule_node node) {

  using namespace matchers;
  if(!node.get()) {
    return false;
  }

  if(ScheduleNodeMatcher::isMatching(matcher, node)) {
    return true;
  }

  size_t nChildren =
    static_cast<size_t>(isl_schedule_node_n_children(node.get()));
  for (size_t i = 0; i < nChildren; ++i) {
    if(!ScheduleNodeMatcher::isMatching(matcher, node)) {
      return false;
    }
  }
  
  return true;
} 
  

bool is2DTransposeMatching(Scop scop) {

  auto deps = computeAllDependences(scop);
  scop.schedule =
    mergeIfTilable(scop.schedule.get_root(), deps).get_schedule();

  // matchers for 2D transpose.
  auto is2DTranspose = [=](isl::schedule_node band) {
    using util::ScopedCtx;
    auto ctx = isl::ctx(isl_ctx_alloc());

    using namespace matchers;
    auto prefixSchedule = band.child(0).get_prefix_schedule_union_map();
    auto scheduledReads = scop.reads.curry().apply_domain(prefixSchedule);
    auto scheduledWrites = scop.mustWrites.curry().apply_domain(prefixSchedule);

    auto _A_read = arrayPlaceholder();
    auto _i_read = placeholder(ctx);
    auto _j_read = placeholder(ctx);

    auto _B_write = arrayPlaceholder();
    auto _i_write = placeholder(ctx);
    auto _j_write = placeholder(ctx);

    auto psRead = allOf(access(_A_read, _i_read, _j_read));
    auto readMatches = match(scheduledReads, psRead);

    auto psWrite = allOf(access(_B_write, _i_write, _j_write));
    auto writeMatches = match(scheduledWrites, psWrite);

    std::cout << readMatches.size() << std::endl;

    if(readMatches.size() != 1u)
      return false;
    if(writeMatches.size() != 1u)
      return false;
    if(writeMatches[0][_i_write].payload().inputDimPos_ !=
       readMatches[0][_j_read].payload().inputDimPos_)
      return false;
    if(writeMatches[0][_j_write].payload().inputDimPos_ !=
       readMatches[0][_i_read].payload().inputDimPos_)
      return false;
    return true;
  };

  using namespace matchers;
  auto matcher2DTranspose =
    band(is2DTranspose, leaf());

  auto root = scop.schedule.get_root();
  return walkTree(matcher2DTranspose,root);
}

int detectPattern(Scop scop) {
  
  if(is2DTransposeMatching(scop)) {
    return 1;
  }
  else {
    return 0;
  }
}

bool generate_AP(struct Options &options) {

  using util::ScopedCtx;
  auto ctx = ScopedCtx(pet::allocCtx());
  pet::ScopContainer container;
  container = pet::Scop::parseMultipleScop(ctx, options.inputFile);

  if(container.c.size() == 0) {
    std::cout << "no Scop detetcted.." << std::endl;
    // TODO: copy back the entire file.
  }

  size_t size = container.c.size();
  std::cout << size << std::endl;
  for(size_t i=1; i<size; ++i) {
    int id_pattern = detectPattern(container.c[i]);
    std::cout << "id_pattern " << id_pattern << std::endl;
  }

  return true;
}


