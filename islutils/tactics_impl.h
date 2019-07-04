#include <islutils/tuner.h>

namespace LoopTactics {

using namespace TunerLoopTactics;

template<class T, class...>
struct are_same : std::true_type
{};

template<class T, class U, class... TT>
struct are_same<T, U, TT...>
    : std::integral_constant<bool, std::is_same<T,U>{} && are_same<T, TT...>{}>
{};

template <typename T, typename arg, typename... args>
void do_for(T f, arg first, args... rest) {

  f(first);
  do_for(f, rest...);
}

template <typename T>
void do_for(T f) {}

template<typename T, typename... Args>
void Tactics::tile(T arg, Args... args) {
  
  static_assert(are_same<TileParam, Args...>{}, "must be of type TileParam");

  TileParams tile_parameters;
  tile_parameters.push_back(arg);
 
  do_for([&](auto arg) {
    tile_parameters.push_back(arg);
  }, args...);

  if (tile_parameters.size() == 0)
    throw Error::Error("tile size parameters must be > 1");

  TileConfigurations tcs;
  size_t max = 1;
  for (auto const &p : tile_parameters)
    max *= p.values_.size();

  // generate all the possible permutation.
  for (size_t i = 0; i < max; i++) {  
    auto tmp = i;
    TileConfiguration tc;
    for (auto const &p : tile_parameters) {
      auto index = tmp % p.values_.size();
      tmp /= p.values_.size();
      TileSetting s = TileSetting(p.name_, p.values_[index]);
      tc.push_back(s);
    }
    tcs.push_back(tc);
    tc.clear();
  }

  #ifdef DEBUG
    DUMP_CONFIGS(tcs)
  #endif
  
  TileConfiguration best_configuration; 
  try { 
    Tuner t{tcs,program_.arrays(), path_to_file_};
    best_configuration = t.tune();
  } 
  catch (Error::Error e) {
    std::cout << "cannot build tuner: " << e.message_ << "\n";
    return;
  }

  #ifdef DEBUG
    DUMP_CONFIG(best_configuration);
  #endif

}


} // end namespace
