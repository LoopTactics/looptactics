#ifndef ISLUTILS_DIE_H
#define ISLUTILS_DIE_H

#ifdef ISLUTILS_EXCEPTIONS
#include <stdexcept>
#else
#include <cassert>
#endif

#ifdef ISLUTILS_EXCEPTIONS
#define ISLUTILS_DIE(message) throw std::logic_error(message);
#else
#define ISLUTILS_DIE(message) assert(false && (message));
#endif

#endif
