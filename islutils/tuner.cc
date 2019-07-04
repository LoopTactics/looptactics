#include <islutils/tuner.h>

using namespace TunerLoopTactics;
using namespace LoopTactics;

// Messages printed to stdout (in colours)
const std::string kMessageFull    = "\x1b[32m[==========]\x1b[0m";
const std::string kMessageHead    = "\x1b[32m[----------]\x1b[0m";
const std::string kMessageRun     = "\x1b[32m[ RUN      ]\x1b[0m";
const std::string kMessageInfo    = "\x1b[32m[   INFO   ]\x1b[0m";
const std::string kMessageVerbose = "\x1b[39m[ VERBOSE  ]\x1b[0m";
const std::string kMessageOK      = "\x1b[32m[       OK ]\x1b[0m";
const std::string kMessageWarning = "\x1b[33m[  WARNING ]\x1b[0m";
const std::string kMessageFailure = "\x1b[31m[   FAILED ]\x1b[0m";
const std::string kMessageResult  = "\x1b[32m[ RESULT   ]\x1b[0m";
const std::string kMessageBest    = "\x1b[35m[     BEST ]\x1b[0m";

bool Tuner::check_configurations(const TileConfigurations cs) {
  bool res = (cs.size() == 0) ? false : true;
  return res;
}

bool Tuner::check_arrays(const std::vector<LoopTactics::PetArray> pa) {
  bool res = (pa.size() == 0) ? false : true;
  return res;
}

Tuner::Tuner(const TileConfigurations cs, 
  const std::vector<LoopTactics::PetArray> pa,
  const std::string path_to_file) :
  cs_(check_configurations(cs) ? cs : throw Error::Error("invalid configurations")),
  arrays_(check_arrays(pa) ? pa : throw Error::Error("invalid arrays")),
  opt_(path_to_file) {}


template <typename Iterator>
TileConfiguration run_jobs(Iterator begin, Iterator end, const TileConfigurations cs) {

  for (auto it = begin; it != end; it++) {
    TileConfiguration c = *it;
    
  }

  return cs[0];
}


TileConfiguration Tuner::tune() {

  auto result = run_jobs(cs_.begin(), cs_.end(), cs_);
  return result;
}
