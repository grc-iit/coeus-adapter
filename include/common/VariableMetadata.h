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



#ifndef COEUS_ADAPTER_VARIABLEMETADATA_H
#define COEUS_ADAPTER_VARIABLEMETADATA_H

#include <hermes/hermes_types.h>
#include "common/MetadataStructs.h"
#include <vector>
#include <string>

#include <vector>
#include <string>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <adios2/core/Variable.h>
#include <adios2/cxx11/Variable.h>
#include "globalVariable.h"


// This structure is designed to capture metadata information about adios2 variables.

enum class adiosOpType {
    get = 0,
    put = 1
};

struct metaInfo {
    adiosOpType operation;
    std::string time;
    std::string name;
    std::string dataType;
    size_t selectionSize;
    size_t sizeofVariable;
    adios2::ShapeID shapeID;
    std::vector<size_t> shape;
    std::vector<size_t> start;
    std::vector<size_t> count;
    bool constantShape;
    size_t steps;
    size_t stepStart;
    size_t blockID;
    //int order;
    std::string blob_name;
    std::string bucket_name;
    std::string processor;


    template<typename T>
    metaInfo(const adios2::core::Variable<T> &variable, adiosOpType operationType, std::string blob,
             std::string bucket, std::string processor_name ) {
        name = variable.m_Name;
        operation = operationType;
        blob_name = blob;
        processor = processor_name;
        bucket_name = bucket;
        std::time_t currentTime = std::time(nullptr);
        std::tm* localTime = std::localtime(&currentTime);
        time = std::to_string(localTime->tm_hour) + ":" + std::to_string(localTime->tm_min)+ ":" + std::to_string(localTime->tm_sec);
        selectionSize = variable.SelectionSize();
        sizeofVariable = variable.m_ElementSize;
        shape = variable.Shape();
        shapeID = variable.m_ShapeID;
        stepStart = variable.m_AvailableStepsStart;
        blockID = variable.m_BlockID;
        steps = variable.m_AvailableStepsCount;
        // Check if start is empty or null and assign an empty array if so
        if (variable.m_Start.empty() || variable.m_Start.data() == nullptr) {
            start = std::vector<size_t>();
        } else {
            start = variable.m_Start;
        }

        count = variable.Count();

        // Check if count is empty or null and assign an empty array if so
        if (variable.Count().empty() || variable.Count().data() == nullptr) {
            count = std::vector<size_t>();
        } else {
            count = variable.Count();
        }

        constantShape = variable.IsConstantDims();
        dataType = adios2::ToString(variable.m_Type);
    }

    template<typename T>
    explicit metaInfo(const adios2::Variable<T> variable, adiosOpType operationType,  std::string blob,
                      std::string bucket, std::string processor_name ) {
        name = variable.Name();
        shape = variable.Shape();
        processor = processor_name;
        operation = operationType;
        blob_name = blob;
        bucket_name = bucket;
        std::time_t currentTime = std::time(nullptr);
        std::tm* localTime = std::localtime(&currentTime);
        time = std::to_string(localTime->tm_hour) + ":" + std::to_string(localTime->tm_min)+ ":" + std::to_string(localTime->tm_sec);
        selectionSize = variable.SelectionSize();
        sizeofVariable= variable.Sizeof();
        shape = variable.Shape();
        shapeID = variable.ShapeID();
        stepStart = variable.StepStart();
        blockID = variable.BlockID();
        steps = variable.Steps();

        // Check if start is empty or null and assign an empty array if so
        if (variable.Start().empty() || variable.Start().data() == nullptr) {
            start = std::vector<size_t>();
        } else {
            start = variable.Start();
        }

        // Check if count is empty or null and assign an empty array if so
        if (variable.Count().empty() || variable.Count().data() == nullptr) {
            count = std::vector<size_t>();
        } else {
            count = variable.Count();
        }

        constantShape = false;
        /* We need to see if this is correct.
           Though this is mostly just for the unit test,
           as the engine will reference the adios::core::variable constructor */
        dataType = variable.Type();
    }


    adios2::DataType getDataType() {
        return adios2::helper::GetDataTypeFromString(dataType);
    }


    template <class Archive>
    void serialize(Archive &ar) {
        ar(operation, time, name, sizeofVariable, shapeID, shape, start, count, constantShape, dataType,
        steps, stepStart, blockID, blob_name, bucket_name, processor);
    }
};

// Name, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable, ShapeID, steps, stepstart, blockID, order.
std::ostream& operator<<(std::ostream &out, const metaInfo &data) {
    out << data.name << ",";

    for (const auto &s : data.shape) {
        out << s << " ";
    }
    out << ",";
    for (const auto &s : data.start) {
        out << s << " ";
    }
    out << ",";
    for (const auto &c : data.count) {
        out << c << " ";
    }
    out << "," << (data.constantShape ? "True" : "False");
    out << "," << data.time << "," << data.selectionSize <<
        "," << data.sizeofVariable << "," << data.shapeID << "," << data.steps <<
                                    "," << data.stepStart << "," << data.blockID << "," <<
                                    data.blob_name << "," << data.bucket_name << "," << data.processor;
    return out;
}

std::string metaInfoToString(metaInfo variableMetadata) {
    std::stringstream ss;
    ss << variableMetadata;
    return ss.str();
}

class VariableMetadataSerializer{
public:



    static std::string SerializeMetadata(metaInfo variableMetadata) {
        std::stringstream ss;
        {
            cereal::BinaryOutputArchive oarchive(ss);
            oarchive(variableMetadata);
        }
        return ss.str();
    }


};
#endif //COEUS_ADAPTER_VARIABLEMETADATA_H
