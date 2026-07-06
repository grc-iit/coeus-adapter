#ifndef COEUS_MDM_METHODS_H_
#define COEUS_MDM_METHODS_H_

#include <clio_runtime/clio_runtime.h>

namespace coeus::coeus_mdm {

/** The set of methods in the coeus_mdm task */
struct Method {
  static constexpr clio::run::u32 kCreate = 0;
  static constexpr clio::run::u32 kDestroy = 1;
  static constexpr clio::run::u32 kMdm_insert = 10;
};

}  // namespace coeus::coeus_mdm

#endif  // COEUS_MDM_METHODS_H_
