#ifndef COEUS_MDM_METHODS_H_
#define COEUS_MDM_METHODS_H_

#include <chimaera/chimaera.h>

namespace chimaera::coeus_mdm {

/** The set of methods in the coeus_mdm task */
struct Method {
  static constexpr chi::u32 kCreate = 0;
  static constexpr chi::u32 kDestroy = 1;
  static constexpr chi::u32 kMdm_insert = 10;
};

}  // namespace chimaera::coeus_mdm

#endif  // COEUS_MDM_METHODS_H_

