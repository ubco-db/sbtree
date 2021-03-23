# SBITS Embedded Index Structure for Time Series Data 

SBITS is a high performance embedded data storage and index structure for time series data for embedded systems and Arduino:

1. Uses the minimum of two page buffers for performing all operations. The memory usage is less than 1.5 KB for 512 byte pages.
2. Performance is several times faster than using B-trees and hash-based indexes. Simplifies data management without worrying about low-level details and storing data in raw files.
3. No use of dynamic memory (i.e. malloc()). All memory is pre-allocated at creation of the index.
4. Efficient insert (put) and query (get) of arbitrary key-value data. Ability to search data both on timestamp (key) and by data value.
5. Option to store time series data with or without an index. Adding an index allows for faster retrieval of records based on data value.
6. Support for iterator to traverse data in sorted order.
7. Easy to use and include in existing projects. Requires only an Arduino with an SD card.
8. Open source license. Free to use for commerical and open source projects.

## Code Files

* test_sbits.h - test file demonstrating how to get, put, and iterate through data in index
* main.cpp - main Arduino code file
* sbits.h, sbits.c - implementation of SBITS index structure supporting arbitrary key-value data items

## Support Code Files

* serial_c_iface.h, serial_c_iface.cpp - allows printf() on Arduino
* sd_stdio_c_iface.h, sd_stdio_c_iface.h - allows use of stdio file API (e.g. fopen())

## Documentation

A paper describing SBITS use for time series indexing is [available from the publisher](https://www.scitepress.org/Link.aspx?doi=10.5220/0010318800920099) and a [pre-print is also available](SBITS_time_series_index.pdf).

## Usage

### Setup Index and Configure Memory

```c
/* Configure SBITS state */
sbitsState* state = (sbitsState*) malloc(sizeof(sbitsState));

state->recordSize = 16;
state->keySize = 4;
state->dataSize = 12;
state->pageSize = 512;
uint8_t M = 4;					/* Using an index requires 4 buffers. Minimum memory without an index is 2 buffers. */
state->bufferSizeInBlocks = M;
state->buffer  = malloc((size_t) state->bufferSizeInBlocks * state->pageSize);    
int8_t* recordBuffer = (int8_t*) malloc(state->recordSize);   

/* Address level parameters */
state->startAddress = 0;
state->endAddress = state->pageSize * 1000;  /* Decide how much memory to use for data storage */	
state->eraseSizeInPages = 4;
state->parameters = SBITS_USE_MAX_MIN | SBITS_USE_BMAP | SBITS_USE_INDEX;

/* Initialize SBITS structure with parameters */
sbitsInit(state);
```

### Insert (put) items into tree

```c
/* keyPtr points to key to insert. dataPtr points to associated data value. */
sbitsPut(state, (void*) keyPtr, (void*) dataPtr);
```

### Query (get) items from tree

```c
/* keyPtr points to key to search for. dataPtr must point to pre-allocated space to copy data into. */
int8_t result = sbitsGet(state, (void*) keyPtr, (void*) dataPtr);
```

### Iterate through items in tree

```c
/* Iterator with filter on keys */
sbitsIterator it;
int32_t *itKey, *itData;

uint32_t minKey = 1, maxKey = 1000;     
it.minKey = &minKey; 
it.maxKey = &maxKey;
it.minData = NULL;
it.maxData = NULL;    

sbitsInitIterator(state, &it);

while (sbitsNext(state, &it, (void**) &itKey, (void**) &itData))
{                      
	/* Process record */	
}

/* Iterator with filter on data */       
it.minKey = NULL;    
it.maxKey = NULL;
uint32_t minData = 90, maxData = 100;  
it.minData = &minData;
it.maxData = &maxData;    

sbitsInitIterator(state, &it);

while (sbitsNext(state, &it, (void**) &itKey, (void**) &itData))
{                      
	/* Process record */	
}
```
#### Ramon Lawrence<br>University of British Columbia Okanagan



