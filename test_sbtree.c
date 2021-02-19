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
    int8_t M = 2;    
    int32_t numRecords = 1500;

    /* Configure SBTree state */
    sbtreeState* state = malloc(sizeof(sbtreeState));

    state->recordSize = 16;
    state->keySize = 4;
    state->dataSize = 12;
    state->pageSize = 512;
    state->bufferSizeInBlocks = M;
    state->buffer  = malloc((size_t) state->bufferSizeInBlocks * state->pageSize);    
    int8_t* recordBuffer = malloc(state->recordSize);

    /* Setup output file. TODO: Will replace with direct memory access. */
    FILE *fp;
    fp = fopen("myfile.bin", "w+b");
    if (NULL == fp) {
        printf("Error: Can't open file!\n");
        return;
    }
    state->file = fp;
    state->parameters = SBTREE_USE_INDEX | SBTREE_USE_BMAP;
    
    /* TODO: Setup for data and bitmap comparison functions */
    state->inBitmap = inBitmapInt8Bucket;
    state->updateBitmap = updateBitmapInt8Bucket;
    state->compareData = int32Comparator;
    state->buildBitmap = buildBitmapInt8BucketWithRange;

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
/*        
        // Check min/max just before reaching buffer fills up and is flushed
        if (i % state->maxRecordsPerPage == 0) {

         //   printf("Calculated Sum : Actual Sum: %d : %d\n", SBTREE_GET_SUM(state->buffer), sum);
            printf("Calculated Max : Actual Max: %d : %d\n", SBTREE_GET_MAX(state->buffer), max);
            printf("Calculated Min : Actual Min: %d : %d\n", SBTREE_GET_MIN(state->buffer), min);
            printf("End of buffer\n");

      //      if (SBTREE_GET_SUM(state->buffer) != sum) {
       //         sumErr = 1;
      //      }
            if (SBTREE_GET_MAX(state->buffer) != max) {
                maxErr = 10;
            }
            if (SBTREE_GET_MIN(state->buffer) != min) {
                minErr = 100;
            }
            min = *recordBuffer;
            sum = 0;
        }
        sum += *recordBuffer;
        max = *recordBuffer;
        */
        // sbtreeInsertRec(state, 1000000+i, recordBuffer);        
        sbtreeInsert(state, 1000000+i, recordBuffer, recordBuffer + 4);        
    }    

    int16_t minMaxSumError = sumErr + maxErr + minErr;
    printf("Errors: min/max/sum: %d\n", minMaxSumError);
    free(recordBuffer);

    /* Verify stored all records successfully */
    sbtreeFlush(state);
    fflush(state->file);

    clock_t end = clock();
    printf("Elapsed Time: %0.6f s\n", ((double) (end - start)) / CLOCKS_PER_SEC);
    printf("Records inserted: %d\n", numRecords);

    sbtreePrint(state);

    test_record_t *rec;
    sbtreeIterator it;
    int mv = 40;     // For all records, select mv = 1.
    it.minKey = &mv;
    int v = 59;
    it.maxKey = &v;
    it.minTime = NULL;
    it.maxTime = NULL;

    sbtreeInitIterator(state, &it);
    i = 0;
    int8_t success = 1;    
    while (sbtreeNext(state, &it, (void**) &recordBuffer))
    {        
        rec = (test_record_t*) recordBuffer;        
  //      printf("Key: %d\n", rec->key);
        if (i+mv != rec->key)
        {   success = 0;
            printf("Key: %d Error\n", rec->key);
        }
        i++;
    }
    printf("Read records: %d\n", i);
    // if (success && i == numRecords)
    if (success && i == (v-mv+1) && !minMaxSumError)
        printf("SUCCESS\n");
    else
        printf("FAILURE\n");    
    

    /* Perform various queries to test performance */


    fclose(state->file);
    
    free(state->buffer);
}

/**
 * Main function to run tests
 */ 
void main()
{
	runalltests_sbtree();
}  