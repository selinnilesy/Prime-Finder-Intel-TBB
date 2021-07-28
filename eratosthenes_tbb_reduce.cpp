//
// Created by Selin Yıldırım on 20.07.2021.
//
#include "oneapi/tbb.h"
#include <iostream>
#include <stdlib.h>     /* atoi */
#include <cstring>
#include <numeric>
#include <chrono>

using namespace oneapi::tbb;
using namespace std;

class SieveRange {
    int my_stride, my_grainsize;
    int my_begin,my_end;
    bool assert_okay() const {
        assert(my_begin % my_stride == 0);
        assert(my_begin <= my_end);
        assert(my_stride <= my_grainsize);
        return true;
    }
public:
    bool is_divisible() const { return my_end - my_begin > my_grainsize; }
    bool empty() const { return my_end <= my_begin
    ; }
    // Split r into subranges r and *this
    SieveRange( SieveRange& r, split ): my_stride(r.my_stride),
                                        my_grainsize(r.my_grainsize),
                                        my_end(r.my_end) {
        assert(r.is_divisible());
        assert(r.assert_okay());
        int middle = r.my_begin + (r.my_end - r.my_begin + r.my_stride - 1) / 2;
        middle = middle / my_stride * my_stride;
        my_begin = middle;
        r.my_end = middle;
        assert(assert_okay());
        assert(r.assert_okay());
    }
    int begin() const {
        return my_begin;
    }
    int end() const {
        return my_end;
    }
    SieveRange(int begin, int end, int stride, int grainsize)
            : my_begin(begin),
              my_end(end),
              my_stride(stride),
              my_grainsize(grainsize < stride ? stride : grainsize) {
        assert(assert_okay());
    }
};

class ApplySieve {
    bool* isPrime;
    const int k;
public:
    void operator()( const SieveRange& range ) const {
        bool *copyPrime = isPrime;
        int local_k = k;
        size_t i = range.begin();
        size_t end = range.end();
        //cout << "Range feedback: " << range.begin() << "-" << range.end() << endl;
        for( ; i<end;  i+=k ){
            copyPrime[i] = 0;
        }
    }
    ApplySieve(int k_init, bool* isPrimeInit) :  k(k_init), isPrime(isPrimeInit) {}
    ApplySieve( ApplySieve& x, split ) :  k(x.k), isPrime(x.isPrime) {}

    void join( const ApplySieve& y ) {}

};

inline void ParallelApplySieve( int* a, size_t n, bool* primes , int local_k) {

    ApplySieve obj(local_k, primes);
    //parallel_for(blocked_range<size_t>(0, n, 25000), obj, simple_partitioner()); // grain size
    oneapi::tbb::parallel_reduce(SieveRange(local_k*local_k, n, local_k, n/4), // begin end stride grain size
                                 obj,
                                 oneapi::tbb::simple_partitioner());
    //parallel_for(blocked_range<size_t>(0, n), obj);   // automatic chunking
    //parallel_for(blocked_range<size_t>(0, n), obj, ap);
}
std::chrono::duration<double> SerialSieve( size_t n , int k, bool silent) {
    const auto t1 = std::chrono::high_resolution_clock::now();
    int i,j, local_k = k;
    std::chrono::duration<double> seqTime;
    bool *primes = new bool[n];
    memset(primes, 1, n);                // all of them are prime(1) at the beginning.

    bool finish;
    primes[0] = primes[1] = 0;
    while(1) {
        for (j=local_k*local_k; j <= n; j+=local_k){
            primes[j] = 0;
        }
        finish = 1;
        for (i = local_k + 1; i < n; i++) { if (primes[i]) { local_k = i; finish = 0; break; } }    // update k to next prime number
        if (finish || local_k > sqrt(n)) { break; }  // no more primes can be found after this k
    }
    const auto t2 = std::chrono::high_resolution_clock::now();
    seqTime = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    if(!silent){
        for(i=0; i<n; i++) cout <<"Seq- Prime bool value of " << i << " is: " << primes[i] << endl;
    }
    delete [] primes;
    return seqTime;
}

int main(int argc, char** argv){ // ./a.out 1000 4 silent
    bool silent;
    int size = atoi(argv[1]);
    int threadCount = atoi(argv[2]);
    if(argv[3] && !strcmp(argv[3],"silent")) silent=1;

    /* data initializations */
    int *data = new int[size];
    for(int i=0; i<size; i++) data[i]=i;        // start with 0, edit at the end.

    cout << "Sequential time: " << SerialSieve( size , 2, silent).count() << endl;

    const auto t1 = std::chrono::high_resolution_clock::now();
    global_control c(global_control::max_allowed_parallelism,  threadCount);
    std::chrono::duration<double> parallelTime;
    bool *primes = new bool[size];
    memset(primes, 1, size);                // all of them are prime(1) at the beginning.

    int i, local_k = 2;                          // initial prime
    bool finish ;
    primes[0] = primes[1] = 0;

    /* multithreading */
    while(1){
        ParallelApplySieve( data, size, primes , local_k);                       // apply prime sieve for current k
        primes[local_k] = 1; // minor edit
        finish = 1;
        for(i = local_k+1; i<size; i++) { if (primes[i]) {local_k=i; finish=0; break; }} // update k to next prime numberextern atomic<int> UpperBound;
        if(finish || local_k > sqrt(size)) {break;}  // no more primes can be found after this k
    }
    // minor edits
    const auto t2 = std::chrono::high_resolution_clock::now();
    parallelTime = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

    primes[0] = primes[1] = 0;
    primes[2] = 1;
    cout << "Parallel time: " << parallelTime.count() << "secs" << endl;
    if(!silent){
       for(i=0; i<size; i++) cout <<"Prime bool value of " << i << " is: " << primes[i] << endl;
    }

    delete [] data;
    delete [] primes;
    primes = nullptr;
    data = nullptr;
    return 0;
}
