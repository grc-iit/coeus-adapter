//
// Created by yejie on 9/16/20.
//

#include <hcl/common/singleton.h>
#include <sentinel/common/data_structures.h>
#include <stdio.h>
#include <memory>

typedef struct Job1: public Job{
    Job1(): Job(){}
    Job1(const Job1 &other): Job(other){}
    Job1(Job1 &other): Job(other) {}
    /*Define Assignment Operator*/
    Job1 &operator=(const Job1 &other){
        Job::operator=(other);
        return *this;
    }
    std::shared_ptr<Task> GetTask(uint32_t task_id_ = 0){
        printf("Begin to create Task in Job1....\n");
        //return std::make_shared<Task>();
    }

    void CreateDag(){
        printf("Job1 create Dag\n");
    }
};

extern "C" std::shared_ptr<Job> create_job_1() {
    printf("Begin to create object.....\n");
    //return hcl::Singleton<Job1>::GetInstance();
}
extern "C" void free_job_1(Job* p) { delete p; }


