#ifndef ISLUTILS_TACTICS_H
#define ISLUTILS_TACTICS_H

#include <string>       // std::string
#include <iostream>     // std::cout 
#include <set>          // std::set
#include <algorithm>    // std::remove_if
#include <locale>       // std::isspace
#include <regex>        // std::regex
#include <cassert>
#include <islutils/error.h>
#include <islutils/parser.h>
#include <islutils/pet_wrapper.h>
#include <islutils/ctx.h>
#include <islutils/access_patterns.h>
#include <islutils/access.h>
#include <islutils/matchers.h>
#include <islutils/builders.h>
#include <islutils/program.h>

namespace LoopTactics {

  class Tactics {
    private:
      
      // utility class 
      Program program_;  
      // name of the tactics
      const std::string tactics_id_;
      // accesses decriptors obtained from parser.
      std::vector<Parser::AccessDescriptor> accesses_descriptors_; 
      // schedule 
      isl::schedule current_schedule_;

    public:
      Tactics(std::string id, 
              std::string pattern, std::string path_to_file);
      void show();
      void match();
      void tile(std::string loop_id, int tile_size);
      void interchange(std::string loop_source, std::string loop_destination);
  };

} // end namespace tactics

#endif
