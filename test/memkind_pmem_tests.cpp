/*
 * Copyright (C) 2015 - 2017 Intel Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice(s),
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice(s),
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <memkind/internal/memkind_pmem.h>
#include <memkind/internal/memkind_private.h>
#include "allocator_perf_tool/TimerSysTime.hpp"

#include <sys/param.h>
#include <sys/mman.h>
#include <stdio.h>
#include "common.h"
#include <pthread.h>

static const size_t PMEM_PART_SIZE = MEMKIND_PMEM_MIN_SIZE + 4096;
static const char*  PMEM_DIR = "/tmp/";

class MemkindPmemTests: public :: testing::Test
{

protected:
    memkind_t pmem_kind;
    void SetUp()
    {
        // create PMEM partition
        int err = memkind_create_pmem(PMEM_DIR, PMEM_PART_SIZE, &pmem_kind);
        ASSERT_EQ(0, err);
        ASSERT_TRUE(NULL != pmem_kind);
    }

    void TearDown()
    {}
};

class MemkindPmemCallocTests : public MemkindPmemTests,
                             public ::testing::WithParamInterface<std::tuple<int, int>> {
};

class MemkindPmemMallocTimeTests : public MemkindPmemTests,
                             public ::testing::WithParamInterface<std::tuple<int, int>> {
};

class MemkindPmemMallocTests : public MemkindPmemTests,
                             public ::testing::WithParamInterface<size_t> {
};

class MemkindPmemAlignmentTests : public MemkindPmemTests,
                             public ::testing::WithParamInterface<size_t> {
};

static void pmem_get_size(struct memkind *kind, size_t& total, size_t& free)
{
    struct memkind_pmem *priv = reinterpret_cast<struct memkind_pmem *>(kind->priv);

    total = priv->max_size;
    free = priv->max_size - priv->offset; /* rough estimation */
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemPriv)
{
    size_t total_mem = 0;
    size_t free_mem = 0;

    pmem_get_size(pmem_kind, total_mem, free_mem);

    ASSERT_TRUE(total_mem != 0);
    ASSERT_TRUE(free_mem != 0);

    EXPECT_EQ(total_mem, roundup(PMEM_PART_SIZE, MEMKIND_PMEM_CHUNK_SIZE));

    size_t offset = total_mem - free_mem;
    EXPECT_LT(offset, MEMKIND_PMEM_CHUNK_SIZE);
    EXPECT_LT(offset, total_mem);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemMalloc)
{
    const size_t size = 1024;
    char *default_str = NULL;

    default_str = (char *)memkind_malloc(pmem_kind, size);
    EXPECT_TRUE(NULL != default_str);

    sprintf(default_str, "memkind_malloc MEMKIND_PMEM\n");
    printf("%s", default_str);

    memkind_free(pmem_kind, default_str);

    // Out of memory
    default_str = (char *)memkind_malloc(pmem_kind, 2 * PMEM_PART_SIZE);
    EXPECT_EQ(NULL, default_str);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemMalloc0)
{
    const size_t size0 = 0;
    const size_t size1 = -1;
    void *test1 = NULL;

    test1 = memkind_malloc(pmem_kind, size0);
    ASSERT_TRUE(test1 == NULL);

    test1 = memkind_malloc(pmem_kind, size1);
    ASSERT_TRUE(test1 == NULL);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemCalloc)
{
    const size_t size = 1024;
    const size_t num = 1;
    char *default_str = NULL;

    default_str = (char *)memkind_calloc(pmem_kind, num, size);
    EXPECT_TRUE(NULL != default_str);
    EXPECT_EQ(*default_str, 0);

    sprintf(default_str, "memkind_calloc MEMKIND_PMEM\n");
    printf("%s", default_str);

    memkind_free(pmem_kind, default_str);

    // allocate the buffer of the same size (likely at the same address)
    default_str = (char *)memkind_calloc(pmem_kind, num, size);
    EXPECT_TRUE(NULL != default_str);
    EXPECT_EQ(*default_str, 0);

    sprintf(default_str, "memkind_calloc MEMKIND_PMEM\n");
    printf("%s", default_str);

    memkind_free(pmem_kind, default_str);
}

