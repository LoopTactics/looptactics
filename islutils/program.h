#ifndef ISLUTILS_PROGRAM_H
#define ISLUTILS_PROGRAM_H

#include <fstream>    // std::ifstream
#include <string>     // std::string
#include <cassert>    // lovely asserts
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <islutils/error.h>

namespace LoopTactics {

  class Program {
    friend class Tactics;
    public:
      Program() = delete;
      Program(const std::string path_to_file);

      // return the schedule of the current scop
      isl::schedule schedule() const;
      // return the reads of the current scop
      isl::union_map reads() const;
      // return the writes of the current scop
      isl::union_map writes() const;
      // return all the arrays detected in the scop
      std::vector<pet::PetArray> arrays() const;

    private:
      static bool check_file_path(const std::string path_to_file);
      pet::Scop scop_;
  };


} // end namespace loopTactics

#endif
