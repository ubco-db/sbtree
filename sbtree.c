/******************************************************************************/
/**
@file		sbtree.c
@author		Ramon Lawrence
@brief		Implementation for sequential B-tree.
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


/*
Comparison functions. Code is adapted from ldbm.
*/

/**
@brief     	Compares two unsigned int32_t values.
@param     	a
                value 1
@param     b
                value 2
*/
static int8_t uint32Compare(void *a, void *b)
{
	return *((uint32_t*)a) - *((uint32_t*)b);	
}


/**
@brief     	Compares two values by bytes. 
@param     	a
                value 1
@param     b
                value 2
*/
static int8_t byteCompare(void *a, void *b, int16_t size)
{
	return memcmp(a, b, size);	
}

/**
@brief     	Initialize in-memory buffer page.
@param     	state
                SBTree algorithm state structure
@param     	pageNum
                In memory buffer page id (number)
*/
void initBufferPage(sbtreeState *state, int pageNum)
{	
	/* Insure all values are 0 in page. */
	/* TODO: May want to initialize to all 1s for certian memory types. */	
	void *buf = state->buffer + pageNum * state->pageSize;
	for (uint16_t i = 0; i < state->pageSize/sizeof(int32_t); i++)
    {
        ((int32_t*) buf)[i] = 0;
    }		
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

	state->compareKey = uint32Compare;

	/* Calculate block header size */
	/* Header size fixed: 14 bytes: 4 byte id, 2 for record count. */
	/* Variable size header if use min/max and bitmap indexing */		
	state->headerSize = 6;
	/* TODO: Add variable size header calculation. */
	state->bmOffset = 22; /* 1 byte offset. TODO: Remove this hard-code of location and size of bitmap */

	/* Calculate number of records per page */
	state->maxRecordsPerPage = (state->pageSize - state->headerSize) / state->recordSize;
	state->maxInteriorRecordsPerPage = (state->pageSize - state->headerSize) / (state->keySize+sizeof(id_t));

	// Hard-code for testing for now
	state->maxRecordsPerPage = 10;
	state->maxInteriorRecordsPerPage = 3;	
	state->levels = 1;

	/* Create and write empty root node */
	initBufferPage(state, 0);
	SBTREE_SET_ROOT(state->buffer);		
	state->activePath[0] = writePage(state, state->buffer);		/* Store root location */	

	/* Allocate first page of buffer as output page for data records */	
	initBufferPage(state, 0);
}


/**
@brief     	Return the smallest key in the node
@param     	state
                SBTree algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* sbtreeGetMinKey(sbtreeState *state, void *buffer)
{
	return (void*) (buffer+state->headerSize);
}

/**
@brief     	Return the smallest key in the node
@param     	state
                SBTree algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* sbtreeGetMaxKey(sbtreeState *state, void *buffer)
{
	int16_t count =  SBTREE_GET_COUNT(buffer); 
	if (count == 0)
		count = 1;		/* Force to have value in buffer. May not make sense but likely initialized to 0. */
	return (void*) (buffer+state->headerSize+(count-1)*state->recordSize);
}

