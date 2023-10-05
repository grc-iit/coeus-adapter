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
#include "common/SQlite.h"
#include "common/FileLock.h"

std::vector<double> produce_vector(int step) {
  return {step * 1.0, step * 2.0, step * 3.0};
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 4) {
    if (rank == 0) {
      std::cerr << "Please provide the number of steps as an argument." << std::endl;
    }
    MPI_Finalize();
    return -1;
  }

  int N = std::stoi(argv[1]);  // Number of steps
  std::string db_path = argv[2];  // Number of steps
  int ppn = std::stoi(argv[3]);

  adios2::ADIOS adios(MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");

  auto var = io.DefineVariable<double>("vector", {size_t(size), 3}, {size_t(rank), 0}, {1, 3}, adios2::ConstantDims);

  FileLock lock(db_path + ".lock");
  lock.lock();
  SQLiteWrapper db(db_path);
  lock.unlock();

  MPI_Barrier(MPI_COMM_WORLD);

  double localInsertAppsTime = 0.0, localInsertBlobsTime = 0.0, localInsertMetadataTime = 0.0,
      localInsertBlobsTimeLocked = 0.0, localInsertMetadataTimeLocked = 0.0;

  for (int step = 0; step < N; ++step) {
    if (rank % ppn == 0) {
      auto startInsertApps = std::chrono::high_resolution_clock::now();
      lock.lock();
      db.UpdateTotalSteps("App" + std::to_string(rank), step);
      lock.unlock();
      auto endInsertApps = std::chrono::high_resolution_clock::now();
      localInsertAppsTime += std::chrono::duration<double>(endInsertApps - startInsertApps).count();
    }
  }


  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0) std::cout << "Rank " << rank << " finished inserting apps" << std::endl;

  for (int step = 0; step < N; ++step) {
    auto startInsertBlobsLocked = std::chrono::high_resolution_clock::now();
    lock.lock();
    auto startInsertBlobs = std::chrono::high_resolution_clock::now();
    BlobInfo blobInfo("Bucket" + std::to_string(rank), "Blob" + std::to_string(step));
    db.InsertBlobLocation(step, rank, "Var" + std::to_string(step), blobInfo);
    auto endInsertBlobs = std::chrono::high_resolution_clock::now();
    localInsertBlobsTime += std::chrono::duration<double>(endInsertBlobs - startInsertBlobs).count();
    lock.unlock();
    auto endInsertBlobsLocked = std::chrono::high_resolution_clock::now();
    localInsertBlobsTimeLocked += std::chrono::duration<double>(endInsertBlobsLocked - startInsertBlobsLocked).count();

    auto startInsertMetadataLocked = std::chrono::high_resolution_clock::now();
    lock.lock();
    auto startInsertMetadata = std::chrono::high_resolution_clock::now();
    VariableMetadata metadata("Var" + std::to_string(step), {4, 4}, {0, 0}, {4, 4}, true, "int");
    db.InsertVariableMetadata(step, rank, metadata);
    auto endInsertMetadata = std::chrono::high_resolution_clock::now();
    localInsertMetadataTime += std::chrono::duration<double>(endInsertMetadata - startInsertMetadata).count();
    lock.unlock();
    auto endInsertMetadataLocked = std::chrono::high_resolution_clock::now();
    localInsertMetadataTimeLocked += std::chrono::duration<double>(endInsertMetadataLocked - startInsertMetadataLocked).count();
  }

  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0) std::cout << "Rank " << rank << " finished writting" << std::endl;

  double localQueryAppsTime = 0.0, localQueryBlobsTime = 0.0, localQueryMetadataTime = 0.0;
  for (int step = 0; step < N; ++step) {
    auto startQueryApps = std::chrono::high_resolution_clock::now();
    db.GetTotalSteps("App" + std::to_string(rank));
    auto endQueryApps = std::chrono::high_resolution_clock::now();
    localQueryAppsTime += std::chrono::duration<double>(endQueryApps - startQueryApps).count();

    auto startQueryBlobs = std::chrono::high_resolution_clock::now();
    db.GetBlobLocation(step, rank, "Var" + std::to_string(step));
    auto endQueryBlobs = std::chrono::high_resolution_clock::now();
    localQueryBlobsTime += std::chrono::duration<double>(endQueryBlobs - startQueryBlobs).count();

    auto startQueryMetadata = std::chrono::high_resolution_clock::now();
    db.GetVariableMetadata(step, rank, "Var" + std::to_string(step));
    auto endQueryMetadata = std::chrono::high_resolution_clock::now();
    localQueryMetadataTime += std::chrono::duration<double>(endQueryMetadata - startQueryMetadata).count();
  }

  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0) std::cout << "Rank " << rank << " finished reading" << std::endl;

  double globalInsertAppsTime = 0.0, globalInsertBlobsTime = 0.0, globalInsertMetadataTime = 0.0;
  double globalInsertBlobsTimeLocked = 0.0, globalInsertMetadataTimeLocked = 0.0;
  double globalQueryAppsTime = 0.0, globalQueryBlobsTime = 0.0, globalQueryMetadataTime = 0.0;

  MPI_Reduce(&localInsertAppsTime, &globalInsertAppsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localInsertBlobsTime, &globalInsertBlobsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localInsertBlobsTimeLocked, &globalInsertBlobsTimeLocked, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localInsertMetadataTime, &globalInsertMetadataTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localInsertMetadataTimeLocked, &globalInsertMetadataTimeLocked, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localQueryAppsTime, &globalQueryAppsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localQueryBlobsTime, &globalQueryBlobsTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localQueryMetadataTime, &globalQueryMetadataTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  MPI_Barrier(MPI_COMM_WORLD);

  if(rank == 0) {
    std::string header = "Size,"
                         "globalInsertAppsTime,globalInsertBlobsTime,globalInsertBlobsTimeLocked"
                         "globalInsertMetadataTime,globalInsertMetadataTimeLocked"
                         "globalQueryAppsTime,globalQueryBlobsTime,globalQueryMetadataTime\n";
    bool needHeader = false;

    // Check if the file is empty or doesn't exist
    std::ifstream checkFile("metadata_empress_results.csv");
    if (!checkFile.good() || checkFile.peek() == std::ifstream::traits_type::eof()) {
      needHeader = true;
    }
    checkFile.close();

    // Open the file for appending
    std::ofstream outputFile("metadata_empress_results.csv", std::ios_base::app);

    // Write the header if needed
    if (needHeader) {
      outputFile << header;
    }

    // Append the results
    outputFile << size << ","
            << N  << ","
            << globalInsertAppsTime  << ","
            << globalInsertBlobsTime  << ","
            << globalInsertBlobsTimeLocked  << ","
            << globalInsertMetadataTime << ","
            << globalInsertMetadataTimeLocked  << ","
            << globalQueryAppsTime  << ","
            << globalQueryBlobsTime  << ","
            << globalQueryMetadataTime << "\n";    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}