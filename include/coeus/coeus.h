//
// Created by lukemartinlogan on 3/28/23.
//

#ifndef COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
#define COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_

#include <adios2.h>

class MyEngine : public adios2::core::Engine
{
 public:
  MyEngine(adios2::IO &io, const std::string &name, const adios2::Mode mode,
           MPI_Comm comm);

  ~MyEngine() = default;

  void Open(const std::string &name, const adios2::Mode mode) override;

  void BeginStep() override;

  void PutSync(const adios2::VariableBase &variable, const void *data) override;

  void EndStep() override;

  void Close() override;

 private:
  std::string m_FileName;
  std::ofstream m_OutputFile;
};

#endif //COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
