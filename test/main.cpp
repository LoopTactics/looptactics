#include <boost/program_options.hpp>

using namespace boost;
#include <iostream>
#include <string>
#include <cassert>
#include <fstream>
#include <islutils/builders.h>
#include <islutils/ctx.h>
#include <islutils/pet_wrapper.h>
#include <islutils/matchers.h>

using namespace std;

namespace {
  const size_t ERROR_IN_COMMAND_LINE = 1;
  const size_t SUCCESS = 0;
  const size_t ERROR_UNHANDLED_EXCEPTION = 2;
}

struct Options {
  // name input file 
  std::string inputFile = "empty";
  // name output file
  std::string outputFile;
  // the target we generate code for
  int target = -1;
};

// derive the output file from the "input" file name or use 
// the "output" name. In both case we append "mathcers.cpp" at
// the end on the file.
static ofstream get_output_file(std::string in, std::string out) {

  std::string extension = ".matchers.cpp";
  ofstream of;

  in.replace(in.find('.'), in.length(), extension);

  if(out.empty()) {
    out = in;
  } else {
    out.replace(out.find('.'), in.length(), extension);
  }

  of.open(out);
  return of;
}


static void write_on_file(std::string s, std::ofstream &o) {
  if(o.is_open()) {
    o << s;
  }
} 

static std::string read_file(std::string in) {
  std::stringstream buffer;
  std::ifstream t(in);
  assert(!t && "not able to open the file");
  buffer << t.rdbuf();
  return buffer.str();
}  

// just copy the file. no opt.
static bool generate_cpu(struct Options &options) { 
  auto of = get_output_file(options.inputFile, options.outputFile);
  std::string content = read_file(options.inputFile);
  write_on_file(content, of);
  return true;
}

static inline isl::union_map
filterOutCarriedDependences(isl::union_map dependences,
                            isl::schedule_node node) {
  auto partialSchedule = node.get_prefix_schedule_multi_union_pw_aff();
  return dependences.eq_at(partialSchedule);
}

static bool canMerge(isl::schedule_node parentBand,
                     isl::union_map dependences) {
  // Permutability condition: there are no negative distances along the
  // dimensions that are not carried until now by any of dimensions.
  auto t1 = parentBand.band_get_partial_schedule();
  auto t2 = parentBand.child(0).band_get_partial_schedule();
  auto schedule = isl::union_map::from(t1.flat_range_product(t2));
  auto scheduleSpace = isl::set(schedule.range()).get_space();
  auto positiveOrthant =
      isl::set(isl::basic_set::positive_orthant(scheduleSpace));
  dependences = filterOutCarriedDependences(dependences, parentBand);
  return dependences.apply_domain(schedule)
      .apply_range(schedule)
      .deltas()
      .is_subset(positiveOrthant);
}

static isl::union_map computeAllDependences(const Scop &scop) {
  // For the simplest possible dependence analysis, get rid of reference tags.
  auto reads = scop.reads.domain_factor_domain();
  auto mayWrites = scop.mayWrites.domain_factor_domain();
  auto mustWrites = scop.mustWrites.domain_factor_domain();

  // False dependences (output and anti).
  // Sinks are writes, sources are reads and writes.
  auto falseDepsFlow = isl::union_access_info(mayWrites.unite(mustWrites))
                           .set_may_source(mayWrites.unite(reads))
                           .set_must_source(mustWrites)
                           .set_schedule(scop.schedule)
                           .compute_flow();

  isl::union_map falseDeps = falseDepsFlow.get_may_dependence();

  // Flow dependences.
  // Sinks are reads and sources are writes.
  auto flowDepsFlow = isl::union_access_info(reads)
                          .set_may_source(mayWrites)
                          .set_must_source(mustWrites)
                          .set_schedule(scop.schedule)
                          .compute_flow();

  isl::union_map flowDeps = flowDepsFlow.get_may_dependence();

  return flowDeps.unite(falseDeps);
}

static inline isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
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

isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {
  node = replaceOnce(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderOnce(node.child(i), pattern, replacement).parent();
  }
  return node;
}

isl::schedule_node mergeIfTilable(isl::schedule_node node,
                                  isl::union_map dependences) {
  isl::schedule_node parent, child, grandchild;

  auto canMergeCaptureChild = [&child, dependences](isl::schedule_node node) {
    if (canMerge(node.parent(), dependences)) {
      child = node;
      return true;
    }
    return false;
  };

  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(canMergeCaptureChild,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      auto descr =
          BandDescriptor(parent.band_get_partial_schedule().flat_range_product(
              child.band_get_partial_schedule()));
      descr.permutable = 1;
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  return replaceDFSPreorderRepeatedly(node, matcher, declarativeMerger);
}

static int matchScop(isl::ctx ctx, Scop scop) {

  auto deps = computeAllDependences(scop);
  scop.schedule =
    mergeIfTilable(scop.schedule.get_root(), deps).get_schedule();

  isl::schedule_node root = scop.schedule.get_root();
  std::cout << root.to_str() << std::endl;

  return 0;
}

static bool generate_AP(struct Options &options) {
 
  using util::ScopedCtx; 
  auto ctx = ScopedCtx(pet::allocCtx());
  pet::ScopContainer container;
  container = pet::Scop::parseMultipleScop(ctx, options.inputFile);
  
  if(container.c.size() == 0) {
    std::cout << "no Scop detetcted.." << std::endl;
    // TODO: copy back the entire file.
  }

  size_t size = container.c.size();
  for(size_t i=0; i<size; ++i) {
    matchScop(ctx, container.c[i]);
  }
  
  return true;
}


// transform the file called "input" by replacing each scops by 
// the corresponding optimized builder if available. The result is 
// written in a file called "output". "input" and "output" are
// payloads in the "options" struct. At the moment this is done
// only for the Access Processor.

static bool generate_code(struct Options &options) {

  bool res = false;
  switch(options.target) {
    case 1:
      res = generate_cpu(options);
      break;
    case 2:
      res = generate_AP(options);
      break;
    default:
      assert(0 && "options.target not defined");
  }

  return res;
}


int main(int ac, char* av[]) {

  Options options;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help,h", "print help message")
      ("input,i", po::value<string>(&options.inputFile), "input file name")
      ("output,o", po::value<string>(&options.outputFile), "output file name")
      ("target,t", po::value<int>(&options.target), "target we generate code for");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(ac, av, desc),vm);
      if(vm.count("help")) {
        cout << "command line options" << endl;
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
    } catch(po::error& e) {
      std::cerr << "error: " << e.what() << endl;
      std::cerr << desc << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if(options.target == -1) {
      std::cout << "target not specified assuming CPU" << std::endl;
      options.target = 1;
    }
    if(options.inputFile == "empty") {
      std::cout << "target file not specified.. exit" << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }
    
    bool res = generate_code(options);
    assert(res && "generate_code returned false");

  } catch(std::exception& e) {
    std::cerr << "unhandled excpetion reached main: " << endl;
    std::cerr << e.what() << " exit now " << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }

  #ifdef NDEBUG
    std::cout << options.inputFile << std::endl;
    std::cout << options.outputFile << std::endl;
    std::cout << options.target << std::endl;
  #endif 

  return SUCCESS;
}
