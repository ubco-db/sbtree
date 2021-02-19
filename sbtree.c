/******************************************************************************/
/**
@file		sbtree.c
@author		Ramon Lawrence
@brief		This file is for sequential B-tree implementation.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "sbtree.h"


// #define DEBUG  			1
// #define DEBUG_READ 		1
// #define DEBUG_WRITE		1

void initBufferPage(sbtreeState *state, int pageNum)
{
	/* Initialize page */
	int32_t i = 0;
	void *buf = state->buffer + pageNum * state->pageSize;
	for (i = 0; i < state->pageSize/sizeof(int32_t); i++)
    {
        ((int32_t*) buf)[i] = 0;
    }	

	/* Initialize header min/max, sum is already set to zero by the for-loop above */
	*(int16_t*)(buf + SBTREE_MAX_OFFSET) = INT16_MIN;
	*(int16_t*)(buf + SBTREE_MIN_OFFSET) = INT16_MAX;
}

/**
@brief     	Initialize an SBTree structure.
@param     	state
                SBTree algorithm state structure
*/
void sbtreeInit(sbtreeState *state)
{
	printf("Initializing SBTree.\n");
	printf("Buffer size: %d  Page size: %d\n", state->bufferSizeInBlocks, state->pageSize);	
	state->recordSize = state->keySize + state->dataSize;
	printf("Record size: %d\n", state->recordSize);
	printf("Use index: %d  Max/min: %d Bmap: %d\n", SBTREE_USING_INDEX(state->parameters), SBTREE_USING_MAX_MIN(state->parameters),
								SBTREE_USING_BMAP(state->parameters));

	/* TODO: These values would be set during recovery if database already exists. */
	state->nextPageId = 0;
	state->nextPageWriteId = 0;

	/* Calculate block header size */
	/* Header size fixed: 14 bytes: 4 byte id, 4 bytes start and end timestamp, 2 for record count. */
	/* 2 bytes each for Min/Max, 4 bytes for Sum */
	/* TODO: Header size variable: max/min and sum of data value (size dependent on data item), bitmap */
	state->headerSize = 23;
	state->bmOffset = 22; /* 1 byte offset. TODO: Remove this hard-code of location and size of bitmap */

	/* Calculate number of records per page */
	state->maxRecordsPerPage = (state->pageSize - state->headerSize) / state->recordSize;
	state->maxInteriorRecordsPerPage = (state->pageSize - state->headerSize) / (state->keySize+sizeof(int32_t));

	// Hard-code for testing for now
	state->maxRecordsPerPage = 10;
	state->maxInteriorRecordsPerPage = 3;	
	state->levels = 1;

	/* Create and write empty root node */
	initBufferPage(state, 0);
	SBTREE_SET_ROOT(state->buffer);
		
	state->activePath[0] = writePage(state, state->buffer);	/* Store root location */	

	/* Allocate first page of buffer as output page */
	initBufferPage(state, 0);
}



void sbtreePrintNode(sbtreeState *state, int page, int depth)
{
	readPage(state, page);
	for (int i=0; i < depth*3; i++)
		printf(" "); 
	
	void *buf = state->buffer+state->pageSize;
	int16_t count =  SBTREE_GET_COUNT(buf); 

	printf("Id: %d Page: %d Cnt: %d IsRoot? %d IsInt? %d Min %d Max %d\n", SBTREE_GET_ID(buf), page, count, (SBTREE_IS_ROOT(buf)), SBTREE_IS_INTERIOR(buf), SBTREE_GET_MIN(buf), SBTREE_GET_MAX(buf));
	if (SBTREE_IS_INTERIOR(buf))
	{
		for (int c=0; c < count; c++)
		{
			int32_t key = *((int32_t*) (buf+state->keySize * c + state->headerSize));
			int32_t val = *((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(int32_t)));
			if (c == count-1 && depth+1 < state->levels && page == state->activePath[depth])
			{	/* Current pointer is on active path. */
				if (state->activePath[depth+1] != val)
					printf("Node mapping. Node: %d Current: %d\n",val, state->activePath[depth+1]);
				val = state->activePath[depth+1];

			}			
			sbtreePrintNode(state, val, depth+1);	
			readPage(state, page);			
		}
		/*
		if (count > 0 && SBTREE_IS_ROOT(buf))
		{	// Print last child pointer 
			int32_t val = *((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + (count)*sizeof(int32_t)));			
			sbtreePrintNode(state, val, depth+1);		
		}
		*/
	}
}

