# 一个加法运算

```cpp
#include <iostream>
#define N 10
__global__ void add(int *a, int *b, int *c) {
    int tid = blockIdx.x;
    if (tid < N) {
        c[tid]=a[tid]+b[tid];
    }
}
int main() {
  int a[N], b[N], c[N];
  int *dev_a, *dev_b, *dev_c;
  cudaMalloc((void **)&dev_a, N * sizeof(int));
  cudaMalloc((void **)&dev_b, N * sizeof(int));
  cudaMalloc((void **)&dev_c, N * sizeof(int));
  for (int i = 0; i < N; i++) {
    a[i] = -i;
    b[i]=i*i;
  }
  cudaMemcpy(dev_a, a, N * sizeof(int), cudaMemcpyHostToDevice);
  cudaMemcpy(dev_b, b, N * sizeof(int), cudaMemcpyHostToDevice);
  add<<<N, 1>>>(dev_a, dev_b, dev_c);
  for (int i = 0; i < N; i++) {
      printf("%d + %d = %d\n",a[i],b[i],c[i]);
  }
  cudaFree(dev_a);
  cudaFree(dev_b);
  cudaFree(dev_c);
}
```

作为一个最简单的应用，可以看出来cuda的编程思想了。

计算无关的最好逻辑放到CPU上，剩下的内容在GPU上则是可非常容易进行并行化的。

每一个并行执行的环境叫做线程块，指定多少个线程就会有这么多的线程进行运算。

# 线程同步

CUDA下的共享数组类似于RCU,所以必须对数据进行同步，避免竞态条件。

一个并行的点积的例子：

```cpp
#include <iostream>
#include <cuda.h>
#define imin(a, b) (a < b ? a : b)
#define sum_squares(x) (x * (x + 1) * (2 * x + 1) / 6)
static constexpr int N = 33 * 1024;
constexpr static int threadPerBlock = 256;
__global__ void dot(float *a, float *b, float *c) {
  __shared__ float cache[threadPerBlock];
  int tid = threadIdx.x + blockIdx.x * blockDim.x;
  int cacheIndex = threadIdx.x;
  float temp = 0;
  while (tid < N) {
    temp += a[tid] * b[tid];
    tid += blockDim.x * gridDim.x;
  }
  cache[cacheIndex] = temp;
  __syncthreads();
  int i = blockDim.x / 2;
  while (i != 0) {
    if (cacheIndex < i) {
        cache[cacheIndex]+=cache[cacheIndex+i];
    }
    __syncthreads();
    i/=2;
  }
  if (cacheIndex == 0)
    c[blockIdx.x] = cache[0];

}
constexpr static int blocksPerGrid =
    imin(32, (N + threadPerBlock - 1) / threadPerBlock);
int main() {
  float *a, *b, c, *partial_c;
  float *dev_a, *dev_b, *dev_partial_c;
  a = new float[N];
  b = new float[N];
  partial_c = new float[blocksPerGrid];
  cudaMalloc((void **)&dev_a, N * sizeof(float));
  cudaMalloc((void **)&dev_b, N * sizeof(float));
  cudaMalloc((void **)&dev_partial_c, blocksPerGrid * sizeof(float));
  for (int i = 0; i < N; i++) {
    a[i] = i;
    b[i]=i*2;
  }
  cudaMemcpy(dev_a, a, N * sizeof(float), cudaMemcpyHostToDevice);
  cudaMemcpy(dev_b, b, N * sizeof(float), cudaMemcpyHostToDevice);
  dot<<<blocksPerGrid, threadPerBlock>>>(dev_a, dev_b, dev_partial_c);
  cudaMemcpy(partial_c, dev_partial_c, blocksPerGrid * sizeof(float),
             cudaMemcpyDeviceToHost);
  c = 0;
  for (int i = 0; i < blocksPerGrid; i++) {
      c += partial_c[i];
  }
  printf("Does GPU Value %.6g = %.6g?\n",c,2*sum_squares((float)(N-1)));
  cudaFree(dev_a);
  cudaFree(dev_b);
  cudaFree(dev_partial_c);
  delete [] a;
  delete [] b;
  delete [] partial_c;

}
```

需要知道线程发散带来的问题。

## 常量内存

__constant__ 这个标志可以把变量的访问变成只读，好处是可以节约内存带宽。

