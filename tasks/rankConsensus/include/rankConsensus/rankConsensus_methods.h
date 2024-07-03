#ifndef HRUN_RANKCONSENSUS_METHODS_H_
#define HRUN_RANKCONSENSUS_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kGetRank = kLast + 0;
};

#endif  // HRUN_RANKCONSENSUS_METHODS_H_