#ifndef HRUN_COEUS_MDM_METHODS_H_
#define HRUN_COEUS_MDM_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kMdm_insert = kLast + 0;
  TASK_METHOD_T kPut_hash = kLast + 1;
};

#endif  // HRUN_COEUS_MDM_METHODS_H_