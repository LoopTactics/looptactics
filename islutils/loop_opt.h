#ifndef ISLUTILS_LOOP_OPT_H
#define ISLUTILS_LOOP_OPT_H

#include <isl/cpp.h>
#include <stack>  // std::stack
#include <string> // std::string

namespace LoopTactics {

class LoopOptimizer {

public:
  isl::schedule tile(isl::schedule schedule, const std::string &loop_id,
                     const int &tile_size);
  isl::schedule swap_loop(isl::schedule schedule,
                          const std::string &loop_source,
                          const std::string &loop_dest);
  isl::schedule unroll_loop(isl::schedule schedule, const std::string &loop_id,
                            const int &unroll_factor);
  isl::schedule loop_reverse(isl::schedule schedule,
                             const std::string &loop_id);
};

} // namespace LoopTactics

#endif
