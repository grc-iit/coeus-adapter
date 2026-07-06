#------------------------------------------------------------------------------
# Build rankConsensus ChiMod (clio-core)
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

# Client library -> target coeus_rankConsensus_client, alias coeus::rankConsensus_client
add_clio_module_client(
  SOURCES
    src/rankConsensus_client.cc
)

# Runtime library -> target coeus_rankConsensus_runtime, alias coeus::rankConsensus_runtime
add_clio_module_runtime(
  SOURCES
    src/rankConsensus_runtime.cc
    src/autogen/rankConsensus_lib_exec.cc
)
