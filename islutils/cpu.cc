#include "islutils/cpu.h"
#include "islutils/common.h"
#include <glog/logging.h>
#include "islutils/access_patterns.h"
/*
isl::schedule function_call_optimization_CPU(isl::ctx ctx, Scop scop) {

  auto dependences = computeAllDependences(scop);
  scop.schedule =
    mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();
  isl::schedule_node root = scop.schedule.get_root();

  isl::schedule_node node;

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

    node = band;
    return true;

  };

  // check if matching with transpose operation.
  using namespace matchers;
  auto matcher = band(is2Dtranspose, leaf());
  if(ScheduleNodeMatcher::isMatching(matcher, root.child(0))) {
    LOG(INFO) << "matched with 2D transpose";
  }

  int singleStatement = node.get_domain().n_set();
  std::cout << singleStatement << std::endl;
  
  //std::string group_id_tag = "transpose";
  //isl::id group_id = 
  //  isl::manage(isl_id_alloc(ctx.get(), group_id_tag.c_str(), nullptr));
  //node = node.group(group_id);
  //return node.get_schedule();
    
  return scop.schedule;
}
*/

// check tiling profitability.
 
static bool isTileable(isl::schedule_node node) {
  auto space = 
    isl::manage(isl_schedule_node_band_get_space(node.get()));
  auto dims = space.dim(isl::dim::set);
  if(dims <= 1) {
    return false;
  }
  return true;
}

// apply tiling.

isl::schedule_node applyTiling(isl::schedule_node node) {

  isl::schedule_node capturedSubtree, capturedNode;

  auto matcher = matchers::band(
    [&capturedNode](isl::schedule_node n) {
      if(isTileable(n)) {
        capturedNode = n;
        return true;
      } else return false;
    },
    matchers::anyTree(capturedSubtree));

  // TODO: check if we can avoid to repeat
  // code.
  builders::ScheduleNodeBuilder builder = 
    builders::band(
      [&capturedNode] {
        int dimOutNum = isl_schedule_node_band_n_member(capturedNode.get());
        std::vector<int> tileSizes(dimOutNum);
        for(int i=0; i<dimOutNum; ++i) {
          tileSizes[i] = 32;
        }
        auto tileSchedule = getScheduleTile(capturedNode, tileSizes);
        return tileSchedule;
      },
      builders::band(
        [&capturedNode] {
          int dimOutNum = isl_schedule_node_band_n_member(capturedNode.get());
          std::vector<int> tileSizes(dimOutNum);
          for(int i=0; i<dimOutNum; ++i) {
            tileSizes[i] = 32;
          }
          auto tileSchedule = getScheduleTile(capturedNode, tileSizes);
          auto pointSchedule = getSchedulePointTile(capturedNode, tileSchedule);
          return pointSchedule;
        },
        builders::subtree(capturedSubtree)));


  if(matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    std::cout << "possible tiling" << "\n";
  }

  return node;      
}

// Optimize for locality. Same optimization as Polly does.
// (more or less)

isl::schedule standard_optimization_CPU(isl::ctx ctx, Scop scop) {

  LOG(INFO) << "standard optimization for locality\n";
  isl::schedule_node root = scop.schedule.get_root(); 
 
  // apply tiling transformation.
  isl::schedule_node node = applyTiling(root.child(0));

  return node.root().get_schedule();  
}

// Optimize the schedule. Optimize the current scop
// for locality.

isl::schedule optimize_CPU(isl::ctx ctx, Scop scop) {

  LOG(INFO) << "optimize for CPUs\n";
  isl::schedule optimizedSchedule;
  optimizedSchedule = standard_optimization_CPU(ctx, scop);
  return optimizedSchedule;
}

// Given the optimized kernels in "optimizedScops" the function
// swaps the code in between #pragma scop /* something */ #pragma endscop
// with the optimized versions. The output and input file name are
// controlled with "options".

void generate_code_CPU(std::vector<std::string> &optimizedScops,
                   struct Options &options) {
 
  auto of = get_output_file(options.inputFile, options.outputFile);
  std::string content = read_from_file(options.inputFile);

  std::string begin = "#pragma scop";
  std::string end = "#pragma endscop";

  for(size_t i=0; i<optimizedScops.size(); ++i) {
    std::size_t found_b = content.find(begin);
    std::size_t found_e = content.find(end);
    content.replace(found_b, found_e - found_b + end.size(), 
                    optimizedScops[i]);
  }

  LOG(INFO) << "Written content :" << content;
  write_on_file(content, of);
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

    // call to codegen.
    pet_scop.schedule() = schedule;
    std::string optimizedScop;
    if(!options.function_call) {
      optimizedScop = pet_scop.codegen();
    }
    else {
      //optimizedScop = pet_scop.codegen(codegenLibrary);
    }
    
    // save output of codegen.    
    optimizedScops.push_back(optimizedScop);
  }

  assert(optimizedScops.size() == container.c.size()
         && "number of optimized scops differs from number of extracted scops");

  for(size_t i=0; i<optimizedScops.size(); ++i) {
    LOG(INFO) << "************KERNEL************";
    LOG(INFO) << optimizedScops[i];
    LOG(INFO) << "************ENDKERNEL************";
  }

  generate_code_CPU(optimizedScops, options); 
  
  return true;
}
