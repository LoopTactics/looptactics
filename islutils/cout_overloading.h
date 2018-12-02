#ifndef ISLUTILS_COUT_H
#define ISLUTILS_COUT_H 

#include <isl/cpp.h>

//
// overloading << for printing isl::is
inline auto& operator<<(std::ostream &OS, isl::id i) { 
  OS << i.to_str() << "\n";
  return OS;
}

//
// overloading << for printing isl::space
inline auto& operator<<(std::ostream &OS, isl::space s) {
  OS << s.to_str() << "\n";
  return OS;
}

//
// overloading of << to print isl::union_map
inline auto& operator<<(std::ostream &OS, isl::union_map m) {
  return OS << m.to_str() << "\n";
}

//
// overloading of << to print isl::pw_aff
inline auto& operator<<(std::ostream &OS, isl::pw_aff a) {
  return OS << a.to_str() << "\n";
}

//
//overloading of << to print isl::schedule
inline auto& operator<<(std::ostream &OS, isl::schedule schedule) {
  return OS << schedule.to_str() << "\n";
}

//
// overloading of << to print isl::schedule_node
inline auto& operator<<(std::ostream &OS, isl::schedule_node node) {
  return OS << node.to_str() << "\n";
}

//
// overloading of << to print isl::union_set
inline auto& operator<<(std::ostream &OS, isl::union_set uset) {
  return OS << uset.to_str() << "\n";
}

#endif // ISLUTILS_COUT_H
