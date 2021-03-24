/******************************************************************************/
/**
@file		test_sbtree.c
@author		Ramon Lawrence
@brief		This file does performance/correctness testing of sequential,
            copy-on-write B-tree (SBTree).
@copyright	Copyright 2021
			The University of British Columbia,
            Ramon Lawrence		
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
#include "fileStorage.h"
#include "memStorage.h"

/**
 * Test iterator
 */
void testIterator(sbtreeState *state)
{
    sbtreeIterator it;
    int mv = 40;     
    it.minKey = &mv;
    int v = 299;
    it.maxKey = &v;   
    void *data;

    sbtreeInitIterator(state, &it);
    uint32_t i = 0;
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
    
    printStats(state->buffer);
}

/**
 * Runs all tests and collects benchmarks
 */ 
void runalltests_sbtree()
{
    printf("\nSTARTING SEQUENTIAL B-TREE TESTS.\n");

    int8_t M = 5;    
    int32_t numRecords = 1000000;
    uint32_t numSteps = 10, stepSize = numRecords / numSteps;
    count_t r, numRuns = 3, l;
    uint32_t times[numSteps][numRuns];
    uint32_t reads[numSteps][numRuns];
    uint32_t writes[numSteps][numRuns];    
    uint32_t hits[numSteps][numRuns];    
    uint32_t rtimes[numSteps][numRuns];
    uint32_t rreads[numSteps][numRuns];
    uint32_t rhits[numSteps][numRuns];    
   
    for (r=0; r < numRuns; r++)
    {
        printf("\nRun: %d\n", (r+1));

        /* Configure file storage */        
        fileStorageState *storage = malloc(sizeof(fileStorageState));
        storage->fileName = "myfile.bin";
        if (fileStorageInit((storageState*) storage) != 0)
        {
            printf("Error: Cannot initialize storage!\n");
            return;
        }        

        /* Configure memory storage */
        /*
        memStorageState *storage = malloc(sizeof(memStorageState));        
        if (memStorageInit((storageState*) storage) != 0)
        {
            printf("Error: Cannot initialize storage!\n");
            return;
        }
        */
       
        /* Configure buffer */
        dbbuffer* buffer = malloc(sizeof(dbbuffer));
        buffer->pageSize = 512;
        buffer->numPages = M;
        buffer->status = malloc(sizeof(id_t)*M);
        buffer->buffer  = malloc((size_t) buffer->numPages * buffer->pageSize);   
        buffer->storage = (storageState*) storage;       

        /* Configure SBTree state */
        sbtreeState* state = malloc(sizeof(sbtreeState));

        state->recordSize = 16;
        state->keySize = 4;
        state->dataSize = 12;           
        state->buffer = buffer;

        state->tempKey = malloc(sizeof(int32_t)); 
        int8_t* recordBuffer = malloc(state->recordSize);

        /* Initialize SBTree structure */
        sbtreeInit(state);

        /* Initial contents of test record. */    
        int32_t i;
        for (i = 0; i < state->recordSize-4; i++) 
        {
            recordBuffer[i + sizeof(int32_t)] = 0;
        }

        printf("\nInsert test:\n");
        clock_t start = clock();

        /* Insert records into structure */    
        for (i = 0; i < numRecords; i++)
        {        
            *((int32_t*) recordBuffer) = i;
            *((int32_t*) (recordBuffer+4)) = i;
        
            sbtreePut(state, recordBuffer, (void*) (recordBuffer + 4));    
            
            if (i % stepSize == 0)
            {           
                printf("Num: %lu KEY: %lu\n", i, i);
                // btreePrint(state);               
                l = i / stepSize -1;
                if (l < numSteps && l >= 0)
                {
                    times[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
                    reads[l][r] = state->buffer->numReads;
                    writes[l][r] = state->buffer->numWrites;                    
                    hits[l][r] = state->buffer->bufferHits;                     
                }
            }       
        }    
    
        sbtreeFlush(state);    

        clock_t end = clock();
        l = numSteps-1;
        times[l][r] = (end-start)*1000/CLOCKS_PER_SEC;
        reads[l][r] = state->buffer->numReads;
        writes[l][r] = state->buffer->numWrites;        
        hits[l][r] = state->buffer->bufferHits; 
        printStats(state->buffer);

        printf("Elapsed Time: %lu ms\n", times[l][r]);
        printf("Records inserted: %d\n", numRecords);

        /* Clear stats */
        dbbufferClearStats(state->buffer);
        // sbtreePrint(state);        

        printf("\nQuery test:\n");
        start = clock();

        /* Query all values in tree */
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

            if (i % stepSize == 0)
            {                           
                // btreePrint(state);               
                l = i / stepSize - 1;
                if (l < numSteps && l >= 0)
                {
                    rtimes[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
                    rreads[l][r] = state->buffer->numReads;                    
                    rhits[l][r] = state->buffer->bufferHits;                     
                }
            }   
        }

        l = numSteps-1;       
        rtimes[l][r] = (clock()-start)*1000/CLOCKS_PER_SEC;
        rreads[l][r] = state->buffer->numReads;                    
        rhits[l][r] = state->buffer->bufferHits;   
        printStats(state->buffer);       
        printf("Elapsed Time: %lu ms\n", rtimes[l][r]);
        printf("Records queried: %lu\n", numRecords);  

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
        
        /* Optional: test iterator */
        testIterator(state);

        /* Clean up and free memory */
        closeBuffer(buffer);    
        
        free(state->tempKey);        
        free(recordBuffer);

        free(buffer->status);
        free(state->buffer->buffer);
        free(buffer);
        free(state);
    }

    // Prints results
    uint32_t sum;
    for (count_t i=1; i <= numSteps; i++)
    {
        printf("Stats for %lu:\n", i*stepSize);
    
        printf("Reads:   ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += reads[i-1][r];
            printf("\t%lu", reads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Writes: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += writes[i-1][r];
            printf("\t%lu", writes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);                       

        printf("Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += hits[i-1][r];
            printf("\t%lu", hits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        

        printf("Write Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += times[i-1][r];
            printf("\t%lu", times[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("R Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rtimes[i-1][r];
            printf("\t%lu", rtimes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Reads: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rreads[i-1][r];
            printf("\t%lu", rreads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rhits[i-1][r];
            printf("\t%lu", rhits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
    }
}

/**
 * Main function to run tests
 */ 
void main()
{
	runalltests_sbtree();
}  