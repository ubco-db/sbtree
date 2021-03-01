/******************************************************************************/
/**
@file		test_sbtree.c
@author		Ramon Lawrence
@brief		This file does performance/correctness testing of sequential bitmap 
            indexing for time series (SBTree).
@copyright	Copyright 2021
			The University of British Columbia,		
@par Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

@par 1.Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

@par 2.Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

@par 3.Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

@par THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
/******************************************************************************/
#include <time.h>
#include <string.h>

#include "sbtree.h"


#define EXTERNAL_SORT_MAX_RAND 1000000

/* A bitmap with 8 buckets (bits). Range 0 to 100. */
void updateBitmapInt8Bucket(void *data, void *bm)
{
    // Note: Assuming int key is right at the start of the data record
    int32_t val = *((int16_t*) data);
    uint8_t* bmval = (int8_t*) bm;

    if (val < 10)
        *bmval = *bmval | 128;        
    else if (val < 20)
        *bmval = *bmval | 64;        
    else if (val < 30)
        *bmval = *bmval | 32;        
    else if (val < 40)
        *bmval = *bmval | 16;        
    else if (val < 50)
        *bmval = *bmval | 8;        
    else if (val < 60)
        *bmval = *bmval | 4;            
    else if (val < 100)
        *bmval = *bmval | 2;        
    else 
        *bmval = *bmval | 1;        
}	

/* A bitmap with 8 buckets (bits). Range 0 to 100. Build bitmap based on min and max value. */
void buildBitmapInt8BucketWithRange(void *min, void *max, void *bm)
{
    /* Note: Assuming int key is right at the start of the data record */
    int32_t val = *((int16_t*) min);
    uint8_t* bmval = (int8_t*) bm;

    if (min == NULL && max == NULL)
    {
        *bmval = 255;  /* Everything */
    }
    else
    {
        int8_t i = 0;
        uint8_t val = 128;
        if (min != NULL)
        {
            /* Set bits based on min value */
            updateBitmapInt8Bucket(min, bm);

            /* Assume here that bits are set in increasing order based on smallest value */                        
            /* Find first set bit */
            while ( (val & *bmval) == 0 && i < 8)
            {
                i++;
                val = val / 2;
            }
            val = val / 2;
            i++;
        }
        if (max != NULL)
        {
            /* Set bits based on min value */
            updateBitmapInt8Bucket(max, bm);

            while ( (val & *bmval) == 0 && i < 8)
            {
                i++;
                *bmval = *bmval + val;
                val = val / 2;                 
            }
        }
        else
        {
            while (i < 8)
            {
                i++;
                *bmval = *bmval + val;
                val = val / 2;
            }
        }        
    }        
}	

int8_t inBitmapInt8Bucket(void *data, void *bm)
{
    int32_t val = *((int16_t*) data);
    uint8_t* bmval = (int8_t*) bm;

    uint8_t tmpbm = 0;
    updateBitmapInt8Bucket(data, &tmpbm);

    // Return a number great than 1 if there is an overlap
    return tmpbm & *bmval;
}

int8_t int32Comparator(
        void			*a,
        void			*b
) {
	int32_t result = *((int32_t*)a) - *((int32_t*)b);
	if(result < 0) return -1;
	if(result > 0) return 1;
    return 0;
}

/**
 * Runs all tests and collects benchmarks
 */ 
