#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define OUTPUT_FILE_NAME "Output.wav"
#define FILE_HEADER_SIZE 44
#define BYTES_PER_SAMPLE 2
#define BITS_PER_SAMPLE ((BYTES_PER_SAMPLE) * 8)
#define DATA_BUFF_SIZE 1000
#define SAMPLE_RATE 48000
#define CHANNELS 2

#define SIGNAL_FREQUENCY 400
#define START_AMPLITUDE 0
#define SIGNAL_TIME 3	//in seconds

#define PI 3.14159265358979323846


typedef struct {
	uint8_t fileFormat[4];
	uint32_t fileSize;
	uint8_t fileTypeHeader[4];
	uint8_t formatChunkMarker[4];
	uint32_t formatDataLength;
	uint16_t formatType;
	uint16_t channels;
	uint32_t sampleRate;
	uint32_t byterate;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
	uint8_t dataChunkHeader[4];
	uint32_t dataSize;
} WavHeader;

typedef struct {
	double frequency;
	double currAmplitude;
	double maxAmplitude;
	double amplitudeStep;
	double signalTime;
	uint32_t timeCounter;
	uint32_t samplesNum;
} Signal;


double dBtoGain(double dB);
int16_t doubleToFixed15(double x);
void signalInitialization(Signal *signal);
void fileHeaderInitialization(WavHeader *header, Signal *signal);
FILE * openFile(char *fileName, _Bool mode);
void writeHeader(WavHeader *headerBuff, FILE *outputFilePtr);
int16_t generateToneSignal(Signal *signal);
void run(Signal *signal, FILE *outputFilePtr);

int main()
{
	Signal signal;
	signalInitialization(&signal);

	WavHeader header;
	fileHeaderInitialization(&header, &signal);
	FILE *outputFilePtr = openFile(OUTPUT_FILE_NAME, 1);
	writeHeader(&header, outputFilePtr);

	run(&signal, outputFilePtr);
	fclose(outputFilePtr);

	return 0;
}


double dBtoGain(double dB)
{
	return pow(10, dB / 20.0);
}

int16_t doubleToFixed15(double x)
{
	if (x >= 1)
	{
		return INT16_MAX;
	}
	else if (x < -1)
	{
		return INT16_MIN;
	}

	return (int16_t)(x * (double)(1LL << 15));
}

void signalInitialization(Signal *signal)
{
	signal->frequency = SIGNAL_FREQUENCY;
	signal->currAmplitude = 0;
	signal->maxAmplitude = dBtoGain(START_AMPLITUDE);
	signal->signalTime = SIGNAL_TIME;
	signal->timeCounter = 0;
	signal->samplesNum = (uint32_t)round(signal->signalTime * SAMPLE_RATE);
	signal->amplitudeStep = signal->maxAmplitude / signal->samplesNum;
}

void fileHeaderInitialization(WavHeader *header, Signal *signal)
{
	header->fileFormat[0] = 'R';
	header->fileFormat[1] = 'I';
	header->fileFormat[2] = 'F';
	header->fileFormat[3] = 'F';

	header->fileTypeHeader[0] = 'W';
	header->fileTypeHeader[1] = 'A';
	header->fileTypeHeader[2] = 'V';
	header->fileTypeHeader[3] = 'E';

	header->formatChunkMarker[0] = 'f';
	header->formatChunkMarker[1] = 'm';
	header->formatChunkMarker[2] = 't';
	header->formatChunkMarker[3] = ' ';

	header->dataChunkHeader[0] = 'd';
	header->dataChunkHeader[1] = 'a';
	header->dataChunkHeader[2] = 't';
	header->dataChunkHeader[3] = 'a';

	header->formatDataLength = 16;
	header->formatType = 1;
	header->channels = CHANNELS;
	header->sampleRate = SAMPLE_RATE;
	header->bitsPerSample = 16;
	header->byterate = (SAMPLE_RATE * BITS_PER_SAMPLE * CHANNELS) / 8;
	header->blockAlign = (BITS_PER_SAMPLE * CHANNELS) / 8;
	header->dataSize = signal->samplesNum * header->blockAlign;
	header->fileSize = header->dataSize + FILE_HEADER_SIZE;
}

FILE * openFile(char *fileName, _Bool mode)		//if 0 - read, if 1 - write
{
	FILE *filePtr;

	if (mode == 0)
	{
		if ((filePtr = fopen(fileName, "rb")) == NULL)
		{
			printf("Error opening input file\n");
			system("pause");
			exit(0);
		}
	}
	else
	{
		if ((filePtr = fopen(fileName, "wb")) == NULL)
		{
			printf("Error opening output file\n");
			system("pause");
			exit(0);
		}
	}

	return filePtr;
}

void writeHeader(WavHeader *headerBuff, FILE *outputFilePtr)
{
	if (fwrite(headerBuff, FILE_HEADER_SIZE, 1, outputFilePtr) != 1)
	{
		printf("Error writing output file (header)\n");
		system("pause");
		exit(0);
	}
}

int16_t generateToneSignal(Signal *signal)
{
	int16_t res = doubleToFixed15(signal->currAmplitude * sin(2 * PI * signal->frequency *
		(double)signal->timeCounter / SAMPLE_RATE));

	signal->currAmplitude += signal->amplitudeStep;
	signal->timeCounter++;

	return res;
}

void run(Signal *signal, FILE *outputFilePtr)
{
	uint32_t i;
	uint32_t samples;
	uint32_t counter = signal->samplesNum * CHANNELS;
	int16_t *buff = malloc(BITS_PER_SAMPLE * DATA_BUFF_SIZE);

	while (counter > 0)
	{
		if (counter >= DATA_BUFF_SIZE)
		{
			samples = DATA_BUFF_SIZE;
			counter -= DATA_BUFF_SIZE;
		}
		else
		{
			samples = counter;
			counter = 0;
		}

		for (i = 0; i <= samples - CHANNELS; i += CHANNELS)
		{
			buff[i] = generateToneSignal(signal);
			buff[i + 1] = buff[i];
		}

		fwrite(buff, BYTES_PER_SAMPLE, samples, outputFilePtr);
	}

	free(buff);
}