TEST_P(MemkindPmemCallocTests, test_TC_MEMKIND_PmemCallocSize)
{
    void *test = NULL;

    test = memkind_calloc(pmem_kind, std::get<0>(GetParam()), std::get<1>(GetParam()));
    ASSERT_TRUE(test == NULL);
}

INSTANTIATE_TEST_CASE_P(
    CallocParam, MemkindPmemCallocTests,
    ::testing::Values(std::make_tuple(10, 0),
                      std::make_tuple(0, 0),
                      std::make_tuple(-1, 1),
                      std::make_tuple(10, -1)));

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemCallocHuge)
{
    const size_t size = MEMKIND_PMEM_CHUNK_SIZE;
    const size_t num = 1;
    char *default_str = NULL;

    default_str = (char *)memkind_calloc(pmem_kind, num, size);
    EXPECT_TRUE(NULL != default_str);
    EXPECT_EQ(*default_str, 0);

    sprintf(default_str, "memkind_calloc MEMKIND_PMEM\n");
    printf("%s", default_str);

    memkind_free(pmem_kind, default_str);

    // allocate the buffer of the same size (likely at the same address)
    default_str = (char *)memkind_calloc(pmem_kind, num, size);
    EXPECT_TRUE(NULL != default_str);
    EXPECT_EQ(*default_str, 0);

    sprintf(default_str, "memkind_calloc MEMKIND_PMEM\n");
    printf("%s", default_str);

    memkind_free(pmem_kind, default_str);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemRealloc)
{
    const size_t size1 = 512;
    const size_t size2 = 1024;
    char *default_str = NULL;

    default_str = (char *)memkind_realloc(pmem_kind, default_str, size1);
    EXPECT_TRUE(NULL != default_str);

    sprintf(default_str, "memkind_realloc MEMKIND_PMEM with size %zu\n", size1);
    printf("%s", default_str);

    default_str = (char *)memkind_realloc(pmem_kind, default_str, size2);
    EXPECT_TRUE(NULL != default_str);

    sprintf(default_str, "memkind_realloc MEMKIND_PMEM with size %zu\n", size2);
    printf("%s", default_str);

    memkind_free(pmem_kind, default_str);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemReallocSize)
{
    size_t size = 1024;
    void *test1 = NULL;
    void *test2 = NULL;

    test1 = memkind_malloc(pmem_kind, size);
    ASSERT_TRUE(test1 != NULL);

    test2 = memkind_malloc(pmem_kind, size);
    ASSERT_TRUE(test2 != NULL);

    size = 0;
    test1 = memkind_realloc(pmem_kind, test1, size);
    ASSERT_TRUE(test1 == NULL);

    size = -1;
    test2 = memkind_realloc(pmem_kind, test2, size);
    ASSERT_TRUE(test2 == NULL);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemReallocValue)
{
    size_t size = 1024;
    char *test1 = NULL;
    char *test2 = NULL;

    test1 = (char*)memkind_malloc(pmem_kind, size);
    ASSERT_TRUE(test1 != NULL);

    sprintf(test1, "test_TC_MEMKIND_PmemReallocValue");

    size *= 2;
    test2 = (char*)memkind_realloc(pmem_kind, test1, size);
    ASSERT_TRUE(test2 != NULL);
    ASSERT_EQ(*test1, *test2);

    memkind_free(pmem_kind, test1);
    memkind_free(pmem_kind, test2);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemReallocInPlace)
{
    void *test1 = memkind_malloc(pmem_kind, 12 * 1024 * 1024);
    ASSERT_TRUE(test1 != NULL);

    void *test1r = memkind_realloc(pmem_kind, test1, 6 * 1024 * 1024);
    ASSERT_EQ(test1r, test1);

    test1r = memkind_realloc(pmem_kind, test1, 12 * 1024 * 1024);
    ASSERT_EQ(test1r, test1);

    test1r = memkind_realloc(pmem_kind, test1, 8 * 1024 * 1024);
    ASSERT_EQ(test1r, test1);

    void *test2 = memkind_malloc(pmem_kind, 2 * 1024 * 1024);
    ASSERT_TRUE(test2 != NULL);

    /* 2MB => 16B */
    void *test2r = memkind_realloc(pmem_kind, test2, 16);
    ASSERT_TRUE(test2r != NULL);
    /* ... but the usable size is still 4MB. */

    /* 8MB => 16B */
    test1r = memkind_realloc(pmem_kind, test1, 16);
    /*
     * If the old size of the allocation is larger than
     * the chunk size (2MB), we can reallocate it to 2MB first (in place),
     * releasing some space, which makes it possible to do the actual
     * shrinking...
     */
    ASSERT_TRUE(test1r != NULL);
    ASSERT_NE(test1r, test1);

    /* ... and leaves some memory for new allocations. */
    void *test3 = memkind_malloc(pmem_kind, 3 * 1024 * 1024);
    ASSERT_TRUE(test3 != NULL);

    memkind_free(pmem_kind, test1r);
    memkind_free(pmem_kind, test2r);
    memkind_free(pmem_kind, test3);
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemMaxFill)
{
    void *test1 = NULL;
    int i=16800000;
    do {
        test1 = memkind_malloc(pmem_kind, --i);
    } while (test1 == NULL && i>0);
    ASSERT_NE(i, 0);

    void *test2 = NULL;
    int j=2000000;
    do {
        test2 = memkind_malloc(pmem_kind, --j);
    } while (test2 == NULL && j>0);
    ASSERT_NE(j, 0);

    void *test3 = NULL;
    int l=100000;
    do {
        test2 = memkind_malloc(pmem_kind, --l);
    } while (test3 == NULL && l>0);
    ASSERT_EQ(l, 0);

    memkind_free(pmem_kind, test1);
    memkind_free(pmem_kind, test2);
    memkind_free(pmem_kind, test3);
}

