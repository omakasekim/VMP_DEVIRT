#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

int initialized = 0;
int uninitialized;

int *heap_area;
int *mmap_area;

int *stack_func_ptr;
int *stack_main_ptr;

void func() {
    int stack_func[1024];

    printf("stack_func addr : %lx\n", (uint64_t)stack_func);
    printf("stack_main addr : %lx\n", (uint64_t)stack_main_ptr);

    __asm__ __volatile__ ("movaps %xmm2, %xmm3\n");
    stack_func[0] = 0x11111111;
    stack_main_ptr[0] = 0x22222222;

    initialized = 0x33333333;
    uninitialized = 0x44444444;

    heap_area[0] = 0x55555555;
    mmap_area[0] = 0x66666666;

    __asm__ __volatile__ ("movaps %xmm3, %xmm2\n");

}

int main() {
    int stack_main[1024];

    stack_main_ptr = stack_main;

    heap_area = (int *)malloc(sizeof(int) * 16);
    mmap_area = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    printf("initialized : %lx\n", (uint64_t)&initialized);
    printf("uninitialized : %lx\n", (uint64_t)&uninitialized);
    printf("heap_area : %lx\n", (uint64_t)heap_area);
    printf("mmap_area : %lx\n", (uint64_t)mmap_area);

    printf("before func\n");
    __asm__ __volatile__ ("movaps %xmm0, %xmm0\n");

    func();

    __asm__ __volatile__ ("movaps %xmm2, %xmm2\n");
    printf("after func\n");

}


