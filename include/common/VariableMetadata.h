//
// Created by linux on 10/23/23.
//



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

//____________Hua_______________
// this the variable metadata structure, it can collect the variable metadata information
struct metaInfo {
    std::string operation;
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
    int order;

    enum OperationType {
        get = 0,
        put = 1
    };


    template<typename T>
    metaInfo(const adios2::core::Variable<T> &variable, OperationType operationType) {
        name = variable.m_Name;
        operation = operationType;
        std::time_t currentTime = std::time(nullptr);
        std::tm* localTime = std::localtime(&currentTime);
        time = std::to_string(localTime->tm_hour) + ": " + std::to_string(localTime->tm_min)+ ": " + std::to_string(localTime->tm_sec);
        selectionSize = variable.SelectionSize();
        sizeofVariable = variable.m_ElementSize;
        shape = variable.Shape();
        shapeID = variable.m_ShapeID;
        stepStart = variable.m_AvailableStepsStart;
        blockID = variable.m_BlockID;
        steps = variable.m_AvailableStepsCount;
        if(operationType == put){
            order = PutCount;
            PutCount++;
            PutMap[name].push_back(PutCount);
        }
        if(operationType == get){
            order = GetCount;
            GetCount++;
            GetMap[name].push_back(GetCount);
        }
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
    explicit metaInfo(const adios2::Variable<T> variable, OperationType operationType) {
        name = variable.Name();
        shape = variable.Shape();
        operation = operationType;
        std::time_t currentTime = std::time(nullptr);
        std::tm* localTime = std::localtime(&currentTime);
        time = std::to_string(localTime->tm_hour) + ": " + std::to_string(localTime->tm_min)+ ": " + std::to_string(localTime->tm_sec);
        selectionSize = variable.SelectionSize();
        sizeofVariable= variable.Sizeof();
        shape = variable.Shape();
        shapeID = variable.ShapeID();
        stepStart = variable.StepStart();
        blockID = variable.BlockID();
        steps = variable.Steps();
        if(operationType == 1){
            order = PutCount;
            PutCount++;
             PutMap[name].push_back(PutCount);
        }
        if(operationType == 0){
            order = GetCount;
            GetCount++;
            GetMap[name].push_back(GetCount);
        }

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


    // TODO
    template<typename T>
    bool operator==(const adios2::core::Variable<T>& other) const {
        return name == other.m_Name &&
               shape == other.Shape() &&
               start == other.m_Start &&
               count == other.Count() &&
               constantShape == other.IsConstantDims() &&
               selectionSize == other.SelectionSize() &&
               sizeofVariable == other.m_ElementSize &&
               shapeID == other.m_shapeID  &&
               steps == other.m_AvailableStepsCount &&
               stepStart == other.m_AvailableStepsStart &&
               blockID == other.m_BlockID;
    }

    template <class Archive>
    void serialize(Archive &ar) {
        ar(operation, time, name, sizeofVariable, shapeID, shape, start, count, constantShape, dataType,
        steps, stepStart, blockID, order);
    }
};

std::ostream& operator<<(std::ostream &out, const metaInfo &data) {
    out << "Name: " << data.name << "\t";
    out << "Shape: ";
    for (const auto &s : data.shape) {
        out << s << " ";
    }
    out << "\tStart: ";
    for (const auto &s : data.start) {
        out << s << " ";
    }
    out << "\tCount: ";
    for (const auto &c : data.count) {
        out << c << " ";
    }
    out << "\tConstant Shape: " << (data.constantShape ? "True" : "False");
    out << "\tTime: " << data.time << "\tSelectionSize: " << data.selectionSize <<
        "\tsizeofVariable: " << data.sizeofVariable << "\tShapeID: " << data.shapeID << "\tsteps: " << data.steps <<
                                    "\tstepStart: " << data.stepStart << "\tblockID: " << data.blockID << "\torder: " << data.order << std::endl;
    return out;
}


class VariableMetadataSerializer{
public:
    static std::string SerializeBlobInfo(BlobInfo blob_info) {
        std::stringstream ss;
        {
            cereal::BinaryOutputArchive oarchive(ss);
            oarchive(blob_info);
        }
        return ss.str();
    }

    static BlobInfo DeserializeBlobInfo(const hermes::Blob &blob) {
        BlobInfo blob_info;
        std::stringstream ss;
        {
            ss.write(reinterpret_cast<char*>(blob.data()), blob.size());
            cereal::BinaryInputArchive iarchive(ss);
            iarchive(blob_info);
        }
        return blob_info;
    }

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
