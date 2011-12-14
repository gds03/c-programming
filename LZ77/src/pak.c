#include "base.h"
#include "circularBuffer.h"




// ********************************************************************
//					Tests related methods
// ********************************************************************

void TestCircularBuffer()
{
	PRingBufferChar buffer = newInstance(NULL, NULL);
	
	// 
	// Fill lookahead
	fillLookahead(buffer, 'c');
	fillLookahead(buffer, 'o');
	fillLookahead(buffer, 'o');
	fillLookahead(buffer, 'o');

	putChar(buffer, 'k');
	putChar(buffer, 'b');
	putChar(buffer, 'o');
	putChar(buffer, 'o');
	putChar(buffer, 'o');
	putChar(buffer, 'k');
	putChar(buffer, 'o');
	putChar(buffer, 'k');
	putChar(buffer, 'o');
	putChar(buffer, 'k');
	putChar(buffer, 'o');
	putChar(buffer, 'k');

	putChar(buffer, 'x');
	putChar(buffer, 'y');

	decrementLookahead(buffer, 3);

	deleteInstance(buffer);
}

//
// Searches the best match on textwindow within the buffer for
// lookahead word.
// Returns NULL if no matches were found, or returns a pointer to the best match.
// occurrences contains the number of matches (0 when nothing's found)
//
char* 
__stdcall
searchItMax(
	__in PRingBufferChar buffer,
	__inout unsigned int* occurrences
) {
	char* itMax = NULL;		// Pointer to the best phrase on dictionary
	unsigned int itMaxOccurrences = 0;

	char* match;
	unsigned int matchOccurrences = 0;	

	char* twPtr = buffer->start;
	boolean twCovered = FALSE;

	//
	// Search lookahead on text window
	// 

	while(!twCovered && twPtr != buffer->endTw) 
	{	

		//
		// Looking for first character first..
		//

		if(*twPtr == *buffer->endTw) {

			//
			// If we are here, we have the first match of the first character on textwindow

			char* twPtrIter = twPtr;
			char* lhPtrIter = buffer->endTw;

			match = twPtr;	// Set a reference to the first match

			do {
				matchOccurrences++;

				if(twPtrIter == buffer->endTw) {
					
					// 
					// Here we don't do a break because the phrase can be included on the lookahead
					//
					twCovered = TRUE;	
				}

				incrementPtr(buffer, twPtrIter)
				incrementPtr(buffer, lhPtrIter)
			}
			while( (matchOccurrences < buffer->lookahead_size) && *twPtrIter == *lhPtrIter );

			//
			// Establish the higher match here..
			// 

			if( matchOccurrences > itMaxOccurrences ) {
				itMaxOccurrences = matchOccurrences;
				itMax = match;

				if( matchOccurrences == buffer->lookahead_size ) {

					//
					// If we have the greater phrase match we don't need search more and so
					// turn the algorithm more efficient.
					//

					break;
				}
			}
		}

		//
		// Pos-for body

		incrementPtr(buffer, twPtr)
		matchOccurrences = 0;
	}	

	*occurrences = itMaxOccurrences;
	return itMax;
}



//
// Typically called by addBuffer.
// This method returns a integer where the lower bits are the occurrences, the next are the distance and
// the next are the bit at 1 (phrase bit token). For example:
// 1XXXXXXXXYYYY - 8 bits for distance (256 dictionary entries and 16 entries for lookahead
//
unsigned int
__forceinline 
formatPhrase(
	__in unsigned int distance,
	__in unsigned int occurrences
) {
	unsigned char remainingBits = phraseTokenBits;
	unsigned int phraseCh = 0;

	phraseCh = 1 << (--remainingBits);
	phraseCh |= distance << (remainingBits -= twNecessaryBits);
	phraseCh |= occurrences;

	return phraseCh;
}


//
// (One of the most important algorithms of the application)
// Typically called by doCompression.
// This algorithm have 2 possibles codifications.
// 1. Character token (static 9 bits)
// 2. Phrase token (dynamic x bits)
//
// For Character token:
//		- Union fileChar buffer with higher bits of ch.
//      - call WriteToFile
//		- Set fileChar buffer with lower bits of ch.
//		- Verify if freePtr is pointing to 7 (means that we must call WriteToFile) because
//		  the buffer will be filled and must be written to destination.
//
// For Phrase token:
//		- Get phrase token based on distance and occurrences.
//		- Verify if it is available space on the buffer that we can union the phrase token 
//		  without writing to the file.
//			If is: We just make a union.
//			If not:	We union the higher bits of the phrase and start trying to fill the buffer with
//					8 char bits. When there is impossible, we just set the restant bits to the higher part
//					of the buffer

