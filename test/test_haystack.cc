#include "gtest/gtest.h"
#include <haystack/HayStack.h>
#include <islutils/ctx.h>
#include <islutils/pet_wrapper.h>
#include <iostream>
#include <islutils/matchers.h>
#include <islutils/builders.h>


using util::ScopedCtx;

const int CACHE_SIZE1 = 32 * 1024;
const int CACHE_SIZE2 = 512 * 1024;
const int CACHE_LINE_SIZE = 64;

TEST(haystack, runCacheModel) {

  machine_model MachineModel = {CACHE_LINE_SIZE, {CACHE_SIZE1, CACHE_SIZE2}};
  model_options ModelOptions = {true};
  auto ctx = ScopedCtx(pet::allocCtx()); 
  HayStack Model(ctx, MachineModel, ModelOptions);
  Model.compileProgram("inputs/haystack_gemm.cc");
  Model.initModel();
  auto CacheMisses = Model.countCacheMisses();
  // collect and print result
  long TotalAccesses = 0;
  long TotalCompulsory = 0;
  std::vector<long> TotalCapacity(MachineModel.CacheSizes.size(), 0);
  // sum the cache misses for all accesses
  for (auto &CacheMiss : CacheMisses) {
    TotalAccesses += CacheMiss.second.Total;
    TotalCompulsory += CacheMiss.second.CompulsoryMisses;
    std::transform(
      TotalCapacity.begin(), TotalCapacity.end(), CacheMiss.second.CapacityMisses.begin(),
      TotalCapacity.begin(), std::plus<long>());
  }
  EXPECT_TRUE(TotalAccesses == 4297064448);
  EXPECT_TRUE(TotalCompulsory == 196608);
  EXPECT_TRUE(TotalCapacity[0] == 67043328);
  EXPECT_TRUE(TotalCapacity[1] == 67043328);
}

TEST(haystack, runCacheModelFromScop) {

  machine_model MachineModel = {CACHE_LINE_SIZE, {CACHE_SIZE1, CACHE_SIZE2}};
  model_options ModelOptions = {true};
  auto ctx = ScopedCtx(pet::allocCtx()); 
  HayStack Model(ctx, MachineModel, ModelOptions); 
  auto petScop = pet::Scop::parseFile(ctx, "inputs/haystack_gemm.cc");
  Model.compileProgram(petScop.get());
  Model.initModel();
  auto CacheMisses = Model.countCacheMisses();
  // collect and print result
  long TotalAccesses = 0;
  long TotalCompulsory = 0;
  std::vector<long> TotalCapacity(MachineModel.CacheSizes.size(), 0);
  // sum the cache misses for all accesses
  for (auto &CacheMiss : CacheMisses) {
    TotalAccesses += CacheMiss.second.Total;
    TotalCompulsory += CacheMiss.second.CompulsoryMisses;
    std::transform(
      TotalCapacity.begin(), TotalCapacity.end(), CacheMiss.second.CapacityMisses.begin(),
      TotalCapacity.begin(), std::plus<long>());
  }
  EXPECT_TRUE(TotalAccesses == 4297064448);
  EXPECT_TRUE(TotalCompulsory == 196608);
  EXPECT_TRUE(TotalCapacity[0] == 67043328);
  EXPECT_TRUE(TotalCapacity[1] == 67043328); 
}

static inline isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

static isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

static isl::schedule_node
replaceDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement) {
  node = replaceRepeatedly(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderRepeatedly(node.child(i), pattern, replacement)
               .parent();
  }
  return node;
}

static isl::multi_union_pw_aff getScheduleTile(isl::schedule_node node,
                                               int tileSize) {

  isl::space space = isl::manage(isl_schedule_node_band_get_space(node.get()));
  unsigned dims = space.dim(isl::dim::set);

  isl::multi_val sizes = isl::multi_val::zero(space);
  for (unsigned i = 0; i < dims; ++i) {
    sizes = sizes.set_val(i, isl::val(node.get_ctx(), tileSize));
  }

  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  for (unsigned i = 0; i < dims; ++i) {

    isl::union_pw_aff upa = sched.get_union_pw_aff(i);
    isl::val v = sizes.get_val(i);
    upa = upa.scale_down_val(v);
    upa = upa.floor();
    sched = sched.set_union_pw_aff(i, upa);
  }
  return sched;
}

