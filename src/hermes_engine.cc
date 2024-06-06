/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "coeus/HermesEngine.h"

namespace coeus {
/**
 * Construct the HermesEngine.
 *
 * This is a wrapper around Init_, which performs the main custom
 * initialization of the engine. The PluginEngineInterface will store
 * the "io" variable in the "m_IO" variable.
 * */


HermesEngine::HermesEngine(adios2::core::IO &io,//NOLINT
                           const std::string &name,
                           const adios2::Mode mode,
                           adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Hermes = std::make_shared<coeus::Hermes>();
//  mpiComm = std::make_shared<coeus::MPI>(comm.Duplicate());
  Init_();
  TRACE_FUNC("Init HermesEngine");
  engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));

}

/**
 * Test initializer
 * */
HermesEngine::HermesEngine(std::shared_ptr<coeus::IHermes> h,
                           std::shared_ptr<coeus::MPI> mpi,
                           adios2::core::IO &io,
                           const std::string &name,
                           const adios2::Mode mode,
                           adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Hermes = h;
  Init_();
  TRACE_FUNC("hermes engine construction");
  engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));
}

/**
* Initialize the engine.
* */
void HermesEngine::Init_() {
  // initiate the trace manager
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::trace);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  //File log
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/engine_test.txt", true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  // File log for metadata collection
  #ifdef Meta_enabled
  auto file_sink2 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/metadataCollect_get.txt", true);
  file_sink2->set_level(spdlog::level::trace);
  file_sink2->set_pattern("%v");
  spdlog::logger logger2("metadata_logger_get", {file_sink2});
  logger2.set_level(spdlog::level::trace);
  meta_logger_get = std::make_shared<spdlog::logger>(logger2);
  meta_logger_get->info(
      "\nName, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable\n ShapeID, steps, stepstart, blockID, blob_name, bucket_name, processor, process");

  auto file_sink3 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/metadataCollect_put.txt", true);
  file_sink3->set_level(spdlog::level::trace);
  file_sink3->set_pattern("%v");
  spdlog::logger logger3("metadata_logger_put", {file_sink3});
  logger3.set_level(spdlog::level::trace);
  meta_logger_put = std::make_shared<spdlog::logger>(logger3);
  meta_logger_put->info(
      "\nName, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable, \nShapeID, steps, stepstart, blockID, blob_name, bucket_name, processor, process");
#endif

  //Merge Log
  spdlog::logger logger("debug_logger", {console_sink, file_sink});
  logger.set_level(spdlog::level::debug);
  engine_logger = std::make_shared<spdlog::logger>(logger);

  // hermes setup
  if (!Hermes->connect()) {
    engine_logger->warn("Could not connect to Hermes", rank);
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  if (rank == 0) std::cout << "Connected to Hermes" << std::endl;



  // add rank with consensus
  rank_consensus.CreateRoot(DomainId::GetLocal(), "rankConsensus");
  rank = rank_consensus.GetRankRoot(DomainId::GetLocal());
  const size_t bufferSize = 1024;  // Define the buffer size
  char buffer[bufferSize];         // Create a buffer to hold the hostname
  // Get the hostname


  comm_size = m_Comm.Size();
  pid_t processId = getpid();

  //Identifier, should be the file, but we don't get it
  uid = this->m_IO.m_Name;

  //Configuration Setup through the Adios xml configuration
  auto params = m_IO.m_Parameters;
  if (params.find("OPFile") != params.end()) {
    std::string opFile = params["OPFile"];
    if (rank == 0)std::cout << "OPFile: " << opFile << std::endl;
    try {
      operationMap = YAMLParser(opFile).parse();
    }
    catch (std::exception &e) {
      engine_logger->warn("Could not parse operation file", rank);
      throw e;
    }
  }
  if (params.find("ppn") != params.end()) {
    ppn = stoi(params["ppn"]);
    if (rank == 0) std::cout << "PPN: " << ppn << std::endl;
  }
  if (params.find("VarFile") != params.end()) {
    std::string varFile = params["VarFile"];
    if (rank == 0)std::cout << "varFile: " << varFile << std::endl;
    try {
      variableMap = YAMLParser(varFile).parse();
    }
    catch (std::exception &e) {
      engine_logger->warn("Could not parse variable file", rank);
      throw e;
    }
  }
  //Hermes setup

  if (params.find("db_file") != params.end()) {
    db_file = params["db_file"];
    db = new SQLiteWrapper(db_file);
    client.CreateRoot(DomainId::GetGlobal(), "db_operation", db_file);
    if (rank % ppn == 0) {
      db->createTables();
    }
  } else {
    throw std::invalid_argument("db_file not found in parameters");
  }
/*
  if(params.find("adiosOutput") != params.end()) {
      adiosOutput = params["adiosOutput"];
  }

*/
        adiosOutput = "/mnt/ssd/hxu40/copy.bp";



}

/**
 * Close the Engine.
 * */
