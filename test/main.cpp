#include <boost/program_options.hpp>

using namespace boost;
#include <iostream>
#include <islutils/common.h>
#include <islutils/cpu.h>
#include <islutils/access_processor.h>

using namespace std;

namespace {
  const size_t ERROR_IN_COMMAND_LINE = 1;
  const size_t SUCCESS = 0;
  const size_t ERROR_UNHANDLED_EXCEPTION = 2;
}

// transform the file called "input" by replacing each scops by 
// the corresponding optimized builder if available. The result is 
// written in a file called "output". "input" and "output" are
// payloads in the "options" struct. At the moment this is done
// only for the Access Processor.

static bool generate_code(struct Options &options) {

  bool res = false;
  switch(options.target) {
    case 1:
      res = generate_cpu(options);
      break;
    case 2:
      res = generate_AP(options);
      break;
    default:
      assert(0 && "options.target not defined");
  }

  return res;
}


int main(int ac, char* av[]) {

  Options options;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help,h", "print help message")
      ("input,i", po::value<string>(&options.inputFile), "input file name")
      ("output,o", po::value<string>(&options.outputFile), "output file name")
      ("target,t", po::value<int>(&options.target), "target we generate code for");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(ac, av, desc),vm);
      if(vm.count("help")) {
        cout << "command line options" << endl;
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
    } catch(po::error& e) {
      std::cerr << "error: " << e.what() << endl;
      std::cerr << desc << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if(options.target == -1) {
      std::cout << "target not specified assuming CPU" << std::endl;
      options.target = 1;
    }
    if(options.inputFile == "empty") {
      std::cout << "target file not specified.. exit" << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }
    
    bool res = generate_code(options);
    assert(res && "generate_code returned false");

  } catch(std::exception& e) {
    std::cerr << "unhandled excpetion reached main: " << endl;
    std::cerr << e.what() << " exit now " << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }

  #ifdef NDEBUG
    std::cout << options.inputFile << std::endl;
    std::cout << options.outputFile << std::endl;
    std::cout << options.target << std::endl;
  #endif 

  return SUCCESS;
}
