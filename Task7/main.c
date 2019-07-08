#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define OUTPUT_FILE_NAME "Output.wav"
#define FILE_HEADER_SIZE 44
#define BYTES_PER_SAMPLE 2
#define BITS_PER_SAMPLE ((BYTES_PER_SAMPLE) * 8)
#define SAMPLE_RATE 48000
#define CHANNELS 2

#define SIGNAL_FREQUENCY 400
#define MAX_AMPLITUDE 0		//in dB
#define SIGNAL_TIME 3	//in seconds

#define	EXPANDER_THRESHOLD 0.06
#define COMPRESSOR_THRESHOLD 0.5
#define LIMITER_THRESHOLD 0.9
#define RATIO 2

#define RING_BUFF_SIZE 128
#define DATA_BUFF_SIZE 1000		//must be bigger than RING_BUFF_SIZE

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

typedef struct {
	uint8_t currNum;
	int16_t samples[RING_BUFF_SIZE];
} RingBuff;


double dBtoGain(double dB);
int16_t doubleToFixed15(double x);
int32_t doubleToFixed29(double x);
int32_t	Saturation(int64_t x);
int32_t roundFixed58To29(int64_t x);
int32_t Add(int32_t x, int32_t y);
int32_t Sub(int32_t x, int32_t y);
int32_t Mul(int32_t x, int32_t y);
int32_t Abs(int32_t x);
int32_t Div(int32_t x, int32_t y);

void signalInitialization(Signal *signal);
void fileHeaderInitialization(WavHeader *header, Signal *signal);
FILE * openFile(char *fileName, _Bool mode);
void writeHeader(WavHeader *headerBuff, FILE *outputFilePtr);

void ringInitialization(RingBuff *ringBuff, int16_t *samplesBuff);
int16_t generateToneSignal(Signal *signal);
int16_t signalProc(RingBuff *ringBuff);
int16_t signalProcDouble(RingBuff *ringBuff);
void run(Signal *signal, RingBuff *ringBuff, FILE *outputFilePtr);

static int16_t buff[DATA_BUFF_SIZE * CHANNELS];