void HermesEngine::DoClose(const int transportIndex) {
  TRACE_FUNC("engine close");
  open = false;

}

HermesEngine::~HermesEngine() {
  TRACE_FUNC();
  delete db;

}

/**
 * Handle step operations.
 * */

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {

  TRACE_FUNC(std::to_string(currentStep));

  IncrementCurrentStep();

  if (m_OpenMode == adios2::Mode::Read) {
    if (total_steps == -1) total_steps = db->GetTotalSteps(uid);

    if (currentStep > total_steps) {
      return adios2::StepStatus::EndOfStream;
    }
    LoadMetadata();
  }
  std::string bucket_name = "step_" + std::to_string(currentStep)
      + "_rank" + std::to_string(rank);
  Hermes->GetBucket(bucket_name);

  return adios2::StepStatus::OK;
}

void HermesEngine::IncrementCurrentStep() {
  TRACE_FUNC(std::to_string(currentStep));
  currentStep++;
}

size_t HermesEngine::CurrentStep() const {
  TRACE_FUNC(std::to_string(currentStep));
  return currentStep;
}

void HermesEngine::EndStep() {
  TRACE_FUNC(std::to_string(currentStep));
  if (m_OpenMode == adios2::Mode::Write) {
    if (rank % ppn == 0) {
      DbOperation db_op(uid, currentStep);
      client.Mdm_insertRoot(DomainId::GetLocal(), db_op);
    }
  }

  delete Hermes->bkt;

}

/**
 * Metadata operations.
 * */
bool HermesEngine::VariableMinMax(const adios2::core::VariableBase &Var,
                                  const size_t Step,
                                  adios2::MinMaxStruct &MinMax) {
  TRACE_FUNC(Var.m_Name);
  // We initialize the min and max values
  MinMax.Init(Var.m_Type);

  // Obtain the blob from Hermes using the filename and variable name
  hermes::Blob blob = Hermes->bkt->Get(Var.m_Name);

#define DEFINE_VARIABLE(T) \
      if (adios2::helper::GetDataType<T>()  ==  Var.m_Type) { \
          size_t dataSize = blob.size() / sizeof(T);                               \
          const T *data = reinterpret_cast<const T *>(blob.data());               \
          for (size_t i = 0; i < dataSize; ++i) {               \
                  void *elementPtr = const_cast<void *>(static_cast<const void *>(&data[i]));     \
                  ApplyElementMinMax(MinMax, Var.m_Type, elementPtr);                   \
              }              \
      }
  ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
  return true;
}

void HermesEngine::ApplyElementMinMax(adios2::MinMaxStruct &MinMax,
                                      adios2::DataType Type, void *Element) {

  switch (Type) {
    case adios2::DataType::Int8:ElementMinMax<int8_t>(MinMax, Element);
      break;
    case adios2::DataType::Int16:ElementMinMax<int16_t>(MinMax, Element);
      break;
    case adios2::DataType::Int32:ElementMinMax<int32_t>(MinMax, Element);
      break;
    case adios2::DataType::Int64:ElementMinMax<int64_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt8:ElementMinMax<uint8_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt16:ElementMinMax<uint16_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt32:ElementMinMax<uint32_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt64:ElementMinMax<uint64_t>(MinMax, Element);
      break;
    case adios2::DataType::Float:ElementMinMax<float>(MinMax, Element);
      break;
    case adios2::DataType::Double:ElementMinMax<double>(MinMax, Element);
      break;
    case adios2::DataType::LongDouble:ElementMinMax<long double>(MinMax, Element);
      break;
    default:
      /*
       *     case adios2::DataType::None
       *     adios2::DataType::Char
       *     case adios2::DataType::FloatComplex
       *     case adios2::DataType::DoubleComplex
       *     case adios2::DataType::String
       *     case adios2::DataType::Struct
       */
      break;
  }
}

template<typename T>
T *HermesEngine::SelectUnion(adios2::PrimitiveStdtypeUnion &u) {
  TRACE_FUNC();
  return reinterpret_cast<T *>(&u);


}

template<typename T>
void HermesEngine::ElementMinMax(adios2::MinMaxStruct &MinMax, void *element) {
  TRACE_FUNC("MinMax operation");
  T *min = SelectUnion<T>(MinMax.MinUnion);
  T *max = SelectUnion<T>(MinMax.MaxUnion);
  T *value = static_cast<T *>(element);
  if (*value < *min) {
    min = value;
  }
  if (*value > *max) {
    max = value;
  }
}

void HermesEngine::LoadMetadata() {
  TRACE_FUNC(rank);
  auto metadata_vector = db->GetAllVariableMetadata(currentStep, rank);
  for (auto &variableMetadata : metadata_vector) {
    DefineVariable(variableMetadata);
  }


}

