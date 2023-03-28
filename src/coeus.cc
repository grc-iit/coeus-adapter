//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/coeus.h"

MyEngine::MyEngine(adios2::IO &io, const std::string &name, const adios2::Mode mode,
                   MPI_Comm comm)
  : Engine("MyEngine", io, name, mode, comm)
{
}

void MyEngine::Open(const std::string &name, const adios2::Mode mode)
{
  m_FileName = name;
  m_OutputFile.open(name, std::ios_base::out | std::ios_base::binary);
}

void MyEngine::BeginStep()
{
}

void MyEngine::PutSync(const adios2::VariableBase &variable, const void *data)
{
  const std::string variableName(variable.Name());

  m_OutputFile << variableName << ": " << data << std::endl;
}

void MyEngine::EndStep()
{
}

void MyEngine::Close()
{
  m_OutputFile.close();
}

adios2::core::Engine *MyEngineFactory(const std::string &name, adios2::IO &io,
                                      const adios2::Mode mode,
                                      MPI_Comm comm)
{
  return new MyEngine(io, name, mode, comm);
}