/******************************************************************************/
/**
@file		sbtree.h
@author		Ramon Lawrence
@brief		Implementation for sequential, copy-on-write B-tree.
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
#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* Define type for page ids (physical and logical). */
typedef uint32_t id_t;

/* Define type for page record count. */
typedef uint16_t count_t;

#define SBTREE_USE_INDEX	1
#define SBTREE_USE_MAX_MIN	2
#define SBTREE_USE_BMAP		4

#define SBTREE_USING_INDEX(x)  	x & SBTREE_USE_INDEX
#define SBTREE_USING_MAX_MIN(x) x & SBTREE_USE_MAX_MIN
#define SBTREE_USING_BMAP(x)  	x & SBTREE_USE_BMAP

/* Offsets with header */
#define SBTREE_COUNT_OFFSET		sizeof(id_t)

/* MOD 10000 to remove any flags in count that are set above 10000 */
#define SBTREE_GET_ID(x)  		*((id_t *) (x)) 
#define SBTREE_GET_COUNT(x)  	*((count_t *) (x+SBTREE_COUNT_OFFSET)) % 10000
#define SBTREE_SET_COUNT(x,y)  	*((count_t *) (x+SBTREE_COUNT_OFFSET)) = y
#define SBTREE_INC_COUNT(x)  	*((count_t *) (x+SBTREE_COUNT_OFFSET)) = *((count_t *) (x+SBTREE_COUNT_OFFSET))+1

/* Using count field above 10000 for interior node and 20000 for root node */
#define SBTREE_IS_INTERIOR(x)  	(*((count_t *) (x+SBTREE_COUNT_OFFSET)) >= 10000 ? 1 : 0)
#define SBTREE_IS_ROOT(x)  		(*((count_t *) (x+SBTREE_COUNT_OFFSET)) >= 20000 ? 1 : 0)
#define SBTREE_SET_INTERIOR(x) 	SBTREE_SET_COUNT(x,*((count_t *) (x+SBTREE_COUNT_OFFSET))+10000)
#define SBTREE_SET_ROOT(x) 		SBTREE_SET_COUNT(x,*((count_t *) (x+SBTREE_COUNT_OFFSET))+20000)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

typedef struct {
	FILE *file;									/* File for storing data records. TODO: Will be replaced with RAW memory access routines. */
	void *buffer;								/* Pre-allocated memory buffer for use by algorithm */
	uint8_t bufferSizeInBlocks;					/* Size of buffer in blocks */
	uint16_t pageSize;							/* Size of physical page on device */
	uint8_t parameters;    						/* Parameter flags for indexing and bitmaps */
	uint8_t keySize;							/* Size of key in bytes (fixed-size records) */
	uint8_t dataSize;							/* Size of data in bytes (fixed-size records) */
	uint8_t recordSize;							/* Size of record in bytes (fixed-size records) */
	uint8_t headerSize;							/* Size of header in bytes (calculated during init()) */
	id_t nextPageId;							/* Next logical page id. Page id is an incrementing value and may not always be same as physical page id. */
	count_t maxRecordsPerPage;					/* Maximum records per page */
	count_t maxInteriorRecordsPerPage;			/* Maximum interior records per page */
	uint8_t bmOffset;							/* Offset of bitmap in header from start of block */
    int8_t (*compareKey)(void *a, void *b);		/* Function that compares two arbitrary keys passed as parameters */
	void (*extractData)(void *data);			/* Given a record, function that extracts the data (key) value from that record */
	void (*updateBitmap)(void *data, void *bm);	/* Given a record, updates bitmap based on its data (key) value */
	int8_t (*inBitmap)(void *data, void *bm);	/* Returns 1 if data (key) value is a valid value given the bitmap */
	void (*buildBitmap)(void *min, void *max, void *bm);	/* Builds a query bitmap given [min,max] range of keys */
	uint8_t levels;								/* Number of levels in tree */
	id_t activePath[5];							/* Active path of page indexes from root (in position 0) to node just above leaf */
	id_t nextPageWriteId;						/* Physical page id of next page to write. */
	void *tempKey;								/* Used to temporarily store a key value. Space must be preallocated. */
} sbtreeState;

typedef struct {
	id_t activeIteratorPath[5];					/* Active path of iterator from root (in position 0) to current leaf node */    
	count_t lastIterRec[5];						/* Last record processed by iterator at each level */
	void*	minKey;
	void*	maxKey;
    void*	minTime;
	void* 	maxTime;
	void*	queryBitmap;
} sbtreeIterator;

/**
@brief     	Initialize an SBTree structure.
@param     	state
                SBTree algorithm state structure
*/
void sbtreeInit(sbtreeState *state);

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
int8_t sbtreePut(sbtreeState *state, void* key, void *data);

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
int8_t sbtreeGet(sbtreeState *state, void* key, void *data);

/**
@brief     	Initialize iterator on SBTREE structure.
@param     	state
                SBTree algorithm state structure
@param     	it
                SBTree iterator state structure
*/
void sbtreeInitIterator(sbtreeState *state, sbtreeIterator *it);

/**
@brief     	Initialize iterator on SBTREE structure.
@param     	state
                SBTree algorithm state structure
@param     	it
                SBTree iterator state structure
@param     	key
                Key for record
@param     	data
                Data for record
*/
int8_t sbtreeNext(sbtreeState *state, sbtreeIterator *it, void **key, void **data);

/**
@brief     	Flushes output buffer.
@param     	state
                SBTree algorithm state structure
*/
int8_t sbtreeFlush(sbtreeState *state);

/**
@brief     	Prints SBTree structure to standard output.
@param     	state
                SBTree algorithm state structure
*/
void sbtreePrint(sbtreeState *state);

/**
@brief      Reads page from storage. Returns 0 if success.
@param     	state
                SBTree algorithm state structure
@param     	pageNum
                Physical page id (number)
*/
int8_t readPage(sbtreeState *state, int32_t pageNum);

/**
@brief      Writes page to storage. Returns physical page id if success. -1 if failure.
@param     	state
                SBTree algorithm state structure
@param     	buffer
                In memory buffer containing page
*/
int32_t writePage(sbtreeState *state, void *buffer);

#if defined(__cplusplus)
}
#endif