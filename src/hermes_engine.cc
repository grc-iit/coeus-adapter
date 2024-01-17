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
//  if (comm.Rank() == 0) std::cout << "NAMING: " << name << " " << this->m_Name << " "
//  << m_Name << " " << this->m_IO.m_Name << std::endl;
        Hermes = std::make_shared<coeus::Hermes>();
//  mpiComm = std::make_shared<coeus::MPI>(comm.Duplicate());
        Init_();
        TRACE_FUNC();
        engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));
        // debug model
        engine_logger->info("Debug: initial done rank: {}", rank);
    }

/**
 * Test initializer
 * */
    HermesEngine::HermesEngine(std::shared_ptr <coeus::IHermes> h,
                               std::shared_ptr <coeus::MPI> mpi,
                               adios2::core::IO &io,
                               const std::string &name,
                               const adios2::Mode mode,
                               adios2::helper::Comm comm)
            : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
        Hermes = h;
//  mpiComm = mpi;
        Init_();
        TRACE_FUNC();
        engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));
        // debug model
        engine_logger->info("Debug: initial done 2 rank: {}", rank);
    }

/**
* Initialize the engine.
* */
    void HermesEngine::Init_() {
        // initiate the trace manager
        INIT_TRACE_MANAGER();
        // Logger setup
        // Console log

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

        //File log
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                "logs/engine_test.txt", true);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

        // File log for metadata collection
        auto file_sink2 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                "logs/metadataCollect_get.txt", true);
        file_sink2->set_level(spdlog::level::trace);
        file_sink2->set_pattern("%v");
        spdlog::logger logger2("metadata_logger_get", {file_sink2});
        logger2.set_level(spdlog::level::trace);
        meta_logger_get = std::make_shared<spdlog::logger>(logger2);
        meta_logger_get->info(
                "Name, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable, ShapeID, steps, stepstart, blockID");
        auto file_sink3 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                "logs/metadataCollect_put.txt", true);
        file_sink3->set_level(spdlog::level::trace);
        file_sink3->set_pattern("%v");
        spdlog::logger logger3("metadata_logger_put", {file_sink3});
        logger3.set_level(spdlog::level::trace);
        meta_logger_put = std::make_shared<spdlog::logger>(logger3);
        meta_logger_put->info(
                "Name, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable, ShapeID, steps, stepstart, blockID");

        //Merge Log
        spdlog::logger logger("debug_logger", {console_sink, file_sink});
        logger.set_level(spdlog::level::debug);
        engine_logger = std::make_shared<spdlog::logger>(logger);



        //MPI setup
        rank = getpid();
        comm_size = m_Comm.Size();
        pid_t processId = getpid();

        engine_logger->info("rank {}", rank);
        engine_logger->info("comm size {}", comm_size);
        engine_logger->info("Process Id {}", processId);
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
        if (!Hermes->connect()) {
            engine_logger->warn("Could not connect to Hermes", rank);
            throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
        }
        if (rank == 0) std::cout << "Connected to Hermes" << std::endl;

//        if (params.find("db_file") != params.end()) {
//            db_file = params["db_file"];
//            db = new SQLiteWrapper(db_file);
//            if (rank % ppn == 0) {
//                db->createTables();
//                std::cout << "DB_FILE: " << db_file << std::endl;
//            }
//            TRANSPARENT_HERMES();
//            client.CreateRoot(DomainId::GetGlobal(), "db_operation", db_file);
//            if (rank == 0) std::cout << "Done with root" << std::endl;
//        } else {
//            throw std::invalid_argument("db_file not found in parameters");
//        }
        open = true;




        // debug mode
        engine_logger->info("initial Done 3, rank {}", rank);
        const size_t bufferSize = 1024;  // Define the buffer size
        char buffer[bufferSize];         // Create a buffer to hold the hostname

// Get the hostname
        if (gethostname(buffer, bufferSize) == 0) {
            std::cout << "Hostname: " << buffer << std::endl;
            engine_logger->info("Hostname: {}", buffer);
        }

        char processor_name[MPI_MAX_PROCESSOR_NAME];
        int name_len;
        MPI_Get_processor_name(processor_name, &name_len);

        std::cout << "Process " << processId << "rank " << rank << " is running on " << processor_name << std::endl;
        engine_logger->info("rank: {} Process: {} is running on {} ", rank, processId, processor_name);
        engine_logger->flush();
    }

