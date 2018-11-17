#include "islutils/access_processor.h"
#include "islutils/common.h"
#include "islutils/access_patterns.h"


int detectPattern(isl::ctx ctx, Scop scop) {
  
  auto dependences = computeAllDependences(scop);
  scop.schedule =
    mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();
  auto root = scop.schedule.get_root();

  auto is2Dtranspose = [&](isl::schedule_node band) {
    using namespace matchers;
    auto prefixSchedule = band.child(0).get_prefix_schedule_union_map();
    auto scheduledReads = scop.reads.curry().apply_domain(prefixSchedule);
    auto scheduledWrites = scop.mustWrites.curry().apply_domain(prefixSchedule);

    if(scheduledReads.n_map() != 1)
      return false;
    if(scheduledWrites.n_map() != 1)
      return false;
    
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

    if(readMatches.size() != 1 || writeMatches.size() != 1)
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
  auto matcher = band(is2Dtranspose, leaf());
 
  if(ScheduleNodeMatcher::isMatching(matcher, root.child(0)))
    return 1;
  else return 0;
  
}

static void printIDs(std::vector<int> &scopID) {
  size_t size = scopID.size();
  for(size_t i=0; i<size; ++i) {
    std::cout << "Scop ID: " << scopID[i] << std::endl;
  }
}

void generate_code(std::vector<int> &scopID, std::vector<pet::Scop> &scops,
                   struct Options &options) {

  assert(scopID.size() == scops.size() && "expect equal size");
}

bool generate_AP(struct Options &options) {
/*
  using util::ScopedCtx;
  auto ctx = ScopedCtx(pet::allocCtx());
  ScopContainer container;
  container = pet::Scop::parseMultipleScop(ctx, options.inputFile);

  if(container.c.size() == 0) {
    std::cout << "no Scop detetcted.." << std::endl;
    // TODO: copy back the entire file.
  }

  std::vector<int> scopID;

  // to each detected scop is assigned an ID (int).
  // to each ID is assigned a function call.
  size_t size = container.c.size();
  for(size_t i=0; i<size; ++i) {
    int res;
    auto pet_scop = std::move(container.c[i]);
    std::cout << pet_scop.codegen() << std::endl;
    res = detectPattern(ctx, pet_scop.getScop());
    scopID.push_back(res);
  }

  printIDs(scopID); 

  generate_code(scopID, container.c, options); 
*/
  return true;
}


