//
// Created by yejie on 9/17/20.
//
#include <hcl/common/singleton.h>
#include <sentinel/common/data_structures.h>
#include <stdio.h>
#include <memory>

typedef struct Job2: public Job{
    Job2(): Job(){}
    Job2(const Job2 &other): Job(other){}
    Job2(Job2 &other): Job(other) {}
    /*Define Assignment Operator*/
    Job2 &operator=(const Job2 &other){
        Job::operator=(other);
        return *this;
    }
    std::shared_ptr<Task> GetTask(uint32_t task_id_){
        printf("Begin to create Task in Job1....\n");
        return std::make_shared<Task>();
    }

    void CreateDag(){
        printf("Job2 create Dag\n");
    }
};

extern "C" std::shared_ptr<Job> create_job_2() {
    printf("Begin to create object.....\n");
    return hcl::Singleton<Job2>::GetInstance();
}
extern "C" void free_job_2(Job* p) { delete p; }