/**
 * Close the Engine.
 * */
    void HermesEngine::DoClose(const int transportIndex) {
        TRACE_FUNC();
        engine_logger->info("Do close rank {}", rank);
        open = false;
//  mpiComm->free();

        engine_logger->flush();
    }

    HermesEngine::~HermesEngine() {
        TRACE_FUNC();
        std::cout << "Close des" << std::endl;
        engine_logger->info("rank {}", rank);
        delete db;

        // debug mode
        engine_logger->info("deconstrctor Done, rank {}", rank);
        engine_logger->flush();
    }

/**
 * Handle step operations.
 * */

    adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                               const float timeoutSeconds) {
        TRACE_FUNC();
        IncrementCurrentStep();
        // debug mode
        engine_logger->info("finish IncrementCurrentStep, rank {}", rank);


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

        engine_logger->info("finish BeginStep and Hermes getBucket, rank {}", rank);
        engine_logger->flush();
        return adios2::StepStatus::OK;
    }

    void HermesEngine::IncrementCurrentStep() {
        TRACE_FUNC();
        currentStep++;
    }

    size_t HermesEngine::CurrentStep() const {
        TRACE_FUNC();
        return currentStep;
    }

    void HermesEngine::EndStep() {
        TRACE_FUNC();
//   if (m_OpenMode == adios2::Mode::Write) {
//    if(rank % ppn == 0) {
//      DbOperation db_op(uid, currentStep);
//      client.Mdm_insertRoot(DomainId::GetGlobal(), db_op);
//    }
//  }
        // debug Mode
        engine_logger->info("Finish EndStep, rank {}", rank);
        delete Hermes->bkt;
        engine_logger->info("Finish EndStep and delete Hermes->bkt, rank {}", rank);
        engine_logger->flush();
    }

