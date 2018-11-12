#include "islutils/cpu.h"
#include "islutils/common.h"

// no opt.
bool generate_cpu(struct Options &options) {
  auto of = get_output_file(options.inputFile, options.outputFile);
  std::string content = read_from_file(options.inputFile);
  write_on_file(content, of);
  return true;
}
