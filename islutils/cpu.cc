#include "islutils/cpu.h"
#include "islutils/common.h"
#include <glog/logging.h>
#include "islutils/access_patterns.h"


isl::schedule optimize_CPU(isl::ctx ctx, Scop scop) {

  isl::schedule_node root = scop.schedule.get_root();
  std::cout << root.to_str() << std::endl;
  isl::union_map deps = computeAllDependences(scop);
  
  isl::schedule_node node = 
    mergeIfTilable(root, deps);
  std::cout << "node" << node.to_str() << std::endl;
  return root.get_schedule();
}


// Transform the code in the file called "inputFile" member of
// "options" by replacing all scops by corresponding cpu
// code and write the results to a file called "outputFile"
// again member of "options".

bool generate_CPU(struct Options &options) {

  // extract multiple scops from file.
  using util::ScopedCtx;
  auto ctx = ScopedCtx(pet::allocCtx());
  ScopContainer container;
  container = pet::Scop::parseMultipleScop(ctx, options.inputFile);

  // early exit if no scops are detected.
  if(container.c.size() == 0) {
    LOG(INFO) << getStringFromTarget(options.target);
    LOG(INFO) << "no scops detected.. ";
    // TODO: copy back entire file with no modifications.
    return false;
  }

  // iterate over all the scops.
  std::vector<std::string> optimizedScops;
  size_t size = container.c.size();
  LOG(INFO) << "scops detetected " << size << "\n";

  for(size_t i=0; i<size; ++i) {
    pet::Scop pet_scop = pet::Scop(container.c[i]);
    isl::ctx ctx = pet_scop.getCtx();
   
    // optimize code. 
    isl::schedule schedule =
      optimize_CPU(ctx, pet_scop.getScop());

    LOG(INFO) << "Optimized Schedule for CPU: " << schedule.to_str();
  }

  assert(optimizedScops.size() == container.c.size()
         && "number of optimized scops differs from number of extracted scops");
  return true;
}