TEST_P(MemkindPmemMallocTimeTests, test_TC_MEMKIND_PmemMallocTime)
{
    size_t size;
    void *test = NULL;

    srand(time(0));
    TimerSysTime timer;
    timer.start();
    do {
        size = rand() % (std::get<0>(GetParam()) + 1 - std::get<1>(GetParam())) + std::get<1>(GetParam());
        test = memkind_malloc(pmem_kind, size);
        ASSERT_TRUE(test != NULL);
        memkind_free(pmem_kind, test);
    } while(timer.getElapsedTime() < 1*5);
}

INSTANTIATE_TEST_CASE_P(
    MallocTimeParam, MemkindPmemMallocTimeTests,
    ::testing::Values(std::make_tuple(32, 32),
                      std::make_tuple(896, 896),
                      std::make_tuple(4096, 4096),
                      std::make_tuple(131072, 131072),
                      std::make_tuple(2*1024*1024, 2*1024*1024),
                      std::make_tuple(5*1024*1024, 5*1024*1024),
                      std::make_tuple(32, 4096),
                      std::make_tuple(4096, 98304),
                      std::make_tuple(114688, 196608),
                      std::make_tuple(500000, 2*1024*1024),
                      std::make_tuple(2*1024*1024, 4*1024*1024),
                      std::make_tuple(5*1024*1024, 6*1024*1024),
                      std::make_tuple(5*1024*1024, 8*1024*1024),
                      std::make_tuple(32, 9*1024*1024)));

TEST_P(MemkindPmemMallocTests, test_TC_MEMKIND_PmemMallocSize)
{
    void *test[1000000];
    int i, max;

    for(int j=0; j<10; j++)
    {
        i =0;
        do{
            test[i] = memkind_malloc(pmem_kind, GetParam());
        } while(test[i] != 0 && i++<1000000);

        if(j == 0)
            max = i;
        else
            ASSERT_TRUE(i > 0.98*max);

        while(i > 0)
        {
            memkind_free(pmem_kind, test[--i]);
            test[i] = 0;
        }
    }
}

INSTANTIATE_TEST_CASE_P(
    MallocParam, MemkindPmemMallocTests,
    ::testing::Values(32, 60, 80, 100, 128, 150, 160, 250, 256, 300, 320,
                        500, 512, 800, 896, 3000, 4096, 6000, 10000, 60000,
                        98304, 114688, 131072, 163840, 196608, 500000,
                        2*1024*1024, 5*1024*1024));