/**
@brief     	Print a node in an in-memory buffer.
@param     	state
                SBTree algorithm state structure
@param     	pageNum
                Physical page id (number)	
@param     	depth
                Used for nesting print out
@param     	buffer
                In memory page buffer with node data
*/
void sbtreePrintNodeBuffer(sbtreeState *state, int pageNum, int depth, void *buffer)
{
	int16_t count =  SBTREE_GET_COUNT(buffer); 

	if (SBTREE_IS_INTERIOR(buffer))
	{		
		printf("%*cId: %d Page: %d Cnt: %d [%d, %d]\n", depth*3, ' ', SBTREE_GET_ID(buffer), pageNum, count, (SBTREE_IS_ROOT(buffer)), SBTREE_IS_INTERIOR(buffer));		
		/* Print data records (optional) */	
		printf("%*c", depth*3+2, ' ');	
		for (int c=0; c < count; c++)
		{			
			int32_t key = *((int32_t*) (buffer+state->keySize * c + state->headerSize));
			int32_t val = *((int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)));
			printf(" (%d, %d)", key, val);			
		}
		/* Print last pointer */
		int32_t val = *((int32_t*) (buffer+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + count*sizeof(id_t)));
		printf(" (, %d)\n", val);
	}
	else
	{		
		printf("%*cId: %d Pg: %d Cnt: %d (%d, %d)\n", depth*3, ' ', SBTREE_GET_ID(buffer), pageNum, count, *((int32_t*) sbtreeGetMinKey(state, buffer)), *((int32_t*) sbtreeGetMaxKey(state, buffer)));
		/* Print data records (optional) */
		/*
		for (int c=0; c < count; c++)
		{
			int32_t key = *((int32_t*) (buffer + state->headerSize + state->recordSize * c));
			int32_t val = *((int32_t*) (buffer + state->headerSize + state->recordSize * c + state->keySize));
			printf("%*cKey: %d Value: %d\n", depth*3+2, ' ', key, val);			
		}	
		*/
	}
}

/**
@brief     	Print a node read from storage.
@param     	state
                SBTree algorithm state structure
@param     	pageNum
                Physical page id (number)	
@param     	depth
                Used for nesting print out
*/
void sbtreePrintNode(sbtreeState *state, int pageNum, int depth)
{
	readPage(state, pageNum);

	void *buf = state->buffer+state->pageSize;
	int16_t count =  SBTREE_GET_COUNT(buf); 	

	sbtreePrintNodeBuffer(state, pageNum, depth, buf);
	if (SBTREE_IS_INTERIOR(buf))
	{				
		for (int c=0; c < count; c++)
		{
			int32_t key = *((int32_t*) (buf+state->keySize * c + state->headerSize));
			int32_t val = *((int32_t*) (buf+state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + c*sizeof(id_t)));
			if (c == count-1 && depth+1 < state->levels && pageNum == state->activePath[depth])
			{	/* Current pointer is on active path. */
				if (state->activePath[depth+1] != val)
					printf("%*cNode mapping. Node: %d Current: %d\n",depth*3, ' ', val, state->activePath[depth+1]);
				val = state->activePath[depth+1];

			}			
			sbtreePrintNode(state, val, depth+1);	
			readPage(state, pageNum);			
		}		
	}	
}


/**
@brief     	Print current SBTree as written on storage.
@param     	state
                SBTree algorithm state structure
*/
void sbtreePrint(sbtreeState *state)
{	
	printf("\n\nPrint tree:\n");
	sbtreePrintNode(state, state->activePath[0], 0);
}