void sbtreePrint(sbtreeState *state)
{
	int32_t currentNode = state->activePath[0];

	printf("\n\nPrint tree:\n");
	sbtreePrintNode(state, currentNode, 0);
}

void sbtreeUpdateIndex(sbtreeState *state, void *minkey, void *key, int32_t pageNum)
{
		int32_t minLeafVal = *((int32_t*)minkey);

		/* Read parent page then update */
		int8_t l = 0;
		int32_t prevPageNum = -1;
		
		for (l=state->levels-1; l >= 0; l--)
		{
			if (readPage(state, state->activePath[l]) != 0)
				return;	

			/*	if (state->activePath[l] == 43)
					printf("HEre");
			printf("Read page: %d\n", state->activePath[l]);
			*/
			/* Determine if there is space in the page */
			void *buf = state->buffer+state->pageSize;
			int16_t count =  SBTREE_GET_COUNT(buf); 
			
			
			if (count >= state->maxInteriorRecordsPerPage)
			{	/* Interior node at this level is full. Create a new node. */	

				/* If tree is beyond level 1, update parent node last child pointer as will have changed. Currently in buffer. */
				if (l < state->levels - 1)
				{					
				//	sbtreePrintNode(state, state->activePath[l], 0);
					memcpy(buf + state->keySize * state->maxInteriorRecordsPerPage + sizeof(int32_t) * (count-1) + state->headerSize, &prevPageNum, sizeof(int32_t));											
					state->activePath[l]  = writePage(state, buf);
				}
			
				initBufferPage(state, 0);
				SBTREE_SET_INTERIOR(state->buffer);
				buf = state->buffer;

				/* Store pointer to new leaf node */
				memcpy(buf + state->headerSize, key, state->keySize);
				memcpy(buf + state->keySize * state->maxInteriorRecordsPerPage + state->headerSize, &pageNum, sizeof(int32_t));
				SBTREE_INC_COUNT(buf);	

				prevPageNum = state->activePath[l];
				state->activePath[l] = writePage(state, state->buffer);												
				pageNum = state->activePath[l];						
			}
			else 
			{
				/* Copy record onto page */
				/* Record is key just trying to insert (as above values already in block) and pageNum just written with previous data page */
				/* Keep keys and data as contiguous sorted arrays */
				if (count < state->maxInteriorRecordsPerPage - 1)
				{	/* Do not store key for last child pointer */ 
					memcpy(buf + state->keySize * count + state->headerSize, key, state->keySize);
				}
				memcpy(buf + state->keySize * state->maxInteriorRecordsPerPage + sizeof(int32_t) * count + state->headerSize, &pageNum, sizeof(int32_t));		
				
				/* Update previous pointer as may have changed due to writes. */
				if (count > 0 && prevPageNum != -1)
					memcpy(buf + state->keySize * state->maxInteriorRecordsPerPage + sizeof(int32_t) * (count-1) + state->headerSize, &prevPageNum, sizeof(int32_t));	

				/* Update count */
				SBTREE_INC_COUNT(buf);	

				/* Write updated interior page */								
				/* Update location of page */
				state->activePath[l] = writePage(state, buf);				
				break;
			}
			
		}		 
		
		/* Handle the case where root is currently full */
		/* Grow one level, shift everything down, and create new root */
		if (l == -1)
		{			

			initBufferPage(state, 0);

			/* Copy record onto page */
			memcpy(state->buffer + state->headerSize, &minLeafVal, state->keySize);
			memcpy(state->buffer + state->keySize * state->maxInteriorRecordsPerPage + state->headerSize, &prevPageNum, sizeof(int32_t));
			
			/* Copy greater than record on to page. Note: Basically child pointer and infinity for key */
			minLeafVal = INT_MAX;
			memcpy(state->buffer + state->headerSize, &minLeafVal, state->keySize);
			memcpy(state->buffer + state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + sizeof(int32_t), &state->activePath[0], sizeof(int32_t));
			

			/* Update count */
			SBTREE_INC_COUNT(state->buffer);	
			SBTREE_INC_COUNT(state->buffer);	
			SBTREE_SET_ROOT(state->buffer);
			
			for (l=state->levels; l > 0; l--)
				state->activePath[l] = state->activePath[l-1]; 
			state->activePath[0] = writePage(state, state->buffer);	/* Store root location */			
			state->levels++;

		//	sbtreePrint(state);
			
		}

}

