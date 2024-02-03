#ifndef HRUN_COEUS_MDM_METHODS_H_
#define HRUN_COEUS_MDM_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kMdm_insert = kLast + 0;
};

#endif  // HRUN_COEUS_MDM_METHODS_H_