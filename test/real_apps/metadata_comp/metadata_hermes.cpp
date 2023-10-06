//
// Created by jaime on 9/28/2023.
//

#include <adios2.h>
#include <vector>
#include <cmath>
#include <chrono>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include "coeus/MetadataSerializer.h"
#include "comms/Hermes.h"

std::vector<double> produce_vector(int step) {
  return {step * 1.0, step * 2.0, step * 3.0};
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 3) {
    if (rank == 0) {
      std::cerr << "Please provide the number of steps as an argument." << std::endl;
    }
    MPI_Finalize();
    return -1;
  }

  int N = std::stoi(argv[1]);  // Number of steps
  int ppn = std::stoi(argv[2]);  // Number of steps

  adios2::ADIOS adios(MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");
  auto var = io.DefineVariable<double>("vector", {size_t(size), 3}, {size_t(rank), 0}, {1, 3}, adios2::ConstantDims);

  auto Hermes = std::make_unique<coeus::Hermes>();
  if(!Hermes->connect()){
    std::cout << "Could not connect to Hermes " <<  rank << std::endl;
    return -1;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double localInsertAppsTime = 0.0, localInsertBlobsTime = 0.0, localInsertMetadataTime = 0.0;

  for (int step = 0; step < N; ++step) {
    auto startInsertApps = std::chrono::high_resolution_clock::now();

    if (rank % ppn == 0) {
      int currentStep;
      auto blob_name = "total_steps_" + io.Name();
      Hermes->GetBucket("total_steps");
      Hermes->bkt->Put(blob_name, sizeof(int), &currentStep);
    }

    auto endInsertApps = std::chrono::high_resolution_clock::now();
    localInsertAppsTime += std::chrono::duration<double>(endInsertApps - startInsertApps).count();\
    delete Hermes->bkt;

    auto startInsertBlobs = std::chrono::high_resolution_clock::now();
    {
      std::string bucket_name = "Variable_step_" + std::to_string(step) + "_rank_" + std::to_string(rank);
      BlobInfo blob_info(bucket_name, io.Name());

      std::string serializedBlobInfo = MetadataSerializer::SerializeBlobInfo(blob_info);

      Hermes->GetBucket(bucket_name);
      Hermes->bkt->Put("Var" + std::to_string(step), serializedBlobInfo.size(), serializedBlobInfo.data());

    }
    auto endInsertBlobs = std::chrono::high_resolution_clock::now();
    localInsertBlobsTime += std::chrono::duration<double>(endInsertBlobs - startInsertBlobs).count();
    delete Hermes->bkt;

    auto startInsertMetadata = std::chrono::high_resolution_clock::now();
    {
      std::string bucket_name_metadata = "step_" + std::to_string(step) + "_rank_" + std::to_string(rank);
      VariableMetadata metadata("Var" + std::to_string(step), {4, 4}, {0, 0}, {4, 4}, true, "int");

      std::string serializedMetadata = MetadataSerializer::SerializeMetadata(metadata);

      Hermes->GetBucket(bucket_name_metadata);
      Hermes->bkt->Put("Var" + std::to_string(step), serializedMetadata.size(), serializedMetadata.data());
    }
    auto endInsertMetadata = std::chrono::high_resolution_clock::now();
    localInsertMetadataTime += std::chrono::duration<double>(endInsertMetadata - startInsertMetadata).count();
    delete Hermes->bkt;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0) std::cout << "Rank " << rank << " finished writting" << std::endl;

  double localQueryAppsTime = 0.0, localQueryBlobsTime = 0.0, localQueryMetadataTime = 0.0;
  for (int step = 0; step < N; ++step) {
    auto startQueryApps = std::chrono::high_resolution_clock::now();
    {
      Hermes->GetBucket("total_steps");
      hermes::Blob blob = Hermes->bkt->Get("total_steps_" + io.Name());
      auto total_steps = *reinterpret_cast<const int *>(blob.data());
    }
    auto endQueryApps = std::chrono::high_resolution_clock::now();
    localQueryAppsTime += std::chrono::duration<double>(endQueryApps - startQueryApps).count();
    delete Hermes->bkt;

    auto startQueryBlobs = std::chrono::high_resolution_clock::now();
    {
      std::string bucket_name = "Variable_step_" + std::to_string(step) + "_rank_" + std::to_string(rank);

      Hermes->GetBucket(bucket_name);
      std::vector<hermes::BlobId> blobIds = Hermes->bkt->GetContainedBlobIds();
      for (const auto &blobId : blobIds) {
        hermes::Blob blob = Hermes->bkt->Get(blobId);
        BlobInfo blob_info =
            MetadataSerializer::DeserializeBlobInfo(blob);
      }
    }
    auto endQueryBlobs = std::chrono::high_resolution_clock::now();
    localQueryBlobsTime += std::chrono::duration<double>(endQueryBlobs - startQueryBlobs).count();
    delete Hermes->bkt;

    auto startQueryMetadata = std::chrono::high_resolution_clock::now();
    {
      std::string filename = "step_" + std::to_string(step) +
          "_rank_" + std::to_string(rank);

      Hermes->GetBucket(filename);
      std::vector<hermes::BlobId> blobIds = Hermes->bkt->GetContainedBlobIds();
      for (const auto &blobId : blobIds) {
        hermes::Blob blob = Hermes->bkt->Get(blobId);
        VariableMetadata variableMetadata =
            MetadataSerializer::DeserializeMetadata(blob);
      }
    }
    auto endQueryMetadata = std::chrono::high_resolution_clock::now();
    localQueryMetadataTime += std::chrono::duration<double>(endQueryMetadata - startQueryMetadata).count();
    delete Hermes->bkt;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0) std::cout << "Rank " << rank << " finished reading" << std::endl;

  double globalInsertAppsTime = 0.0, globalInsertBlobsTime = 0.0, globalInsertMetadataTime = 0.0;
  double globalQueryAppsTime = 0.0, globalQueryBlobsTime = 0.0, globalQueryMetadataTime = 0.0;

  MPI_Reduce(&localInsertAppsTime, &globalInsertAppsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localInsertBlobsTime, &globalInsertBlobsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localInsertMetadataTime, &globalInsertMetadataTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localQueryAppsTime, &globalQueryAppsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localQueryBlobsTime, &globalQueryBlobsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localQueryMetadataTime, &globalQueryMetadataTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  MPI_Barrier(MPI_COMM_WORLD);

  if(rank == 0) {
    std::string header = "Size,N,"
                         "globalInsertAppsTime,globalInsertBlobsTime,globalInsertMetadataTime,"
                         "globalQueryAppsTime,globalQueryBlobsTime,globalQueryMetadataTime\n";
    bool needHeader = false;

    // Check if the file is empty or doesn't exist
    std::ifstream checkFile("metadata_hermes_results.csv");
    if (!checkFile.good() || checkFile.peek() == std::ifstream::traits_type::eof()) {
      needHeader = true;
    }
    checkFile.close();

    // Open the file for appending
    std::ofstream outputFile("metadata_hermes_results.csv", std::ios_base::app);

    // Write the header if needed
    if (needHeader) {
      outputFile << header;
    }

    // Append the results
    outputFile << size << ","
               << N  << ","
               << globalInsertAppsTime  << ","
               << globalInsertBlobsTime  << ","
               << globalInsertMetadataTime  << ","
               << globalQueryAppsTime  << ","
               << globalQueryBlobsTime  << ","
               << globalQueryMetadataTime  << "\n";    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}