#ifndef RANKCONSENSUS_METHODS_H_
#define RANKCONSENSUS_METHODS_H_

#include <clio_runtime/clio_runtime.h>

namespace coeus::rankConsensus {

/** The set of methods in the rankConsensus task */
struct Method {
  static constexpr clio::run::u32 kCreate = 0;
  static constexpr clio::run::u32 kDestroy = 1;
  static constexpr clio::run::u32 kGetRank = 10;
};

}  // namespace coeus::rankConsensus

#endif  // RANKCONSENSUS_METHODS_H_