/**
@brief     	Inserts a given key, data pair into structure.
@param     	state
                SBTree algorithm state structure
@param     	timestamp
                Integer timestamp (increasing)
@param     	key
                Key for record
@param     	data
                Data for record
*/
void sbtreeInsert(sbtreeState *state, int32_t timestamp, void* key, void *data)
{	
	/* Copy record into block */
	int16_t count =  SBTREE_GET_COUNT(state->buffer); 

	/* Write current page if full */
	if (count >= state->maxRecordsPerPage)
	{
		// sbtreePrint(state);

		/* Write page first so can use buffer for updating tree structure */
		int32_t pageNum = writePage(state, state->buffer);		

		int32_t minLeafVal = *((int32_t*) (state->buffer+state->headerSize));
		printf("Min leaf val: %d\n", minLeafVal);

		/* Add pointer to page to B-tree structure */
		sbtreeUpdateIndex(state, &minLeafVal, key, pageNum);
				
		count = 0;
		initBufferPage(state, 0);					
	}

	/* Copy record onto page */
	memcpy(state->buffer + state->recordSize * count + state->headerSize, key, state->keySize);
	memcpy(state->buffer + state->recordSize * count + state->headerSize + state->keySize, data, state->dataSize);

	/* Update count */
	SBTREE_INC_COUNT(state->buffer);	

	/* Update Max */
	SBTREE_UPDATE_MAX(state->buffer, key);

	/* Update Min */
	SBTREE_UPDATE_MIN(state->buffer, key);

	/* Update Sum */
//	SBTREE_UPDATE_SUM(state->buffer, key);

	/* TODO: Update statistics and bitmap */
	int8_t* bm = (int8_t*) (state->buffer+state->bmOffset);

	// printf("Current bitmap: "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*bm));	
	state->updateBitmap(key, bm);

	// printf("\nNew bitmap: "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*bm));
	// printf("\n");

	/* TODO: Update timestamps and key min/max in page */
}

/**
@brief     	Inserts a given record into structure.
@param     	state
                SBTree algorithm state structure
@param     	timestamp
                Integer timestamp (increasing)
@param     	record
                Record consisting of key bytes then data bytes
*/
void sbtreeInsertRec(sbtreeState *state, int32_t timestamp, void* record)
{
	sbtreeInsert(state, timestamp, record, record+state->keySize);	
}

