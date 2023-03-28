//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/coeus.h"

int main()
{
  adios2::ADIOS adios(MPI_COMM_WORLD);
  adios.RegisterEngine("MyEngine", MyEngineFactory);

  adios2::IO io = adios.DeclareIO("myIO");
  io.SetEngine("MyEngine");

  adios2::Variable<int> var = io.DefineVariable<int>("var");
  adios2::Engine engine = io.Open("output.bp", adios2::Mode::Write);

  engine.BeginStep();
  engine.PutSync(var, 42);
  engine.EndStep();

  engine.Close();

  return 0;
}