1. 对常量内存的单次读操作可以广播到其他的邻近线程，这可以节约了15次读取。
2. 常量内存的数据将缓存起来，对相同地址的连续读操作不会产生额外的内存通信量。

## 线程束Warp

CUDA中的线程束是一个包含32个线程的集合，按照统一的方式执行。

举个例子：

```cpp
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include <stdio.h>
#include <math.h>
#include "common/book.h"
#include "common/cpu_bitmap.h"

#define INF 2e10f
#define rnd( x ) (x * rand() / RAND_MAX)
#define DIM 1024
#define SPHERES 20

struct Sphere
{
    float r, g, b;
    float radius;
    float x, y, z;

    __device__ float hit(float ox, float oy, float* n) {
        float dx = ox - x;
        float dy = oy - y;
        if (dx * dx + dy * dy < radius * radius) {
            float dz = sqrtf(radius * radius - dx * dx - dy * dy);
            *n = dz / sqrtf(radius * radius);
            return dz + z;
        }
        return -INF;
    }
};

__constant__ Sphere dev_s[SPHERES];

__global__ void kernel(unsigned char* ptr) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int offset = y * gridDim.x * blockDim.x + x;

    float ox = (x - DIM / 2);
    float oy = (y - DIM / 2);

    float r = 0, g = 0, b = 0;
    float maxz = -INF;
    for (int i = 0; i < SPHERES; i++) {
        float n;
        float t = dev_s[i].hit(ox, oy, &n);
        if (t > maxz) {
            float fscale = n;
            r = dev_s[i].r * fscale;
            g = dev_s[i].g * fscale;
            b = dev_s[i].b * fscale;
            maxz = t;
        }
    }

    ptr[offset * 4 + 0] = (int)(r * 255);
    ptr[offset * 4 + 1] = (int)(g * 255);
    ptr[offset * 4 + 2] = (int)(b * 255);
    ptr[offset * 4 + 3] = 255;
}


int main() {
    CPUBitmap bitmap(DIM, DIM);
    unsigned char* dev_ptr;

    cudaEvent_t start, stop;
    HANDLE_ERROR(cudaEventCreate(&start));  // ����һ���¼�
    HANDLE_ERROR(cudaEventCreate(&stop));
    HANDLE_ERROR(cudaEventRecord(start, 0));  // ��¼һ���¼�


    HANDLE_ERROR(cudaMalloc((void**)&dev_ptr, bitmap.image_size()));

    Sphere* spheres = (Sphere*)malloc(SPHERES * sizeof(Sphere));
    for (int i = 0; i < SPHERES; i++) {
        spheres[i].r = rnd(1.0f);
        spheres[i].g = rnd(1.0f);
        spheres[i].b = rnd(1.0f);
        spheres[i].x = rnd(1000.0f) - 500;
        spheres[i].y = rnd(1000.0f) - 500;
        spheres[i].z = rnd(1000.0f) - 500;
        spheres[i].radius = rnd(100.0f) + 20;
    }
    HANDLE_ERROR(cudaMemcpyToSymbol(dev_s, spheres, SPHERES * sizeof(Sphere)));
    free(spheres);

    dim3 blocks(DIM / 16, DIM / 16);
    dim3 threads(16, 16);
    kernel<<<blocks, threads>>>(dev_ptr);

    HANDLE_ERROR(cudaMemcpy(bitmap.get_ptr(), dev_ptr,
                            bitmap.image_size(),
                            cudaMemcpyDeviceToHost));
    HANDLE_ERROR(cudaEventRecord(stop, 0));
    HANDLE_ERROR(cudaEventSynchronize(stop));

    float elapsedTime;
    HANDLE_ERROR(cudaEventElapsedTime(&elapsedTime, start, stop));
    printf("Time to generate:  %3.1f ms\n", elapsedTime);

    HANDLE_ERROR(cudaEventDestroy(start));
    HANDLE_ERROR(cudaEventDestroy(stop));

    bitmap.display_and_exit();

    HANDLE_ERROR(cudaFree(dev_ptr));

    return 0;
}
```

这是一个光线追踪的例子，其中sphere就是一个常量内存。

# 事件

CUDA事件本质上是GPU时间戳，用来在用户指定的时间点上记录。

cudaEventCreate/Record/Synchronise这些API。

cudaEventSynchronize用来让CPU在某个时间和GPU同步。

一个Event使用结束后，需要用cudaEventDestroy来销毁。

# 纹理内存
