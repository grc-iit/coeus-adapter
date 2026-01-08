#include "hermes_shm/util/gpu_api.h"

HSHM_GPU_KERNEL static void TestKernel(int x) { printf("ASFASDFAS: %d\n", x); }

int main() { TestKernel<<<1, 1>>>(252); }