int main()
{
	Signal signal;
	RingBuff ringBuff[2];
	signalInitialization(&signal);

	WavHeader header;
	fileHeaderInitialization(&header, &signal);
	FILE *outputFilePtr = openFile(OUTPUT_FILE_NAME, 1);
	writeHeader(&header, outputFilePtr);

	run(&signal, ringBuff, outputFilePtr);
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

int32_t doubleToFixed29(double x)
{
	if (x >= 4)
	{
		return INT32_MAX;
	}
	else if (x < -4)
	{
		return INT32_MIN;
	}

	return (int32_t)(x * (double)(1LL << 29));
}

int32_t	Saturation(int64_t x)
{
	if (x < (int64_t)INT32_MIN)
	{
		return INT32_MIN;
	}
	else if (x > (int64_t)INT32_MAX)
	{
		return INT32_MAX;
	}

	return (int32_t)x;
}

int32_t roundFixed58To29(int64_t x)
{
	return Saturation((x + (1LL << 28)) >> 29);
}

int32_t Add(int32_t x, int32_t y)
{
	return Saturation((int64_t)x + y);
}

int32_t Sub(int32_t x, int32_t y)
{
	return Saturation((int64_t)x - y);
}

int32_t Mul(int32_t x, int32_t y)
{
	if (x == INT32_MIN && y == INT32_MIN)
	{
		return INT32_MAX;
	}

	return roundFixed58To29((int64_t)x * y);
}

int32_t Abs(int32_t x)
{
	if (x < 0)
	{
		return Saturation(-(int64_t)x);
	}

	return x;
}

int32_t Div(int32_t x, int32_t y)
{
	_Bool isNegative = 0;
	int32_t precision = 5;		//be careful with too small values
	int32_t low = 0;
	int32_t high = INT32_MAX;
	int32_t mid;
	int32_t midY;

	if ((x < 0 && y > 0) || (x > 0 && y < 0))
	{
		isNegative = 1;
	}

	x = Abs(x);
	y = Abs(y);

	if (y == 0)
	{
		return INT32_MAX;
	}

	if (x == 0)
	{
		return 0;
	}

	while (1)
	{
		mid = Add(low, (Sub(high, low) >> 1));
		midY = Mul(mid, y);

		if (Abs(Sub(midY, x)) <= precision && midY != INT32_MAX && midY != INT32_MIN)
		{
			if (isNegative)
			{
				return -mid;
			}

			return mid;
		}

		if (midY < x)
		{
			low = mid;
		}
		else
		{
			high = mid;
		}
	}
}

void signalInitialization(Signal *signal)
{
	signal->frequency = SIGNAL_FREQUENCY;
	signal->currAmplitude = 0;
	signal->maxAmplitude = dBtoGain(MAX_AMPLITUDE);
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

void ringInitialization(RingBuff *ringBuff, int16_t *samplesBuff)
{
	int i;

	for (i = 0; i < RING_BUFF_SIZE; i++)
	{
		ringBuff->samples[i] = samplesBuff[i];
	}

	ringBuff->currNum = 0;
}

int16_t generateToneSignal(Signal *signal)
{
	int16_t res = doubleToFixed15(signal->currAmplitude * sin(2 * PI * signal->frequency *
		(double)signal->timeCounter / SAMPLE_RATE));

	signal->currAmplitude += signal->amplitudeStep;
	signal->timeCounter++;

	return res;
}

int16_t signalProc(RingBuff *ringBuff)
{
	int16_t maxSample = INT16_MIN;
	uint16_t i;
	uint16_t index;
	int16_t tmp;
	int32_t gain = doubleToFixed29(1.0);
	int32_t res;

	for (i = 0; i < RING_BUFF_SIZE; i++)
	{
		tmp = Abs(ringBuff->samples[i]);

		if (tmp > maxSample)
		{
			maxSample = tmp;
		}
	}
	
	index = ringBuff->currNum & (RING_BUFF_SIZE - 1);

	if (maxSample < doubleToFixed15(EXPANDER_THRESHOLD))
	{
		gain = 0;
	}
	else if (maxSample > doubleToFixed15(LIMITER_THRESHOLD))
	{
		gain = Div(doubleToFixed29(LIMITER_THRESHOLD), (int32_t)maxSample << 14);
	}
	else
	{
		if (maxSample < doubleToFixed15(COMPRESSOR_THRESHOLD))
		{
			res = Mul((int32_t)maxSample << 14, doubleToFixed29(RATIO));
		}
		else
		{
			res = Div((int32_t)maxSample << 14, doubleToFixed29(RATIO));
		}

		if (res > doubleToFixed29(LIMITER_THRESHOLD))
		{
			res = doubleToFixed29(LIMITER_THRESHOLD);
		}

		gain = Div(res, (int32_t)maxSample << 14);
	}

	ringBuff->currNum = (ringBuff->currNum + 1) & (RING_BUFF_SIZE - 1);

	return (int16_t)(Mul(ringBuff->samples[index], gain));
}

int16_t signalProcDouble(RingBuff *ringBuff)
{
	double maxSample = 0;
	double samples[RING_BUFF_SIZE];
	double sample;

	for (int i = 0; i < RING_BUFF_SIZE; i++)
	{
		samples[i] = (double)(ringBuff->samples[i] / (double)(1LL << 15));

		if (fabs(samples[i]) > maxSample)
		{
			maxSample = fabs(samples[i]);
		}
	}

	int index = ringBuff->currNum & (RING_BUFF_SIZE - 1);
	sample = samples[index];
	double gain;
	double res;

	if (maxSample < EXPANDER_THRESHOLD)
	{
		gain = 0;
	}
	else if (maxSample > LIMITER_THRESHOLD)
	{
		gain = LIMITER_THRESHOLD / maxSample;
	}
	else
	{
		if (maxSample < COMPRESSOR_THRESHOLD)
		{
			res = maxSample * RATIO;
		}
		else
		{
			res = maxSample / RATIO;
		}

		if (res > LIMITER_THRESHOLD)
		{
			res = LIMITER_THRESHOLD;
		}

		gain = res / maxSample;
	}

	sample *= gain;

	ringBuff->currNum = (ringBuff->currNum + 1) & (RING_BUFF_SIZE - 1);

	return (int16_t)(sample * (double)(1LL << 15));
}

void run(Signal *signal, RingBuff *ringBuff, FILE *outputFilePtr)
{
	uint32_t i;
	uint32_t samples;
	uint32_t counter = signal->samplesNum;
	_Bool isFirstIteration = 1;

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

		for (i = 0; i < samples; i++)
		{
			buff[i * CHANNELS] = generateToneSignal(signal);
		}

		if (isFirstIteration)
		{
			ringInitialization(&ringBuff[0], buff);
			ringInitialization(&ringBuff[1], buff);

			isFirstIteration = 0;
		}

		for (i = 0; i < samples; i++)
		{
			ringBuff[0].samples[ringBuff[0].currNum] = buff[i * CHANNELS];
			ringBuff[1].samples[ringBuff[1].currNum] = buff[i * CHANNELS];
			buff[i * CHANNELS] = signalProc(&ringBuff[0]);
			buff[i * CHANNELS + 1] = signalProcDouble(&ringBuff[1]);
		}

		fwrite(buff, BYTES_PER_SAMPLE, samples * CHANNELS, outputFilePtr);
	}
}