TEST_P(MemkindPmemAlignmentTests, test_TC_MEMKIND_PmemAlignment)
{
    size_t alignment;
    void *test[1000000];
    int i, max, ret;

    for(alignment = 1024; alignment < 140000; alignment *= 2)
    {
        if(GetParam() > alignment)
            continue;
        for(int j=0; j<10; j++)
        {
            i =0;
            do{
                ret = memkind_posix_memalign(pmem_kind, &test[i], alignment, GetParam());
            } while(ret == 0 && i++<1000000);
            printf("%d ", i);
            if(j == 0)
                max = i;
            else
                ASSERT_TRUE(i > 0.98*max);

            while(i > 0)
            {
                memkind_free(pmem_kind, test[--i]);
                test[i] = 0;
            }
        }
    }
}

INSTANTIATE_TEST_CASE_P(
    AlignmentParam, MemkindPmemAlignmentTests,
    ::testing::Values(32, 60, 80, 100, 128, 150, 160, 250, 256, 300, 320,
                        500, 512, 800, 896, 3000, 4096, 6000, 10000, 60000,
                        98304, 114688));

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemPosixMemalign)
{
    const int max_allocs = 1000000;
    const int test_value = 123456;
    uintptr_t alignment;
    unsigned i;
    int *ptrs[max_allocs];
    void *ptr;
    int ret;

    for(alignment = 1024; alignment < 140000; alignment *= 2)
    {
        for (int j = 0; j < 10; j++)
        {
            memset(ptrs, 0, max_allocs * sizeof(ptrs[0]));
            for (i = 0; i < max_allocs; ++i)
            {
                errno = 0;
                ret = memkind_posix_memalign(pmem_kind, &ptr, alignment, sizeof(int *));
                if(ret != 0)
                    break;

                EXPECT_EQ(ret, 0);
                EXPECT_EQ(errno, 0);

                ptrs[i] = (int *)ptr;

                //at least one allocation must succeed
                ASSERT_TRUE(i != 0 || ptr != nullptr);
                if (ptr == nullptr)
                    break;

                //ptr should be usable
                *(int*)ptr = test_value;
                ASSERT_EQ(*(int*)ptr, test_value);

                //check for correct address alignment
                ASSERT_EQ((uintptr_t)(ptr) & (alignment - 1), (uintptr_t)0);
            }

            for (i = 0; i < max_allocs; ++i)
            {
                if (ptrs[i] == nullptr)
                break;

                memkind_free(pmem_kind, ptrs[i]);
            }
        }
    }
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemKinds)
{
    memkind_t pmem_kind[100];
    size_t size = 32;
    void *test = NULL;

    for(int i=0; i<100; i++)
    {
        int err = memkind_create_pmem(PMEM_DIR, PMEM_PART_SIZE, &pmem_kind[i]);
        ASSERT_EQ(0, err);
        ASSERT_TRUE(NULL != pmem_kind[i]);
        test = memkind_malloc(pmem_kind[i], size);
        ASSERT_TRUE(test != NULL);
        memkind_free(pmem_kind[i], test);
    }
}

static memkind_t *pools;
static int npools = 3;
static void* thread_func(void* arg)
{
    int start_idx = *(int *)arg;
    int err = 0;
    for (int idx = 0; idx < npools; ++idx)
    {
        int pool_id = start_idx + idx;

        if (pools[pool_id] == NULL) {
            err = memkind_create_pmem(PMEM_DIR, PMEM_PART_SIZE, &pools[pool_id]);
            EXPECT_EQ(0, err);
        }

        void *test = memkind_malloc(pools[pool_id], sizeof(void *)); 
        EXPECT_TRUE(test != NULL);
        memkind_free(pools[pool_id], test);
    }

    return NULL;
}

TEST_F(MemkindPmemTests, test_TC_MEMKIND_PmemMultithreads)
{
    int nthreads = 10;
    pthread_t *threads = (pthread_t*)calloc(nthreads, sizeof(pthread_t));
    ASSERT_TRUE(threads != NULL);
    int *pool_idx = (int*)calloc(nthreads, sizeof(int));
    ASSERT_TRUE(pool_idx != NULL);
    pools = (memkind_t*)calloc(npools * nthreads, sizeof(memkind_t));
    ASSERT_TRUE(pools != NULL);

    for (int t = 0; t < nthreads; t++)
    {
        pool_idx[t] = npools * t;
        pthread_create(&threads[t], NULL, thread_func, &pool_idx[t]);
    }

    for (int t = 0; t < nthreads; t++)
    {
        pthread_join(threads[t], NULL);
    }

    free(pools);
    free(threads);
    free(pool_idx);
}