void 
__stdcall
addBuffer(
	__in boolean phrase,
	__in unsigned int distance,
	__in unsigned int occurrences,
	__in unsigned char ch
) 
{
	if( !phrase ) {

		unsigned char shifts = freePtr + 1;

		fileChar |= ch >> shifts;
		writeToFile();

		fileChar = ( ch & getMask(shifts) ) << (CHAR_SIZE_BITS - shifts);

		if( freePtr == (CHAR_SIZE_BITS - 1) ) {
			writeToFile();
		}

		freePtr = (freePtr + CHAR_TOKEN_SIZE_BITS) % CHAR_SIZE_BITS;
	}

	else {

		unsigned int phraseCh = formatPhrase(distance, occurrences);
		unsigned char freeBits = (CHAR_SIZE_BITS - freePtr);
		char tmp;

		if( (tmp = phraseTokenBits - freeBits) < 0 ) {
		
			//
			// The phrase fits the buffer and leave bits available
			//
					
			fileChar |= phraseCh << (CHAR_SIZE_BITS - phraseTokenBits);
		}

		else {
			unsigned char remainingBits = phraseTokenBits;
			fileChar |= phraseCh >> (phraseTokenBits - freeBits);

			remainingBits -= freeBits;	
			writeToFile();

			while( (remainingBits / CHAR_SIZE_BITS) >= 1 ) {
				unsigned char shifts = (remainingBits - CHAR_SIZE_BITS);

				fileChar = ( phraseCh & (getMask(CHAR_SIZE_BITS) << shifts) ) >> shifts;
				writeToFile();
				remainingBits -= CHAR_SIZE_BITS;
			}

			if(remainingBits > 0) {
				
				fileChar = ( phraseCh & getMask(remainingBits) ) << (CHAR_SIZE_BITS - remainingBits);
			}
		}

		freePtr = (freePtr + phraseTokenBits) % CHAR_SIZE_BITS;
	}
}


//
// This method fill the lookahead with the first file characters.
// Should be called only when algorithm is starting..
//
void 
__forceinline 
fillLookaheadBuffer(
	__in PRingBufferChar buffer
) {
	unsigned int i = 0;
	char c;

	while( i++ < buffer->lookahead_size  &&  (c = fgetc(sourceFile)) != EOF)
		fillLookahead(buffer, c);
}


//
// Before you have all data compressed, you should call this method to write 
// the pak header within file.
// Typically this method should be called after you compressed all the file because you must
// have the tokens count, and you only know the count when you have finished to compress the file.
//
void 
__stdcall 
writePakHeader() {

	PHeaderPak header = (PHeaderPak) malloc(sizeof(HeaderPak));
	unsigned char srcFileExtLength = strlen(sourceFileExtension);

	header->id[0] = 'P';
	header->id[1] = 'A';
	header->id[2] = 'K';
	header->id[3] = '\0';

	header->offset = sizeof(HeaderPak) + srcFileExtLength - HEADER_OFFSET;
	header->position_bits = twNecessaryBits;
	header->coincidences_bits = lhNecessaryBits;
	header->min_coincidences = 1;
	header->tokens = tokensCount;

	fseek(destFile, 0, SEEK_SET);
	fwrite(header, sizeof(HeaderPak), 1, destFile);
	fwrite(sourceFileExtension, srcFileExtLength, 1, destFile);

	free(header);
}

//
// This method must be called before codification.
// Where we set the twNecessaryBits, lhNecessaryBits and phraseTokenBits.
//
void
__forceinline
setDynamicBits(
	__in unsigned int buffer_size,
	__in unsigned int lookahead_size
) {
	twNecessaryBits = getBits(buffer_size - lookahead_size);
	lhNecessaryBits = getBits(lookahead_size) - 1;			// -1 is necessary because 00 represent 1 occurrence
	phraseTokenBits = twNecessaryBits + lhNecessaryBits + 1;			// +1 for the phrase highest bit
}


