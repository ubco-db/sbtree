/******************************************************************************/
/**
@file		dbbuffer.c
@author		Ramon Lawrence
@brief		Light-weight buffer implementation for small embedded devices.
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
#include <string.h>

#include "dbbuffer.h"


/**
@brief     	Allocates space for buffer given page size and number of pages.
@param     	state
                DBbuffer state structure
*/
void dbbufferInit(dbbuffer *state)
{
	printf("Initializing buffer.\n");
	printf("Buffer size: %d  Page size: %d\n", state->numPages, state->pageSize);			
	
	/* TODO: These values would be set during recovery if database already exists. */
	state->nextPageId = 0;
	state->nextPageWriteId = 0;
	
	state->numReads = 0;
	state->numWrites = 0;
	state->bufferHits = 0;	
}

/**
@brief      Reads page either from buffer or from storage. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPage(dbbuffer *state, id_t pageNum)
{    
//	printf("RP: %d\n", pageNum);
    FILE* fp = state->file;
    void *buf = state->buffer + state->pageSize;

    /* Seek to page location in file */
    fseek(fp, pageNum*state->pageSize, SEEK_SET);

    /* Read page into start of buffer 1 */   
    if (0 ==  fread(buf, state->pageSize, 1, fp))
    {	return NULL;       
    }
    state->numReads++;
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
	return buf;
}

/**
@brief      Reads page either from buffer or from storage. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param     	pageNum
                Physical page id (number)
@param		bufferNum
				Buffer to read into
@return		Returns pointer to buffer page or NULL if error.
*/
void* readPageBuffer(dbbuffer *state, id_t pageNum, count_t bufferNum)
{
	return readPage(state, pageNum);
}


/**
@brief      Reads page either from buffer or from storage. Returns pointer to buffer if success.
@param     	state
                DBbuffer state structure
@param		buffer
				Pointer to in-memory buffer
@param     	pageNum
                Physical page id (number)
@return		Returns 0 if success. -1 if error.
*/
int32_t writePage(dbbuffer *state, void* buffer)
{    
	/* Always writes to next page number. Returned to user. */	
	int32_t pageNum = state->nextPageWriteId++;
	// printf("\nWrite page: %d Key: %d\n", pageNum, *((int32_t*) (buffer+6)));

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
	state->numWrites++;
	return pageNum;
}


/**
@brief     	Initialize in-memory buffer page.
@param     	state
                DBbuffer state structure
@param     	pageNum
                In memory buffer page id (number)
@return		pointer to initialized page
*/
void* initBufferPage(dbbuffer *state, int pageNum)
{	
	/* Insure all values are 0 in page. */
	/* TODO: May want to initialize to all 1s for certian memory types. */	
	void *buf = state->buffer + pageNum * state->pageSize;
	for (uint16_t i = 0; i < state->pageSize/sizeof(int32_t); i++)
    {
        ((int32_t*) buf)[i] = 0;
    }
	return buf;		
}

/**
@brief     	Closes buffer.
@param     	state
                DBbuffer state structure
*/
void closeBuffer(dbbuffer *state)
{
	printf("Num reads: %d\n", state->numReads);
	printf("Buffer hits: %d\n", state->bufferHits);
	 printf("Num writes: %d\n", state->numWrites);
}