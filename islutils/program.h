#ifndef ISLUTILS_PROGRAM_H
#define ISLUTILS_PROGRAM_H

#include <fstream>    // std::ifstream
#include <string>     // std::string
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <islutils/error.h>

namespace LoopTactics {

  class Program {
    friend class Tactics;
    public:
      Program() = delete;
      Program(const std::string path_to_file);

      isl::schedule schedule();
      isl::union_map reads();
      isl::union_map writes();

    private:
      static bool check_file_path(const std::string path_to_file);
      pet::Scop scop_;
  };


} // end namespace loopTactics

#endif
