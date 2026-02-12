#include <benchmark/benchmark.h>
#include <vector>
#include <cstdlib>
#include <immintrin.h>
#include <bitset>
#include "../include/filter.hpp"
#include "../include/packet.hpp"


//Normal code with branching
void filter_scalar(Packet* packets, int count) {
    for(int i = 0; i < count; i++) {
        //This stops the compiler from automatically optimizing this line.
        volatile bool is_admin = (packets[i].header & MASK_ADMIN);
        if (is_admin)
        {
            //Simulates work
            benchmark::DoNotOptimize(is_admin);
        }
    }
}

static void BM_Scalar(benchmark::State& state) {
    //Allocate 8 packets aligned to 32 bytes
    void* mem = std::aligned_alloc(32, sizeof(Packet) * 8);
    Packet* packets = static_cast<Packet*>(mem);
    //Flip the packets header to have admin mask and no admin mask
    for(int i=0; i<8; i++) packets[i].header = (i % 2) ? MASK_ADMIN : 0;

    for(auto _ : state) {
        filter_scalar(packets, 8);
        benchmark::ClobberMemory();//Force memory write
    }
    std::free(mem);
}

static void BM_AVX2(benchmark::State& state) {
    //Same as scalar for controlled testing
    void* mem = std::aligned_alloc(32, sizeof(Packet) * 8);
    Packet* packets = static_cast<Packet*>(mem);
    for(int i=0; i<8; i++) packets[i].header = (i % 2) ? MASK_ADMIN : 0;

    for(auto _: state) {
        uint8_t mask = filter_batch_8_avx2(packets);
        benchmark::DoNotOptimize(mask);
        benchmark::ClobberMemory();
    }
    std::free(mem);
}

BENCHMARK(BM_Scalar);
BENCHMARK(BM_AVX2);

BENCHMARK_MAIN();