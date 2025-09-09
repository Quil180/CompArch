#include "stdio.h"
#include "stdint.h"

int main()
{
	// Variables
	int32_t sampleInt = 0x12131415;
	float sampleFloat = 34.73;
	// Pointer for individual finding of 2 bytes
	uint8_t *bytePointer = (uint8_t *)&sampleInt; // typecasting for funny reasons
	
	// Printing out the thing we are wanting
	if (printf("Printing Bits for Integer 0x12131415...\n") != 40)
	{
		return 1;
	}
	// Individually printing out 2 bytes at a time for Integer
	for (int i = 0; i < 4; i++)
	{
		if (printf("Byte %d: 0x%.2X\n", i+1, bytePointer[i]) != 13)
		{
			return 1;
		}
	}

	// Printing out the thing we are wanting x2
	if (printf("\nPrinting Bits for Float 34.73...\n") != 34)
	{
		return 1;
	}
	// Moving pointer to the float
	bytePointer = (uint8_t *)&sampleFloat;
	// Individually printing out 2 bytes at a time for Floats
	for (int i = 0; i < 4; i++)
	{
		if (printf("Byte %d: 0x%.2X \n", i+1, bytePointer[i]) != 14)
		{
			return 1;
		}
	}
	// Return success code
	return 0;
}
