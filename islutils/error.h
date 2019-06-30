#ifndef ISLUTILS_ERROR_H
#define ISLUTILS_ERROR_H

namespace Error {

class Error {
    public:
      const char *message_;
    public:
      Error(const char *m) {message_ = m; };
  };


} // end namespace error

#endif
