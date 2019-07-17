#ifndef ISLUTILS_TACTICS_H
#define ISLUTILS_TACTICS_H

#include <string>       // std::string
#include <iostream>     // std::cout 
#include <set>          // std::set
#include <algorithm>    // std::remove_if
#include <locale>       // std::isspace
#include <regex>        // std::regex
#include <type_traits>  // std::is_same
#include <cassert>
#include <islutils/error.h>
#include <islutils/parser.h>
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <islutils/access_patterns.h>
#include <islutils/access.h>
#include <islutils/tuner.h>
#include <islutils/loop_opt.h>

namespace LoopTactics {

  class Tactics {
    
    private:  
      // scop
      pet::Scop scop_;
      // optimizer 
      LoopOptimizer opt_;
      // tuner
      TunerLoopTactics::Tuner tuner_;
      // name of the tactic
      const std::string tactics_id_;
      // accesses decriptors obtained from parser
      std::vector<Parser::AccessDescriptor> accesses_descriptors_; 
      // schedule 
      isl::schedule current_schedule_;

    public:
      Tactics(isl::ctx ctx, std::string id, 
              std::string pattern, std::string path_to_file);
      void show();
      void match();

      void tile(std::string loop_id, int tile_size);
      template<typename T, typename... Args>
      void tile(T arg, Args... args);
      void interchange(std::string loop_source, std::string loop_destination);
  };

} // end namespace tactics

#include <islutils/tactics_impl.h>

#endif
