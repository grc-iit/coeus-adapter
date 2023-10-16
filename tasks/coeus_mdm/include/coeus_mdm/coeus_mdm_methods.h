#ifndef LABSTOR_COEUS_MDM_METHODS_H_
#define LABSTOR_COEUS_MDM_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kSQLInsert = kLast + 0;
};

#endif  // LABSTOR_COEUS_MDM_METHODS_H_