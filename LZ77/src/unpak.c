#include "base.h"
#include "circularBuffer.h"



boolean verifyPakHeader() {
	char shouldBePakExtension[4];

	fseek(sourceFile, 0, SEEK_SET);
	fread(shouldBePakExtension, 1, 4, sourceFile);

	return (boolean) (shouldBePakExtension[0] == 'P' && shouldBePakExtension[1] == 'A' &&
					 shouldBePakExtension[2] == 'K' && shouldBePakExtension[3] == '\0');
}

void setSourceFileInformation() {
	unsigned char srcExtLength, offset;
	
	fread(&offset, 1, member_size(HeaderPak, offset), sourceFile); 
	sourceFileExtension = (char*) malloc( (srcExtLength = (offset - after_offset_size)) + 1 );		// +1 for \0 character

	fread(&twNecessaryBits, 1, member_size(HeaderPak, position_bits), sourceFile);
	fread(&lhNecessaryBits, 1, member_size(HeaderPak, coincidences_bits), sourceFile);

	fseek(sourceFile, 1, SEEK_CUR);			// Advance 1 byte (ignore min_coincidences)
	fread(&tokensCount, 1, member_size(HeaderPak, tokens), sourceFile);

	fread(sourceFileExtension, 1, srcExtLength, sourceFile);	// Set extension on sourceFileExtension
	*(sourceFileExtension + srcExtLength) = '\0';				// End the string
}

char* getItMax(PRingBufferChar buffer, unsigned int distance) {

	if( buffer->finish > buffer->start ) {
		return buffer->finish - distance;
	}

	return ((buffer->finish - distance) >= buffer->data)
		   ? (buffer->finish - distance)
		   : (buffer->data + buffer->buffer_size) - (distance - (buffer->finish - buffer->data));
}

void doDecompression() {

	PRingBufferChar buffer = newInstance( getNecessaryMaskFor(twNecessaryBits), getNecessaryMaskFor(lhNecessaryBits) + 1 );

	int c = fgetc(sourceFile);
	unsigned char theChar;

	phraseTokenBits = twNecessaryBits + lhNecessaryBits + 1;

	//
	// 1st Stage: Process & Decompress
	//

	while( TRUE ) 
	{
		if( c == EOF ) {
			break;
		}

		else {
			unsigned char ch = c;

			boolean decodeAsPhrase = (boolean) ( ch & (1 << ((CHAR_SIZE_BITS - 1) - freePtr)) );		// With 1 bit, we can know which token to decode.

			if( !decodeAsPhrase ) {
				unsigned char shifts  = 0;
				theChar = 0;				
			
				// +1 for 0 bit token
				theChar = ( ch & getNecessaryMaskFor( CHAR_SIZE_BITS - (shifts = freePtr + 1) ) ) << shifts;
				c = fgetc(sourceFile);
			
				if( c == EOF ) break;
				else {
					ch = c;
					theChar |= ch >> (CHAR_SIZE_BITS - shifts);
					putTextWindow(buffer, theChar);
					fputc(theChar, destFile);

					if( freePtr == (CHAR_SIZE_BITS - 1) ) {
						c = fgetc(sourceFile);
						if( c == EOF ) break;
					}
				}

				freePtr = (freePtr + CHAR_TOKEN_SIZE_BITS) % CHAR_SIZE_BITS;
			}

			else {
				
				unsigned char remainingBits = phraseTokenBits;
				unsigned char freeBits = (CHAR_SIZE_BITS - freePtr + 1);
				unsigned int distance, occurrences;

				char tmp;
				char* itMax;

				if ( (tmp = phraseTokenBits - freeBits) < 0 ) {

					//
					// The phrase fits the buffer and leave bits available
					//

					distance = (ch &  (getNecessaryMaskFor(twNecessaryBits) << (tmp = (CHAR_SIZE_BITS - twNecessaryBits - (freePtr + 1))))  ) >> tmp;
					remainingBits -= twNecessaryBits;
					occurrences = (ch &  (getNecessaryMaskFor(lhNecessaryBits) << (tmp = (remainingBits - lhNecessaryBits))   )) >> tmp;
				}

				else {

				}

				itMax = getItMax(buffer, distance);

				for( ; occurrences > 0; occurrences--) {
					char character = *itMax;
					incrementPtr(buffer, itMax);

					putTextWindow(buffer, character);
					fputc(character, destFile);
				}
			}
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