/**
 * Metadata operations.
 * */
    bool HermesEngine::VariableMinMax(const adios2::core::VariableBase &Var,
                                      const size_t Step,
                                      adios2::MinMaxStruct &MinMax) {
        TRACE_FUNC();
        // We initialize the min and max values
        MinMax.Init(Var.m_Type);

        // Obtain the blob from Hermes using the filename and variable name
        hermes::Blob blob = Hermes->bkt->Get(Var.m_Name);
        // debug Mode
        engine_logger->info("finish VariableMinMax, rank {}", rank);
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
            case adios2::DataType::Int8:
                ElementMinMax<int8_t>(MinMax, Element);
                break;
            case adios2::DataType::Int16:
                ElementMinMax<int16_t>(MinMax, Element);
                break;
            case adios2::DataType::Int32:
                ElementMinMax<int32_t>(MinMax, Element);
                break;
            case adios2::DataType::Int64:
                ElementMinMax<int64_t>(MinMax, Element);
                break;
            case adios2::DataType::UInt8:
                ElementMinMax<uint8_t>(MinMax, Element);
                break;
            case adios2::DataType::UInt16:
                ElementMinMax<uint16_t>(MinMax, Element);
                break;
            case adios2::DataType::UInt32:
                ElementMinMax<uint32_t>(MinMax, Element);
                break;
            case adios2::DataType::UInt64:
                ElementMinMax<uint64_t>(MinMax, Element);
                break;
            case adios2::DataType::Float:
                ElementMinMax<float>(MinMax, Element);
                break;
            case adios2::DataType::Double:
                ElementMinMax<double>(MinMax, Element);
                break;
            case adios2::DataType::LongDouble:
                ElementMinMax<long double>(MinMax, Element);
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
        // debug Mode

    }

    template<typename T>
    void HermesEngine::ElementMinMax(adios2::MinMaxStruct &MinMax, void *element) {
        TRACE_FUNC();
        T *min = SelectUnion<T>(MinMax.MinUnion);
        T *max = SelectUnion<T>(MinMax.MaxUnion);
        T *value = static_cast<T *>(element);
        if (*value < *min) {
            min = value;
        }
        if (*value > *max) {
            max = value;
        }
        // debug mode
        engine_logger->info("ElementMinMax Done, rank {}", rank);
        engine_logger->flush();
    }

    void HermesEngine::LoadMetadata() {
        TRACE_FUNC();
        auto metadata_vector = db->GetAllVariableMetadata(currentStep, rank);
        for (auto &variableMetadata: metadata_vector) {

            DefineVariable(variableMetadata);
        }


        // debug mode
        engine_logger->info("LoadMetadata Done, rank {}", rank);
        engine_logger->flush();
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
    void HermesEngine::DoGetSync_(const adios2::core::Variable <T> &variable,
                                  T *values) {
        TRACE_FUNC();
        auto blob = Hermes->bkt->Get(variable.m_Name);
        std::string name = variable.m_Name;
#ifdef Meta_enabled
        // add spdlog method to extract the variable metadata
        metaInfo metaInfo(variable, adiosOpType::get);
        meta_logger_get->info("metadata: {}", metaInfoToString(metaInfo));
        globalData.insertGet(name);
        meta_logger_get->info("order: {}", globalData.GetMapToString());
#endif
        //finish metadata extraction
        memcpy(values, blob.data(), blob.size());

        // debug mode
        engine_logger->info("Get Done, rank {}", rank);
        engine_logger->flush();
    }


    template<typename T>
    void HermesEngine::DoGetDeferred_(
            const adios2::core::Variable <T> &variable, T *values) {
        TRACE_FUNC();
        auto blob = Hermes->bkt->Get(variable.m_Name);
        std::string name = variable.m_Name;
#ifdef Meta_enabled
        // add spdlog method to extract the variable metadata
        metaInfo metaInfo(variable, adiosOpType::get);
        meta_logger_get->info("metadata: {}", metaInfoToString(metaInfo));
        globalData.insertGet(name);
        meta_logger_get->info("order: {}", globalData.GetMapToString());
#endif
        //finish metadata extraction
        memcpy(values, blob.data(), blob.size());

        // debug mode
        engine_logger->info("Get Done, rank {}", rank);
        engine_logger->flush();
    }

    template<typename T>
    void HermesEngine::DoPutSync_(const adios2::core::Variable <T> &variable,
                                  const T *values) {
        TRACE_FUNC();
        std::string name = variable.m_Name;
        Hermes->bkt->Put(name, variable.SelectionSize() * sizeof(T), values);
        std::cout << "SyncPut variable info: " <<  " ranks: " << rank << " variable Name: " << variable.m_Name;
        std::cout << " sizeof: " << sizeof(T) << " selectionSize: " <<  variable.SelectionSize() << std::endl;
        /* 2. POSIX engine
        std::string c_filename = "/mnt/nvme/hxu40/output_." + std::to_string(rank) + ".txt";
        const char* filename = c_filename.c_str();
        int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
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
      */
        // metadata extraction

        metaInfo metaInfo(variable, adiosOpType::put);
        meta_logger_put->info("metadata: {}", metaInfoToString(metaInfo));


        // database
//        VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
//                            variable.m_Count, variable.IsConstantDims(),
//                            adios2::ToString(variable.m_Type));
//        BlobInfo blobInfo(Hermes->bkt->name, name);
//        DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
//        client.Mdm_insertRoot(DomainId::GetGlobal(), db_op);

        // debug mode
        engine_logger->info("Put Done, rank {}", rank);
        engine_logger->flush();

    }


    template<typename T>
    void HermesEngine::DoPutDeferred_(
            const adios2::core::Variable <T> &variable, const T *values) {
        TRACE_FUNC();
        std::string name = variable.m_Name;
        Hermes->bkt->Put(name, variable.SelectionSize() * sizeof(T), values);
        std::cout << "DeferredPut variable info: " <<  " ranks: " << rank << " variable Name: " << variable.m_Name;
        std::cout << " sizeof: " << sizeof(T) << " selectionSize: " <<  variable.SelectionSize() << std::endl;
                /* 2. POSIX engine
                std::string c_filename = "/mnt/nvme/hxu40/output_." + std::to_string(rank) + ".txt";
                const char* filename = c_filename.c_str();
                int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
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
              */
                // metadata extraction


        metaInfo metaInfo(variable, adiosOpType::put);
        meta_logger_put->info("metadata: {}", metaInfoToString(metaInfo));


        // database
//        VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
//                            variable.m_Count, variable.IsConstantDims(),
//                            adios2::ToString(variable.m_Type));
//        BlobInfo blobInfo(Hermes->bkt->name, name);
//        DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
//        client.Mdm_insertRoot(DomainId::GetGlobal(), db_op);

        // debug mode
        engine_logger->info("Put Done, rank {}", rank);
        engine_logger->flush();
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

