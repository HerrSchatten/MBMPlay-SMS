// cnvrtsfx.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <tgmath.h>

FILE *fIN;
FILE *fOUT;

int c = 0;
unsigned char psgFrqL = 0;
unsigned char psgFrqH = 0;
unsigned char fmFrqL = 0;
unsigned char fmFrqLPrev = 255;
unsigned char fmFrqH = 0;
unsigned char fmFrqHPrev = 255;
unsigned char volume = 15;
unsigned char volumePrev = 255;
unsigned char latchType = 0; 				// 0 = tone, 16 = volume

unsigned char getOctave(double frequency) {

	if (frequency < 130.8125) {
		return 2;
	}
	else if (frequency < 261.625) {
		return 3;
	}
	else if (frequency < 523.25) {
		return 4;
	}
	else if (frequency < 1046.5) {
		return 5;
	}
	else if (frequency < 2093) {
		return 6;
	}
	else {
		return 7;
	}

}

void dumpFrame(void) {

	double freq = 3579545 / (2*((psgFrqH*16)+psgFrqL)*16);
	if (freq > 6202.4) {
		freq = 6202.4;
	} 
	unsigned char oct = getOctave(freq);
	int fNum = (freq*262144/49716) / (pow(2,(oct-1)));

	fmFrqH = fNum / 256;
	fmFrqL = fNum % 256;

	if (fmFrqL != fmFrqLPrev) {
		fputc(16,fOUT);
		fputc(fmFrqL,fOUT);
		fmFrqLPrev = fmFrqL;
	}
	if (fmFrqH != fmFrqHPrev) {
		fputc(32,fOUT);
		fputc((16|(oct<<1)|fmFrqH),fOUT);
		fmFrqHPrev = fmFrqH;
	}
	if (volume != volumePrev) {
		fputc(48,fOUT);
		fputc((volume|128),fOUT);
		volumePrev = volume;
	}

}

int main (int argc, char *argv[]) {

	if (argc!=3) {
		printf ("Usage: cnvrtsfx inputfile.PSG outputfile.BIN\n");
		return EXIT_FAILURE;
	}

	fIN=fopen(argv[1],"rb");
	if (fIN == 0) {
		printf("Can't open %s\n", argv[1]);
		return EXIT_FAILURE;
	}
	fOUT=fopen(argv[2],"w+b");
	if (fOUT == 0) {
		printf("Can't open %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	while ( (c=fgetc(fIN)) != EOF ) {
		if ( (c>=192) && (c<224) ) {
			if ( (c&16) == 0 ) {			// latch: tone
				psgFrqL = c & 15;
			}
			else {							// latch: volume
				volume = c & 15;
			}
			latchType = c & 16;
		}
		else if ( (c>=64) && (c<128) ) {	// data byte
			if ( latchType == 0 ) {
				psgFrqH = c & 63;
			}
			else {
				volume = c & 15;
			}
		}
		else if ( (c>=56) && (c<64) ) {
			dumpFrame();
			fputc(((c&7)+1),fOUT);			// set frame end marker + wait frames
		}
		else if ( c == 1 ) {
			fputc(255,fOUT);				// set loop point
		}
		else if ( c == 0 ) {
			dumpFrame();
			fputc(0,fOUT);					// set sfx end marker
		}
		else {
			printf("ERROR: Invalid PSG data: %d\n", c);
			return EXIT_FAILURE;
		}
	} 

	fclose (fIN);
	fclose (fOUT);
  
	printf ("Info: done!\n");
	return EXIT_SUCCESS;
}
