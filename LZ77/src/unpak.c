#include "base.h"
#include "circularBuffer.h"


//
// This method verify if the file was compacted by a pak compressor, by verifying the first 4 bytes
// and comparing if they match with PAK extension.
// Should be the first (1st) method to be called after we have destination file ready to be written.
//
boolean 
__forceinline
verifyPakHeader() {
	char shouldBePakExtension[4];

	fseek(sourceFile, 0, SEEK_SET);
	fread(shouldBePakExtension, 1, 4, sourceFile);

	return (boolean) (shouldBePakExtension[0] == 'P' && shouldBePakExtension[1] == 'A' &&
					 shouldBePakExtension[2] == 'K' && shouldBePakExtension[3] == '\0');
}



//
// This method sets an important information to decompressor know how to decompress.
// Sets TwBits, LhBits, phraseBits, Tokens, and the original extension.
// Should be the second (2nd) method to be called.
//
void 
__stdcall
setSourceFileInformation() 
{
	unsigned char srcExtLength, offset;
	
	fread(&offset, 1, member_size(HeaderPak, offset), sourceFile); 
	sourceFileExtension = (char*) malloc( (srcExtLength = (offset - after_offset_size)) + 1 );		// +1 for \0 character

	fread(&twNecessaryBits, 1, member_size(HeaderPak, position_bits), sourceFile);
	fread(&lhNecessaryBits, 1, member_size(HeaderPak, coincidences_bits), sourceFile);

	fseek(sourceFile, 1, SEEK_CUR);			// Advance 1 byte (ignore min_coincidences)
	fread(&tokensCount, 1, member_size(HeaderPak, tokens), sourceFile);

	fread(sourceFileExtension, 1, srcExtLength, sourceFile);	// Set extension on sourceFileExtension
	*(sourceFileExtension + srcExtLength) = '\0';				// End the string

	phraseTokenBits = twNecessaryBits + lhNecessaryBits + 1;
}



//
// This method returns the pointer of the itMax.
// Internally we sees if the finish is ahead of the starting, or the starting is ahead of the finish
// returning the correct pointer based on the distance.
//
char*
__stdcall
getItMax(
	__in PRingBufferChar buffer,
	__in unsigned int distance
) {

	if( buffer->finish > buffer->start ) {
		return buffer->finish - distance;
	}

	return ((buffer->finish - distance) >= buffer->data)
		   ? (buffer->finish - distance)
		   : (buffer->data + buffer->buffer_size) - (distance - (buffer->finish - buffer->data));
}




void 
__stdcall
doDecompression() 
{
	PRingBufferChar buffer = newInstance(
		getMask(twNecessaryBits), 
		getMask(lhNecessaryBits) + 1	// Ignored
	);

	int c = fgetc(sourceFile);

	//
	// 1st Stage: Process & Decompress
	//

	while( c != EOF && tokensCount > 0 ) 
	{
		unsigned char ch = c;		// Used for safety shifts
		boolean isCharacter = (boolean) !(c & (1 << (CHAR_SIZE_BITS - 1 - freePtr)));
		--tokensCount;

		if( isCharacter ) {
			unsigned char theChar = 0;
			unsigned char shifts  = 0;			
			
			// +1 for 0 bit token
			theChar = ( ch & getMask( CHAR_SIZE_BITS - (shifts = freePtr + 1) ) ) << shifts;
			c = fgetc(sourceFile);		// != EOF 
			
			ch = c;
			theChar |= ch >> (CHAR_SIZE_BITS - shifts);
			putTextWindow(buffer, theChar);			// Put character on dictionary and write to destination
			fputc(theChar, destFile);

			if( freePtr == (CHAR_SIZE_BITS - 1) ) {

				// When this happends, we need to load another byte from source, to continue processing
				c = fgetc(sourceFile);
				if( c == EOF ) break;
			}

			freePtr = (freePtr + CHAR_TOKEN_SIZE_BITS) % CHAR_SIZE_BITS;
		}

		else {
			unsigned int distance = 0, occurrences = 0;
			unsigned char remainingDistanceBits = twNecessaryBits;
			unsigned char remainingOccurrencesBits = lhNecessaryBits;
			unsigned char ptr = freePtr + 1;
			unsigned int idx;


			char* itMax;

			//
			// Set distance
			// 
			while( remainingDistanceBits > 0 ) {

				if( ptr == CHAR_SIZE_BITS ) {	// If we are pointing to limits we need load another byte
					c = fgetc(sourceFile);		// != EOF because there are bits for lookahead
					ptr = 0;
				}

				--remainingDistanceBits;

				if( (c & (1 << ( CHAR_SIZE_BITS - 1 - ptr ))) > 0 )
					distance |= 1 << remainingDistanceBits;

				ptr++;
			}	

			//
			// Set occurrences
			// 
			while ( remainingOccurrencesBits > 0 ) {

				if( ptr == CHAR_SIZE_BITS ) {	// If we are pointing to limits we need load another byte
					c = fgetc(sourceFile);
					if( c == EOF ) break;
					ptr = 0;
				}

				--remainingOccurrencesBits;

				if( (c & (1 << ( CHAR_SIZE_BITS - 1 - ptr ))) > 0 )
					occurrences |= 1 << remainingOccurrencesBits;

				ptr++;
			}

			//
			// Here we have the distance and occurrences filled.
			//

			if( ptr == CHAR_SIZE_BITS ) {		// If we are pointing to limits we need load another byte
				c = fgetc(sourceFile);
			}


			//
			// Fill the dictionary
			//

			itMax = getItMax(buffer, distance);

			for(idx = 0; idx <= occurrences; idx++) {	// = is needed because for 0 we have 1 occurrence at least.
				putTextWindow(buffer, *itMax);
				fputc(*itMax, destFile);

				// Advance itMax
				incrementPtr(buffer, itMax)
			}

			freePtr = (freePtr + phraseTokenBits) % CHAR_SIZE_BITS;
		}
	}
}


int main(int argc, char* argv[]) {

	boolean success;

	if( argc != 2 ) {
		fprintf(stderr, "Arguments invalid: Should be unpak <filename>");
		return ARGUMENTS_INVALID;
	}

	if( !trySetSourceFile(argv[1]) ) {
		fprintf(stderr, "File to decompress not found \n");
		return SOURCE_FILE_NOT_FOUND;
	}

	//
	// Before we can create the destination file,
	// we must know what's the extension of original file (we must read the header)
	//

	if( !verifyPakHeader() ) {
		fprintf(stderr, "File to decompress is not a pak file \n");
		return SOURCE_FILE_NOT_PAK_FILE;
	}

	setSourceFileInformation();

	//
	// At this point the source file ptr is pointing to the first token (1st byte)
	// 

	sourceFileExtension = trySetDestinationFile(argv[1], sourceFileExtension, &success);

	if( !success ) {
		fprintf(stderr, "Error while creating decompressed file \n");
		return DEST_FILE_ERROR_CREATING;
	}



	doDecompression();


	//
	// Close kernel handle to the file
	//


	fclose(sourceFile);
	fclose(destFile);


	free(sourceFileExtension);

	return RETURN_OK;

}