//
// This method do the compression algorithm, that is: based on actual lookahead buffer,
// search on textwindow a phrase matching the max size of lookahead. If nothing is found then
// generate a character token and put the next character from source within lookahead.
// If is, generate a phrase token and get maching characters from destination on lookahead.
// 
void
__stdcall 
doCompression(
	__in const char* buffer_size, 
	__in const char* lookahead_size
) {
	unsigned int buf_size = atoi(buffer_size);
	unsigned int lok_size = atoi(lookahead_size);

	int c = NULL;
	char* itMax;
	unsigned int itMaxOccurrences = 0;
	boolean stopCompress = FALSE;

	PRingBufferChar buffer = newInstance(buf_size, lok_size);
	setDynamicBits(buf_size, lok_size);
	
	//
	// 1st Stage: Fill lookahead
	// 

	fillLookaheadBuffer(buffer);	

	//
	// 2nd Stage: Process & Compress
	// 

	while( !stopCompress )
	{
		unsigned int	 distance	  = 0;	
		unsigned int	 matchesFound = 1;
		
		boolean codeAsPhrase = FALSE;

		//
		// Search in dictionary
		// 

		itMax = searchItMax(buffer, &itMaxOccurrences);

		if( itMax != NULL ) {		
			
			//
			// As the phrase token is dynamic, we must decide which codification is lower.
			// Code the sequence as a phrase or separate character tokens.
			//
			
			if( phraseTokenBits < (CHAR_TOKEN_SIZE_BITS * itMaxOccurrences) ) {
				codeAsPhrase = TRUE;
				matchesFound = itMaxOccurrences;

				//
				// We only need the distance when token is a phrase
				//

				if(itMax < buffer->endTw) {
					distance = buffer->endTw - itMax;
				} 
				else {
					distance = ((buffer->data + buffer->buffer_size) - itMax) + (buffer->endTw - buffer->data);
				}
			}	
		}

		//
		// Put character on buffer and if necessary transfer 
		// to destination file
		// 

		++tokensCount;
		addBuffer(codeAsPhrase, distance, matchesFound - 1, *buffer->endTw);
		

		//
		// Transfer from source file, matchesFound characters
		// 

		for( ; matchesFound > 0; matchesFound--) {

			if( c != EOF ) {

				//
				// If we not touch the end of file we load the next char to c
				// 

				c = fgetc(sourceFile);
			}

			if( c != EOF ) {
				// 
				// Put char on ringbuffer and advance lookahead
				// 

				putChar(buffer, c);
			} 
			else {
				
				//
				// We don't have any character to get from source, 
				// so we decrement the lookahead matchesFound times
				// 

				decrementLookahead(buffer, matchesFound);

				if(buffer->endTw == buffer->finish)
					stopCompress = TRUE;

				//
				// Stop for loop always (we don't want to look matchesFound
				// because we already decremented the lookahead)
				//

				break;	
			}
		}
	}

	//
	// 3rd Stage: We must consult if is data on the buffer and if it is,
	// we must write to the file the final bits
	// 

	writeLastByteIfNecessary();
	
	//
	// Free ringbuffer allocated memory!
	// 

	deleteInstance(buffer);
}



int main(int argc, char *argv[])
{

	//
	// At the first stage we must verify if arguments passed to
	// the application, have filename to compress.
	//

	boolean success;

	if( argc < 4 ) {
		fprintf(stderr, "Arguments invalid: Should be pak <filename> <dictionary_size> <lookahead_size>");
		return ARGUMENTS_INVALID;
	}

	if( !trySetSourceFile(argv[1]) ) {
		fprintf(stderr, "File to compress not found \n");
		return SOURCE_FILE_NOT_FOUND;
	}

	sourceFileExtension = trySetDestinationFile(argv[1], "pak", &success);
	
	if( !success ) {
		fprintf(stderr, "Error while creating pak file \n");
		return DEST_FILE_ERROR_CREATING;
	}

	//
	// Here we got the file destination, file source handle that must be closed at the end.
	// The origin extension memory allocated to be setted on file header must be freed at the end.
	// 
	
	positionFileDestination(destFile, strlen(sourceFileExtension));

	doCompression(argv[2], argv[3]);

	writePakHeader();

	//
	// Close kernel handle to the file
	//


	fclose(sourceFile);
	fclose(destFile);


	free(sourceFileExtension);

	return RETURN_OK;
}