void HermesEngine::DefineVariable(const VariableMetadata &variableMetadata) {
  if (currentStep != 1) {
    // If the metadata is defined delete current value to update it
    m_IO.RemoveVariable(variableMetadata.name);
  }

#define DEFINE_VARIABLE(T) \
    if (adios2::helper::GetDataType<T>() == adios2::helper::GetDataTypeFromString(variableMetadata.dataType)) { \
         adios2::core::Variable<T> *variable = &(m_IO.DefineVariable<T>( \
                      variableMetadata.name, \
                      variableMetadata.shape, \
                      variableMetadata.start, \
                      variableMetadata.count, \
                      variableMetadata.constantShape));                         \
         variable->m_AvailableStepsCount = 1;                                   \
         variable->m_SingleValue = false;                                       \
         variable->m_Min = std::numeric_limits<T>::max();                       \
         variable->m_Max = std::numeric_limits<T>::min();                       \
         variable->m_Engine = this;                                             \
          }
  ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
}




template<typename T>
void HermesEngine::DoGetSync_(const adios2::core::Variable<T> &variable,
                              T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  auto blob = Hermes->bkt->Get(variable.m_Name);
  std::string name = variable.m_Name;
#ifdef Meta_enabled
  // add spdlog method to extract the variable metadata

    metaInfo metaInfo(variable, adiosOpType::get, Hermes->bkt->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif
    memcpy(values, blob.data(), blob.size());
}

template<typename T>
void HermesEngine::DoGetDeferred_(
    const adios2::core::Variable<T> &variable, T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  auto blob = Hermes->bkt->Get(variable.m_Name);
  std::string name = variable.m_Name;
#ifdef Meta_enabled
  // add spdlog method to extract the variable metadata
    metaInfo metaInfo(variable, adiosOpType::get, Hermes->bkt->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif
  //finish metadata extraction
  memcpy(values, blob.data(), blob.size());


}

template<typename T>
void HermesEngine::DoPutSync_(const adios2::core::Variable<T> &variable,
                              const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  std::string name = variable.m_Name;
  Hermes->bkt->Put(name, variable.SelectionSize() * sizeof(T), values);



  // database
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(),
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(Hermes->bkt->name, name);

  DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
  client.Mdm_insertRoot(DomainId::GetLocal(), db_op);

  #ifdef POSIX_as_engine
const char *filename = "output.txt";
 int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
   size_t dataSize = variable.SelectionSize() * sizeof(T);
    ssize_t bytesWritten = write(fd, values, dataSize);
    if (bytesWritten == -1) {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }
#endif

#ifdef Meta_enabled
    metaInfo metaInfo(variable, adiosOpType::put, Hermes->bkt->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif


    std::vector<size_t> start2;

    if (variable.m_Start.empty() || variable.m_Start.data() == nullptr) {
        start2 = std::vector<size_t>();
    } else {
        start2 = variable.m_Start;
    }
    adios2::ADIOS adios_copy(MPI_COMM_WORLD);
    adios2::IO io_copy = adios_copy.DeclareIO("twins");
    io_copy.SetEngine("BPFile");
    adios2::Engine writer2 = io_copy.Open(adiosOutput, adios2::Mode::Append);
    adios2::Variable<T> var2 = io_copy.DefineVariable<T>(
            variable.m_Name, variable.Shape(), start2, variable.Count());
    writer2.Put(var2, values);
    writer2.Close();
}

template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  std::string name = variable.m_Name;
  Hermes->bkt->Put(name, variable.SelectionSize() * sizeof(T), values);
  // database
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(),
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(Hermes->bkt->name, name);
  DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
       client.Mdm_insertRoot(DomainId::GetLocal(), db_op);


#ifdef Meta_enabled
    metaInfo metaInfo(variable, adiosOpType::put, Hermes->bkt->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif
    std::vector<size_t> start2;

    if (variable.m_Start.empty() || variable.m_Start.data() == nullptr) {
        start2 = std::vector<size_t>();
    } else {
        start2 = variable.m_Start;
    }
    adios2::ADIOS adios_copy(MPI_COMM_WORLD);
    adios2::IO io_copy = adios_copy.DeclareIO("twins");
    io_copy.SetEngine("BPFile");
    adios2::Engine writer2 = io_copy.Open(adiosOutput, adios2::Mode::Append);
    adios2::Variable<T> var2 = io_copy.DefineVariable<T>(
            variable.m_Name, variable.Shape(), start2, variable.Count());
    writer2.Put(var2, values);
    writer2.Close();
}

}  // namespace coeus
/**
 * This is how ADIOS figures out where to dynamically load the engine.
 * */
extern "C" {

/** C wrapper to create engine */
coeus::HermesEngine *EngineCreate(adios2::core::IO &io,//NOLINT
                                  const std::string &name,
                                  const adios2::Mode mode,
                                  adios2::helper::Comm comm) {
  (void) mode;
  return new coeus::HermesEngine(io, name, mode, comm.Duplicate());
}

/** C wrapper to destroy engine */
void EngineDestroy(coeus::HermesEngine *obj) { delete obj; }
}
