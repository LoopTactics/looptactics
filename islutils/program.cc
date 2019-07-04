#include <islutils/program.h>

using namespace LoopTactics;

/// Return all the arrays detected in the Scop.
std::vector<PetArray> Program::arrays() {

  std::vector<PetArray> res;
  auto scop = scop_.getScop();
  for (int i = 0; i < scop.n_array; i++) {
    auto a = scop.arrays[i];
    TypeElement t = 
      (a.element_type.compare("float") == 0) ? TypeElement::FLOAT : TypeElement::DOUBLE;
    PetArray pa{a.context, a.extent,t};
    res.push_back(pa);
  } 
  return res;
}

/// Return scop schedule.
isl::schedule Program::schedule() {
  
  auto scop = scop_.getScop();
  return scop.schedule;
}

/// Return scop reads.
isl::union_map Program::reads() {

  auto scop = scop_.getScop();
  return scop.reads.curry();
}

/// Return scop writes.
isl::union_map Program::writes() {

  auto scop = scop_.getScop();
  return scop.mustWrites.curry();
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
