#ifndef ISLUTILS_LOOP_OPT_H
#define ISLUTILS_LOOP_OPT_H

#include <string>       // std::string
#include <fstream>    // std::ifstream
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <islutils/matchers.h>
#include <islutils/builders.h>
#include <islutils/error.h>

namespace LoopTactics {

  class LoopOptimizer {

    public:
      isl::schedule tile(const std::string loop_id, 
                        const int tile_size, isl::schedule schedule);
      std::string code_gen(isl::schedule schedule);
      LoopOptimizer() = delete;
      LoopOptimizer(const std::string path_to_file);

    private:
      static bool check_file_path(const std::string path_to_file);
      pet::Scop scop_;
  };


} // end namespace loop tactics




#endif 
