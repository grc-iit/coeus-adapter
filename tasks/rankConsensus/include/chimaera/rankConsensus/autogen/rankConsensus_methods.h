#ifndef RANKCONSENSUS_METHODS_H_
#define RANKCONSENSUS_METHODS_H_

#include <chimaera/chimaera.h>

namespace chimaera::rankConsensus {

/** The set of methods in the rankConsensus task */
struct Method {
  static constexpr chi::u32 kCreate = 0;
  static constexpr chi::u32 kDestroy = 1;
  static constexpr chi::u32 kGetRank = 10;
};

}  // namespace chimaera::rankConsensus

#endif  // RANKCONSENSUS_METHODS_H_

