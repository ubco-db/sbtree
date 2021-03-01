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


/**
 * Runs all tests and collects benchmarks
 */ 
void runalltests_sbtree()
{
    int8_t M = 2;    
    int32_t numRecords = 1000000;
   
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
    buffer->activePath = state->activePath;

    state->tempKey = malloc(sizeof(int32_t)); 
    int8_t* recordBuffer = malloc(state->recordSize);

    /* Setup output file. TODO: Will replace with direct memory access. */
    FILE *fp;
    fp = fopen("myfile.bin", "w+b");
    if (NULL == fp) {
        printf("Error: Can't open file!\n");
        return;
    }
    
    buffer->file = fp;

    state->parameters = 0;    
    state->buffer = buffer;

    /* Initialize SBTree structure with parameters */
    sbtreeInit(state);

       /* Data record is empty. Only need to reset to 0 once as reusing struct. */    
    int32_t i;
    for (i = 0; i < state->recordSize-4; i++) // 4 is the size of the key
    {
        recordBuffer[i + sizeof(int32_t)] = 0;
    }

    clock_t start = clock();

    /* Insert records into structure */    
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
 
    sbtreeFlush(state);    

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
        else if (*((int32_t*) recordBuffer) != key)
        {   printf("ERROR: Wrong data for: %d\n", key);
            printf("Key: %d Data: %d\n", key, *((int32_t*) recordBuffer));
        }
    }

    printStats(buffer);

    /* Below minimum key search */
    int32_t key = -1;
    int8_t result = sbtreeGet(state, &key, recordBuffer);
    if (result == 0) 
        printf("Error1: Key found: %d\n", key);

    /* Above maximum key search */
    key = 3500000;
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

    if (success && i == (v-mv+1))
        printf("SUCCESS\n");
    else
        printf("FAILURE\n");    
    
    printStats(buffer);

    /* Perform various queries to test performance */
    closeBuffer(buffer);    
    
    free(state->buffer->buffer);
}

/**
 * Main function to run tests
 */ 
void main()
{
	runalltests_sbtree();
}  