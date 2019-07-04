#ifndef ISLUTILS_TUNER_H
#define ISLUTILS_TUNER_H

#include <string>         // std::string
#include <vector>         // std::vector
#include <islutils/program.h>
#include <islutils/error.h>
#include <islutils/loop_opt.h>

namespace TunerLoopTactics {

   // A tile parameter represents: {"i", {32, 64, 128"}}
  class TileParam {
    public:
      std::string name_;
      std::vector<int> values_;
      TileParam(std::string s, std::vector<int> v) : name_(s), values_(v) {};
  };
  using TileParams = std::vector<TileParam>;

  // A tile setting represents: {"i", {32}}
  class TileSetting {
    public:
      std::string name_;
      int value_;
      TileSetting() = delete;
      TileSetting(std::string s, int v) : name_(s), value_(v) {};
  };
  using TileConfiguration = std::vector<TileSetting>;
  using TileConfigurations = std::vector<TileConfiguration>;

  enum class Search_method {FULL_SEARCH};
  
  class Tuner {
    private:
      static bool check_configurations(const TileConfigurations);
      static bool check_arrays(const std::vector<LoopTactics::PetArray> pa);

    TileConfigurations cs_;
    std::vector<LoopTactics::PetArray> arrays_;
    LoopTactics::LoopOptimizer opt_;
    public:
      Tuner() = delete;
      Tuner(TileConfigurations, std::vector<LoopTactics::PetArray>, std::string path_to_file);
      TileConfiguration tune();
  };

  #define DUMP_CONFIG(configuration)                        \
  do {                                                      \
    std::cout << " { ";                                     \
    for (size_t i = 0; i < configuration.size(); i++) {     \
      std::cout << "[ ";                                    \
      std::cout << configuration[i].name_ << ", ";          \
      std::cout << configuration[i].value_ << " ]";         \
    }                                                       \
    std::cout << " }";                                      \
  } while(0);                                               

  #define DUMP_CONFIGS(configurations)                      \
  do {                                                      \
    for(size_t j = 0; j < configurations.size(); j++) {     \
      DUMP_CONFIG(configurations[j])                        \
      std::cout << "\n";                                    \
    }                                                       \
    std::cout << "#Configuration :" <<                      \
      configurations.size() << "\n";                        \
  } while(0);

} // end namesapce tunerLoopTactics


#endif
