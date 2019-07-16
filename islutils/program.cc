#include <islutils/program.h>

using namespace LoopTactics;

/// Return all the arrays detected in the Scop.
std::vector<pet::PetArray> Program::arrays() const {
  
  return scop_.arrays();
}

/// Return scop schedule.
isl::schedule Program::schedule() const {
  
  return scop_.schedule();
}

/// Return scop reads.
isl::union_map Program::reads() const {

  return scop_.reads();
}

/// Return scop writes.
isl::union_map Program::writes() const{

  return scop_.writes();
}

/// Is the file path valid?
bool Program::check_file_path(const std::string path_to_file) {

  std::ifstream f(path_to_file.c_str());
  return f.good();
}

/// Constructor 
Program::Program(const std::string path_to_file) :
  scop_(check_file_path(path_to_file) ? 
    pet::Scop::parseFile(util::ScopedCtx(pet::allocCtx()), path_to_file) :
    throw Error::Error("file path not found")) {}