static isl::multi_union_pw_aff getSchedulePoint(isl::schedule_node node,
                                                    int tileSize) {
  auto t = getScheduleTile(node, tileSize);
  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  return sched.sub(t);
}

static isl::schedule_node squeezeTree(isl::schedule_node root) {

  isl::schedule_node parent, child, grandchild;
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
      band(child,
        anyTree(grandchild)));
    //clang-format on
  }();

  auto merger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    // clang-format off
    auto computeSched = [&]() {
      isl::multi_union_pw_aff sched =
        parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
      return sched;
    };
    // clang-format on
    auto st = [&]() { return subtreeBuilder(grandchild); };
    merger = band(computeSched, subtree(st));
  }

  root = replaceDFSPreorderRepeatedly(root, matcher, merger);
  return root.root();
}

static isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

static isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {
  node = replaceOnce(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderOnce(node.child(i), pattern, replacement).parent();
  }
  return node;
}

struct ScopStats {
  long Total = 0;
  long Compulsory = 0;
  std::vector<long>Capacity = {};
};

static ScopStats runHayStackModel(pet::Scop &petScop, isl::ctx ctx) {

  machine_model MachineModel = {CACHE_LINE_SIZE, {CACHE_SIZE1, CACHE_SIZE2}};
  model_options ModelOptions = {false}; 
  HayStack Model(ctx, MachineModel, ModelOptions); 
  Model.compileProgram(petScop.get());
  Model.initModel();
  auto CacheMisses = Model.countCacheMisses();
  // collect and print result
  long TotalAccesses = 0;
  long TotalCompulsory = 0;
  std::vector<long> TotalCapacity(MachineModel.CacheSizes.size(), 0);
  for (auto &CacheMiss : CacheMisses) {
    TotalAccesses += CacheMiss.second.Total;
    TotalCompulsory += CacheMiss.second.CompulsoryMisses;
    std::transform(
      TotalCapacity.begin(), TotalCapacity.end(), CacheMiss.second.CapacityMisses.begin(),
      TotalCapacity.begin(), std::plus<long>());
  }

  //std::cout << "total: " << TotalAccesses << "\n";
  //std::cout << "compulsory: " << TotalCompulsory << "\n";
  //std::cout << "L1: " << TotalCapacity[0] << "\n";
  //std::cout << "L2: " << TotalCapacity[1] << "\n";
  return ScopStats{TotalAccesses, TotalCompulsory, {TotalCapacity[0], TotalCapacity[1]}};
}


TEST(haystack, Tiling) {

  auto ctx = ScopedCtx(pet::allocCtx());
  pet::Scop petScop = pet::Scop::parseFile(ctx, "inputs/gemm.c");
  auto scop = petScop.getScop();
  
  isl::schedule_node root = scop.schedule.get_root();
  root = squeezeTree(root);

  isl::schedule_node bandNode, continuation;
  auto matcher = [&]() {
    using namespace matchers;
    return band(bandNode, anyTree(continuation));
  }();

  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders; 
    auto markerPoint = [&]() {
      return isl::id::alloc(bandNode.get_ctx(), "point_loop", nullptr);
    };
    auto markerTile = [&]() {
      return isl::id::alloc(bandNode.get_ctx(), "tile_loop", nullptr);
    };
    auto st = [&]() { 
      return subtreeBuilder(continuation); 
    };
    auto scheduleTile = [&]() {
      return getScheduleTile(bandNode, 32); 
    }; 
    auto schedulePoint = [&]() {
      return getSchedulePoint(bandNode, 32);
    };
    builder = mark(markerTile, 
                band(scheduleTile, 
                  mark(markerPoint, 
                    band(schedulePoint, subtree(st)))));
  }

  ASSERT_TRUE(
    matchers::ScheduleNodeMatcher::isMatching(matcher, root.child(0)));

  root = root.child(0);
  root = root.cut();
  root = builder.insertAt(root);
  root = root.root();

  root = 
    root.child(0).child(0).child(0).child(0).child(0).child(1).child(0);
  ASSERT_TRUE(
    matchers::ScheduleNodeMatcher::isMatching(matcher, root));
  
  root = root.cut();
  root = builder.insertAt(root);
  root = root.root();
 
  auto res1 = runHayStackModel(petScop, ctx);
  petScop.schedule() = root.get_schedule();
  auto res2 = runHayStackModel(petScop, ctx);
  EXPECT_TRUE(res1.Total == res2.Total);
}
