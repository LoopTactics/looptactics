#include <islutils/locus.h>

int main() {
  isl::aff a1;
  isl::aff a2;
  using map_maker::operator==;
  auto x = (a1 == a2);
}