/**
@brief     	Updates the B-tree index structure from leaf node to root node as required.
@param     	state
                SBTree algorithm state structure
@param     	minkey
                smallest key in currently full buffer page
@param     	key
                current key being inserted				
@param     	pageNum
                Physical page id of full leaf page just written to storage
*/
void sbtreeUpdateIndex(sbtreeState *state, void *minkey, void *key, id_t pageNum)
{		
	/* Read parent pages (nodes) until find space for new interior pointer (key, pageNum) */
	int8_t l = 0;
	int16_t count;
	int32_t prevPageNum = -1;
	void *buf;

	for (l=state->levels-1; l >= 0; l--)
	{
		if (readPage(state, state->activePath[l]) != 0)
			return;		/* TODO: Should handle error condition on failed read. */
		
		/* Determine if there is space in the page */
		buf = state->buffer+state->pageSize;
		count =  SBTREE_GET_COUNT(buf); 
				
		if (count > state->maxInteriorRecordsPerPage)
		{	/* Interior node at this level is full. Create a new node. */	

			/* If tree is beyond level 1, update parent node last child pointer as will have changed. Currently in buffer. */
			if (l < state->levels - 1)
			{								
				memcpy(buf + state->keySize * state->maxInteriorRecordsPerPage + sizeof(id_t) * (count-1) + state->headerSize, &prevPageNum, sizeof(id_t));											
				state->activePath[l]  = writePage(state, buf);
			}
		
			initBufferPage(state, 0);
			SBTREE_SET_INTERIOR(state->buffer);
			buf = state->buffer;

			/* Store pointer to new leaf node */
			memcpy(buf + state->headerSize, key, state->keySize);
			memcpy(buf + state->keySize * state->maxInteriorRecordsPerPage + state->headerSize, &pageNum, sizeof(id_t));
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
			/* TODO: Evaluate if benefit for storing key for last child pointer or not. */
			if (count < state->maxInteriorRecordsPerPage)
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

		/* Copy record onto page (minkey, prevPageNum) */
		memcpy(state->buffer + state->headerSize, minkey, state->keySize);		
		memcpy(state->buffer + state->headerSize + state->keySize * state->maxInteriorRecordsPerPage, &prevPageNum, sizeof(id_t));
		
		/* Copy greater than record on to page. Note: Basically child pointer and infinity for key */		
		// int32_t maxKey = INT_MAX;
		// memcpy(state->buffer + state->headerSize + state->keySize, &maxKey, state->keySize);
		memcpy(state->buffer + state->keySize * state->maxInteriorRecordsPerPage + state->headerSize + sizeof(id_t), &state->activePath[0], sizeof(id_t));		

		/* Update count */
		SBTREE_INC_COUNT(state->buffer);	
		// SBTREE_INC_COUNT(state->buffer);	
		SBTREE_SET_ROOT(state->buffer);
		
		for (l=state->levels; l > 0; l--)
			state->activePath[l] = state->activePath[l-1]; 
		state->activePath[0] = writePage(state, state->buffer);	/* Store root location */			
		state->levels++;
	}
}

/**
@brief     	Puts a given key, data pair into structure.
@param     	state
                SBTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t sbtreePut(sbtreeState *state, void* key, void *data)
{		
	int16_t count =  SBTREE_GET_COUNT(state->buffer); 

	/* Write current page if full */
	if (count >= state->maxRecordsPerPage)
	{	
		/* Write page first so can use buffer for updating tree structure */
		int32_t pageNum = writePage(state, state->buffer);				

		/* Add pointer to page to B-tree structure */
		/* Second pointer parameter is minimum key in currently full leaf node of data */
		/* Need to copy key from current write buffer as will reuse buffer */
		memcpy(state->tempKey, (void*) (state->buffer+state->headerSize), state->keySize); 
		sbtreeUpdateIndex(state, state->tempKey, key, pageNum);

		count = 0;			
		initBufferPage(state, 0);					
	}

	/* Copy record onto page */
	memcpy(state->buffer + state->recordSize * count + state->headerSize, key, state->keySize);
	memcpy(state->buffer + state->recordSize * count + state->headerSize + state->keySize, data, state->dataSize);

	/* Update count */
	SBTREE_INC_COUNT(state->buffer);	

	/* Update Max */
//	SBTREE_UPDATE_MAX(state->buffer, key);

	/* Update Min */
//	SBTREE_UPDATE_MIN(state->buffer, key);

	/* TODO: Update statistics and bitmap */
//	int8_t* bm = (int8_t*) (state->buffer+state->bmOffset);

	// printf("Current bitmap: "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*bm));	
//	state->updateBitmap(key, bm);

	// printf("\nNew bitmap: "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*bm));
	// printf("\n");

	// sbtreePrintNodeBuffer(state, 0, 0, state->buffer);
	return 0;
}

