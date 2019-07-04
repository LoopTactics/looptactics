#ifndef ISLUTILS_PROGRAM_H
#define ISLUTILS_PROGRAM_H

#include <fstream>    // std::ifstream
#include <string>     // std::string
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <islutils/error.h>

namespace LoopTactics {

  enum TypeElement {FLOAT, DOUBLE};

  class PetArray {
    public: 
      isl::set context_;
      isl::set extent_;
      TypeElement type_;
      PetArray() = delete;
      PetArray(isl::set c, isl::set e, TypeElement et)
        : context_(c), extent_(e), type_(et) {};
  };    

  class Program {
    friend class Tactics;
    public:
      Program() = delete;
      Program(const std::string path_to_file);

      // return the schedule of the current scop
      isl::schedule schedule();
      // return the reads of the current scop
      isl::union_map reads();
      // return the writes of the current scop
      isl::union_map writes();
      // return all the arrays detected in the scop
      std::vector<PetArray> arrays();

    private:
      static bool check_file_path(const std::string path_to_file);
      pet::Scop scop_;
  };


} // end namespace loopTactics

#endif