int32_t writePage(sbtreeState *state, void *buffer)
{    
	/* Always writes to next page number. Returned to user. */
	// Sprintf("\nWrite page: %d\n", pageNum);
	int32_t pageNum = state->nextPageWriteId++;

	/* Setup page number in header */	
	memcpy(buffer, &(state->nextPageId), sizeof(int32_t));
	state->nextPageId++;

	/* Seek to page location in file */
    fseek(state->file, pageNum*state->pageSize, SEEK_SET);

	fwrite(buffer, state->pageSize, 1, state->file);
	#ifdef DEBUG_WRITE
            printf("Wrote block. Idx: %d Cnt: %d\n", *((int32_t*) buffer), SBTREE_GET_COUNT(state->buffer));
			printf("BM: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY( *((uint8_t*) (state->buffer+state->bmOffset))));
            for (int k = 0; k < SBTREE_GET_COUNT(buffer); k++)
            {
                test_record_t *buf = (void *)(buffer + state->headerSize + k * state->recordSize);
                printf("%d: Output Record: %d\n", k, buf->key);
            }
	#endif
	return pageNum;
}

int8_t readPage(sbtreeState *state, int pageNum)
{    
	// printf("RP: %d\n", pageNum);
    FILE* fp = state->file;
    void *buf = state->buffer + state->pageSize;

    /* Seek to page location in file */
    fseek(fp, pageNum*state->pageSize, SEEK_SET);

    /* Read page into start of buffer 1 */   
    if (0 ==  fread(buf, state->pageSize, 1, fp))
    {	return 1;       
    }
       
    #ifdef DEBUG_READ
        printf("Read block: %d Count: %d\r\n",pageNum, SBTREE_GET_COUNT(buf));   
		printf("BM: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY( *((uint8_t*) (buf+state->bmOffset))));  		
        // for (int k = 0; k < SBTREE_GET_COUNT(buf); k++)
		for (int k = 0; k < 1; k++) // Only print first record
        {
            test_record_t *rec = (void *)(buf+state->headerSize+k*state->recordSize);
            printf("%d: Record: %d\n", k, rec->key);
        }
    #endif
	return 0;
}

/* Returns a value of a tuple given a record number in a block (that has been previously buffered) */
void* getRecord(sbtreeState* state, int recordNum)
{      
    test_record_t *buf = (test_record_t*) (state->buffer+state->pageSize+state->headerSize+recordNum*state->recordSize);
    return NULL;
	// todo;
	// return (void*) buf->key;	    
}


/**
@brief     	Initialize iterator on SBTREE structure.
@param     	state
                SBTREE algorithm state structure
*/
void sbtreeInitIterator(sbtreeState *state, sbtreeIterator *it)
{
	printf("Initializing iterator.\n");

	/* Build query bitmap (if used) */
	if (SBTREE_USING_BMAP(state->parameters))
	{
		uint8_t *bm = malloc(sizeof(uint8_t));
		*bm = 0;
		state->buildBitmap(it->minKey, it->maxKey, bm);
		printf("BM: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(*bm));  	
		it->queryBitmap = bm;
	}

	/* Read first page into memory */
	it->lastIterPage = -1;
	it->lastIterRec = 10000;	/* Force to read next page */	
}

/**
@brief     	Flushes output buffer.
@param     	state
                SBTREE algorithm state structure
*/
int8_t sbtreeFlush(sbtreeState *state)
{
	int32_t pageNum = writePage(state, state->buffer);	

	/* Add pointer to page to B-tree structure */
	int32_t minLeafVal = *((int32_t*) (state->buffer+state->headerSize));
	int32_t maxKey = INT_MAX;
	sbtreeUpdateIndex(state, &minLeafVal, &maxKey, pageNum);

	/* Reinitialize buffer */
	initBufferPage(state, 0);
	return 0;
}

/**
@brief     	Return next record in SBTREE structure.
@param     	state
                SBTREE algorithm state structure
@param     	data
                Data for record
*/
int8_t sbtreeNext(sbtreeState *state, sbtreeIterator *it, void **data)
{	
	void *buf = state->buffer+state->pageSize;
	/* Iterate until find a record that matches search criteria */
	while (1)
	{	
		if (it->lastIterRec >= SBTREE_GET_COUNT(buf))
		{	/* Read next page */			
			it->lastIterRec = 0;

			while (1)
			{
				it->lastIterPage++;
				if (readPage(state, it->lastIterPage) != 0)
					return 0;		


				/* TODO: Check timestamps, min/max, and bitmap to see if query range overlaps with range of records	stored in block */
				/* If not read next block */
				if (SBTREE_USING_BMAP(state->parameters))
				{
					uint8_t bm = SBTREE_GET_BITMAP(state, buf);
					/* TODO: Need to make bitmap comparison more generic. */
					if ( ( *((uint8_t*) it->queryBitmap) & bm) >= 1)
					{	/* Overlap in bitmap - go to next page */
						break;
					}
					else
					{
					//	printf("Skipping page as no bitmap overlap\n");
					}					
				}
			}
		}
		
		/* Get record */	
		*data = buf+state->headerSize+it->lastIterRec*state->recordSize;
		it->lastIterRec++;
		
		//printf("Key: %d\n", **((int32_t**) data));
		/* TODO: Check that record meets filter constraints */
		if (it->minKey != NULL && state->compareData(*data, it->minKey) < 0)
			continue;
		if (it->maxKey != NULL && state->compareData(*data, it->maxKey) > 0)
			continue;
		return 1;
	}
}

/**
@brief     	Inserts a given record into structure.
@param     	state
                SBTREE algorithm state structure
@param     	timestamp
                Integer timestamp (increasing)
@param     	data
                Data for record
*/
void sbtreeRangeQuery(sbtreeState *state, void *minRange, void *maxRange)
{

}