/**
@brief     	Given a key, searches the node for the key.
			If interior node, returns pointer to next page id to follow.
			If leaf node, returns pointer to first record with that key.
			Returns NULL if key is not found.			
@param     	state
                SBTree algorithm state structure
@param     	buffer
                Pointer to in-memory buffer holding node
@param     	key
                Key for record
*/
void* sbtreeSearchNode(sbtreeState *state, void *buffer, void* key)
{
	int16_t first, last, middle, count;
	int8_t compare, interior;
	void *mkey;

	first = 0;
	count = SBTREE_GET_COUNT(buffer);
  	last =  count - 1;
  	middle = (first+last)/2;
	interior = SBTREE_IS_INTERIOR(buffer);

	if (interior)
	{
		while (first <= last) 
		{			
			mkey = buffer+state->headerSize+state->keySize*(middle-1);
			compare = state->compareKey(mkey, key);
			if (compare < 0)
				first = middle + 1;
			else if (compare == 0) 
				return buffer + state->headerSize + state->keySize*count +state->dataSize*(middle-1);						
			else
				last = middle - 1;

			middle = (first + last)/2;
		}
		return NULL;
	}
	else
	{
		while (first <= last) 
		{			
			mkey = buffer+state->headerSize+state->recordSize*(middle-1);
			compare = state->compareKey(mkey, key);
			if (compare < 0)
				first = middle + 1;
			else if (compare == 0) 
				return mkey;
			else
				last = middle - 1;

			middle = (first + last)/2;
		}
		return NULL;
	}
}


/**
@brief     	Given a key, returns data associated with key.
			Note: Space for data must be already allocated.
			Data is copied from database into data buffer.
@param     	state
                SBTree algorithm state structure
@param     	key
                Key for record
@param     	data
                Pre-allocated memory to copy data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t sbtreeGet(sbtreeState *state, void* key, void *data)
{
	/* Starting at root search for key */
	int8_t l;
	void* next, *buf;;

	for (l=0; l < state->levels-1; l++)
	{		
		readPage(state, state->activePath[l]);
		buf = state->buffer + state->pageSize;

		/* Find the key within the node. Sorted by key. Use binary search. */
		next = sbtreeSearchNode(state, buf, key);

		if (next != NULL)
		{
			int32_t* nextId = (int32_t*) next;
			printf("Next page: %d\n", *nextId);
		}
	}
	return 0;
}

/**
@brief     	Writes a physical page to storage currently in given buffer.
			Returns physical page number of page location.
			Updates page to have next page id.
@param     	state
                SBTree algorithm state structure
@param     	buffer
                in-memory buffer containing page to write
@return     
            Physical page number of location where page is written
*/
int32_t writePage(sbtreeState *state, void *buffer)
{    
	/* Always writes to next page number. Returned to user. */
	// printf("\nWrite page: %d\n", pageNum);
	int32_t pageNum = state->nextPageWriteId++;

	/* Setup page number in header */	
	memcpy(buffer, &(state->nextPageId), sizeof(id_t));
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

/**
@brief     	Reads a physical page into a buffer. 
			Returns buffer pointer or NULL if failure.
@param     	state
                SBTree algorithm state structure
@param     	pageNum
                physical page id to read
@return     
            Buffer pointer or NULL if failutre
*/
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

/**
@brief     	Flushes output buffer.
@param     	state
                SBTREE algorithm state structure
*/
int8_t sbtreeFlush(sbtreeState *state)
{
	int32_t pageNum = writePage(state, state->buffer);	

sbtreePrint(state);
	/* Add pointer to page to B-tree structure */		
	/* So do not have to allocate memory. Use the next key value in the buffer temporarily to store a MAX_KEY of all 1 bits */	
	/* Need to copy key from current write buffer as will reuse buffer */
	memcpy(state->tempKey, (void*) (state->buffer+state->headerSize), state->keySize); 	
	void *maxkey = state->buffer + state->recordSize * SBTREE_GET_COUNT(state->buffer) + state->headerSize;
	memset(maxkey, 1, state->keySize);
	sbtreeUpdateIndex(state, state->tempKey, maxkey, pageNum);

	/* Reinitialize buffer */
	initBufferPage(state, 0);
	return 0;
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
					uint8_t bm = 0; // SBTREE_GET_BITMAP(state, buf);
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
				else
					break;
			}
		}
		
		/* Get record */	
		*data = buf+state->headerSize+it->lastIterRec*state->recordSize;
		it->lastIterRec++;
		
		//printf("Key: %d\n", **((int32_t**) data));
		/* TODO: Check that record meets filter constraints */
		if (it->minKey != NULL && state->compareKey(*data, it->minKey) < 0)
			continue;
		if (it->maxKey != NULL && state->compareKey(*data, it->maxKey) > 0)
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