void runalltests_sbtree()
{
    int8_t M = 8;    
    int32_t numRecords = 10000;
   
    /* Configure buffer */
    dbbuffer* buffer = malloc(sizeof(dbbuffer));
    buffer->pageSize = 512;
    buffer->numPages = M;
    buffer->status = malloc(sizeof(id_t)*M);
    buffer->buffer  = malloc((size_t) buffer->numPages * buffer->pageSize);   

    /* Configure SBTree state */
    sbtreeState* state = malloc(sizeof(sbtreeState));

    state->recordSize = 16;
    state->keySize = 4;
    state->dataSize = 12;
    state->pageSize = 512;
    state->bufferSizeInBlocks = M;
    buffer->activePath = state->activePath;

    // state->buffer  = malloc((size_t) state->bufferSizeInBlocks * state->pageSize);   
    state->tempKey = malloc(sizeof(int32_t)); 
    int8_t* recordBuffer = malloc(state->recordSize);

    /* Setup output file. TODO: Will replace with direct memory access. */
    FILE *fp;
    fp = fopen("myfile.bin", "w+b");
    if (NULL == fp) {
        printf("Error: Can't open file!\n");
        return;
    }
    state->file = fp;
    buffer->file = fp;

   // state->parameters = SBTREE_USE_INDEX | SBTREE_USE_BMAP;
     state->parameters = 0;

    /* TODO: Setup for data and bitmap comparison functions */
    state->inBitmap = inBitmapInt8Bucket;
    state->updateBitmap = updateBitmapInt8Bucket;
    state->compareKey = int32Comparator;
    state->buildBitmap = buildBitmapInt8BucketWithRange;
    state->buffer = buffer;

    /* Initialize SBTree structure with parameters */
    sbtreeInit(state);

    /* Insert records into structure */    

    /* Data record is empty. Only need to reset to 0 once as reusing struct. */    
    int32_t i;
    for (i = 0; i < state->recordSize-4; i++) // 4 is the size of the key
    {
        recordBuffer[i + sizeof(int32_t)] = 0;
    }

    int32_t sum = 0;
    int32_t max = INT16_MIN;
    int32_t min = INT16_MAX;

    int16_t sumErr = 0;
    int16_t maxErr = 0;
    int16_t minErr = 0;

    clock_t start = clock();

    for (i = 0; i < numRecords; i++)
    {        
        *((int32_t*) recordBuffer) = i;
        *((int32_t*) (recordBuffer+4)) = i;
     
        sbtreePut(state, recordBuffer, (void*) (recordBuffer + 4));    
        
        if (i % 10 == 0)
        {
   //         printf("KEY: %d\n",i);
    //        sbtreePrint(state);   
        }
        
    }    
 
    int16_t minMaxSumError = sumErr + maxErr + minErr;
    printf("Errors: min/max/sum: %d\n", minMaxSumError);    

    /* Verify stored all records successfully */
    sbtreeFlush(state);
    fflush(state->file);

    clock_t end = clock();
    printf("Elapsed Time: %0.6f s\n", ((double) (end - start)) / CLOCKS_PER_SEC);
    printf("Records inserted: %d\n", numRecords);

    // sbtreePrint(state);

    printStats(buffer);

    /* Verify that all values can be found in tree */
    for (i = 0; i < numRecords; i++)    
    { 
        int32_t key = i;
        int8_t result = sbtreeGet(state, &key, recordBuffer);
        if (result != 0) 
            printf("ERROR: Failed to find: %d\n", key);
        if (*((int32_t*) recordBuffer) != key)
        {   printf("ERROR: Wrong data for: %d\n", key);
            printf("Key: %d Data: %d\n", key, *((int32_t*) recordBuffer));
        }
    }

    printStats(buffer);

    int32_t key = -1;
    int8_t result = sbtreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error1: Key found: %d\n", key);

    key = 350000;
    result = sbtreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error2: Key found: %d\n", key);

    printf("Complete");
    free(recordBuffer);
    
    sbtreeIterator it;
    int mv = 40;     // For all records, select mv = 1.
    it.minKey = &mv;
    int v = 299;
    it.maxKey = &v;
    it.minTime = NULL;
    it.maxTime = NULL;
    void *data;

    sbtreeInitIterator(state, &it);
    i = 0;
    int8_t success = 1;    
    int32_t *itKey, *itData;

    while (sbtreeNext(state, &it, (void*) &itKey, (void*) &itData))
    {                      
       // printf("Key: %d  Data: %d\n", *itKey, *itData);
        if (i+mv != *itKey)
        {   success = 0;
            printf("Key: %d Error\n", *itKey);
        }
        i++;        
    }
    printf("Read records: %d\n", i);

    if (success && i == (v-mv+1) && !minMaxSumError)
        printf("SUCCESS\n");
    else
        printf("FAILURE\n");    
    
    printStats(buffer);

    /* Perform various queries to test performance */
    closeBuffer(buffer);
    fclose(state->file);
    
    free(state->buffer->buffer);
}

/**
 * Main function to run tests
 */ 
void main()
{
	runalltests_sbtree();
}  