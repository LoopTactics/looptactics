#ifndef COMMON_H
#define COMMON_H

#include <cassert>
#include <fstream>
#include <islutils/builders.h>
#include <islutils/ctx.h>
#include <islutils/matchers.h>
#include <islutils/pet_wrapper.h>
#include <sstream>
#include <string>
#include <stack>

struct Options {
  // name input file
  std::string inputFile = "empty";
  // name output file
  std::string outputFile;
  // the target we generate code for
  int target = -1;
  // use function calls from optimized libraries
  bool function_call = false;
};

std::ofstream get_output_file(std::string in, std::string out);
void write_on_file(std::string s, std::ofstream &o);
std::string read_from_file(std::string in);

isl::union_map computeAllDependences(const Scop &scop);
isl::union_map filterOutCarriedDependences(isl::union_map dependences,
                                           isl::schedule_node node);
bool canMerge(isl::schedule_node parentBand, isl::union_map dependences);
isl::schedule_node rebuild(isl::schedule_node,
                           const builders::ScheduleNodeBuilder &replacement);
isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement);
isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement);
isl::schedule_node
replaceDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement);
isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement);
isl::schedule_node mergeIfTilable(isl::schedule_node node,
                                  isl::union_map dependences);
std::pair<bool, isl::schedule_node> topmostBand(isl::schedule_node node);

std::string getStringFromTarget(int t);
isl::multi_union_pw_aff getSchedulePointTile(isl::schedule_node node,
                                                    isl::multi_union_pw_aff t);
isl::multi_union_pw_aff getScheduleTile(isl::schedule_node node,
                                               std::vector<int> tileSizes);


#endif
