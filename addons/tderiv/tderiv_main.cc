/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * coeus_tderiv — standalone driver for the time-derivative add-on operator. *
 *                                                                           *
 * Usage:                                                                     *
 *   coeus_tderiv --var pp --ranks 4 --steps 100 --dt 0.05 [--online]        *
 *                [--write-back] [--dtype f] [--newest-tag]                  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "TimeDerivativeOperator.h"
#include "coeus_ops/StepBlobReader.h"

using namespace coeus_ops;

static const char *ArgVal(int argc, char **argv, const char *key,
                          const char *def) {
  for (int i = 1; i < argc - 1; ++i)
    if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
  return def;
}
static bool ArgFlag(int argc, char **argv, const char *key) {
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], key) == 0) return true;
  return false;
}

int main(int argc, char **argv) {
  TDerivConfig cfg;
  cfg.var_name = ArgVal(argc, argv, "--var", "pp");
  cfg.dt = std::atof(ArgVal(argc, argv, "--dt", "1.0"));
  cfg.dtype = ArgVal(argc, argv, "--dtype", "f")[0];
  cfg.center_tag = !ArgFlag(argc, argv, "--newest-tag");
  cfg.write_back = ArgFlag(argc, argv, "--write-back");
  const int ranks = std::atoi(ArgVal(argc, argv, "--ranks", "1"));
  const int steps = std::atoi(ArgVal(argc, argv, "--steps", "1"));
  const bool online = ArgFlag(argc, argv, "--online");
  const int timeout_ms = std::atoi(ArgVal(argc, argv, "--timeout-ms", "60000"));

  StepBlobReader reader;
  if (!reader.Connect()) {
    std::cerr << "[tderiv] failed to attach to CTE core pool (runtime up?)\n";
    return 1;
  }

  TimeDerivativeOperator op(cfg);
  for (int step = 1; step <= steps; ++step) {
    for (int r = 0; r < ranks; ++r) {
      bool ready = online
                       ? reader.WaitForStep(step, r, cfg.var_name, timeout_ms)
                       : reader.OpenStep(step, r);
      if (!ready) continue;
      op.OnStep(step, r, reader);
    }
  }
  op.Finalize();
  return 0;
}
