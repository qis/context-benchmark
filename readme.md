# Context Benchmark
This is a benchmark of possible context/schedule implementations specifically for the usage with coroutines.

## Results

Windows 10 1803, Core i7-2600 @ 3.40 GHz

```
-----------------------------------------------------------------------
Benchmark                                Time           CPU Iterations
-----------------------------------------------------------------------
asio/threads:1                        5524 ns       2278 ns     528168
asio_single/threads:1                 1345 ns       1350 ns     497778
asio_single_post/threads:1            1346 ns       1350 ns     497778
asio_random/threads:1                 5668 ns        500 ns    1000000
asio_random_post/threads:1            5633 ns        719 ns    1000000
mutex/threads:1                        263 ns         97 ns   10000000
mutex_single/threads:1                   8 ns          8 ns   74666667
mutex_single_post/threads:1             62 ns         61 ns   11200000
mutex_random/threads:1                  45 ns         14 ns  100000000
mutex_random_post/threads:1             63 ns         32 ns  100000000
spinlock/threads:1                     272 ns          0 ns   10000000
spinlock_single/threads:1                8 ns          8 ns   74666667
spinlock_single_post/threads:1          34 ns         34 ns   21333333
spinlock_random/threads:1                8 ns          3 ns 1000000000
spinlock_random_post/threads:1         115 ns         66 ns  100000000
iocp/threads:1                        5531 ns          0 ns    1000000
iocp_single/threads:1                   15 ns         14 ns   44800000
iocp_single_post/threads:1            1488 ns       1507 ns     497778
iocp_random/threads:1                   15 ns          8 ns 1000000000
iocp_random_post/threads:1            5447 ns          0 ns    1000000
semaphore/threads:1                   5156 ns          0 ns    1000000
semaphore_single/threads:1               8 ns          8 ns   89600000
semaphore_single_post/threads:1       1482 ns       1465 ns     448000
semaphore_random/threads:1               8 ns          0 ns 1000000000
semaphore_random_post/threads:1       1117 ns        288 ns   10000000
wake/threads:1                         747 ns          0 ns   10000000
wake_single/threads:1                   18 ns         18 ns   40727273
wake_single_post/threads:1              93 ns         92 ns    7466667
wake_random/threads:1                   17 ns          7 ns 1000000000
wake_random_post/threads:1             389 ns          0 ns   10000000
```

Windows 10 1803 WSL, Core i7-2600 @ 3.40 GHz

```
----------------------------------------------------------------------
Benchmark                               Time           CPU Iterations
----------------------------------------------------------------------
asio/threads:1                       6859 ns       2916 ns     235789
asio_single/threads:1                  39 ns         39 ns   17920000
asio_single_post/threads:1             39 ns         38 ns   17920000
asio_random/threads:1                 289 ns          0 ns   10000000
asio_random_post/threads:1            283 ns          5 ns   10000000
mutex/threads:1                       153 ns        130 ns  100000000
mutex_single/threads:1                  3 ns          3 ns  213333333
mutex_single_post/threads:1            50 ns         50 ns   10000000
mutex_random/threads:1                 30 ns         26 ns 1000000000
mutex_random_post/threads:1           427 ns         30 ns   10000000
spinlock/threads:1                    146 ns        107 ns  100000000
spinlock_single/threads:1               3 ns          3 ns  213333333
spinlock_single_post/threads:1         20 ns         19 ns   34461538
spinlock_random/threads:1              31 ns          0 ns  100000000
spinlock_random_post/threads:1        113 ns         54 ns  100000000
epoll/threads:1                      7356 ns          0 ns    1000000
epoll_single/threads:1                  4 ns          4 ns  194782609
epoll_single_post/threads:1          3834 ns       3836 ns     179200
epoll_random/threads:1         ERROR OCCURRED: 'concurrent run not supported'
epoll_random_post/threads:1    ERROR OCCURRED: 'concurrent run not supported'
futex/threads:1                      5544 ns          0 ns    1000000
futex_single/threads:1                  4 ns          4 ns  203636364
futex_single_post/threads:1           917 ns        921 ns     746667
futex_random/threads:1                  4 ns          0 ns 1000000000
futex_random_post/threads:1           521 ns          0 ns   10000000
```

<!--
FreeBSD 12, Intel Xeon E5-2660 v4 @ 2.00 GHz

```
----------------------------------------------------------------------
Benchmark                               Time           CPU Iterations
----------------------------------------------------------------------
```
-->
