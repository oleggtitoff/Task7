#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define OUTPUT_FILE_NAME "Output.wav"
#define FILE_HEADER_SIZE 44
#define BYTES_PER_SAMPLE 4
#define BITS_PER_SAMPLE ((BYTES_PER_SAMPLE) * 8)
#define SAMPLE_RATE 48000
#define CHANNELS 2

#define SIGNAL_FREQUENCY	1000
#define MAX_AMPLITUDE		0		//in dB
#define SIGNAL_TIME			1.0	//in seconds

#define NOISE_THR			(dBtoGain(-20))
#define	EXPANDER_HIGH_THR	(dBtoGain(-7))
#define COMPRESSOR_LOW_THR	(dBtoGain(-4))
#define LIMITER_THR			(dBtoGain(-2.5))

// Active flags
#define NOISE_GATE_IS_ACTIVE	1
#define EXPANDER_IS_ACTIVE		1
#define	COMPRESSOR_IS_ACTIVE	1
#define LIMITER_IS_ACTIVE		1

#define COMPRESSOR_RATIO	2.0
#define EXPANDER_RATIO		2.0

#define RING_BUFF_SIZE 128
#define DATA_BUFF_SIZE 1000		//must be bigger than RING_BUFF_SIZE

#define PI 3.14159265358979323846

#define SAMPLES_ATTACK_TIME		0.0000001
#define SAMPLES_RELEASE_TIME	0.03
#define GAIN_ATTACK_TIME		0.001
#define GAIN_RELEASE_TIME		0.001
#define FADE_ATTACK_TIME		0.001
#define FADE_RELEASE_TIME		0.001


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
	_Bool currDirection;	// 1 is Up, 0 - down
	double maxAmplitude;
	double amplitudeStep;
	double signalTime;
	uint32_t timeCounter;
	uint32_t samplesNum;
} Signal;

typedef struct {
	uint16_t currNum;
	int32_t samples[RING_BUFF_SIZE];
	int32_t maxSample;
	_Bool isFade;
} RingBuff;

typedef struct {
	double samplesAlphaAttack;
	double samplesAlphaRelease;
	double gainAlphaAttack;
	double gainAlphaRelease;
	double fadeAlphaAttack;
	double fadeAlphaRelease;
	double expC1;
	double comprC1;
	double expC2;
	double comprC2;
} Coeffs;


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

void ringInitialization(RingBuff *ringBuff, int32_t *samplesBuff);
int32_t generateToneSignal(Signal *signal);
//int16_t signalProc(RingBuff *ringBuff);
int32_t signalProcDouble(RingBuff *ringBuff);
void calcCoeffs(Coeffs *coeffs);
int32_t signalProcDouble1(const Coeffs *coeffs, RingBuff *ringBuff, double *prevSampleY, double *prevGainY);
void run(Signal *signal, const Coeffs *coeffs, RingBuff *ringBuff, FILE *outputFilePtr);


int main()
{
	Signal signal;
	RingBuff ringBuff[2];
	signalInitialization(&signal);

	WavHeader header;
	fileHeaderInitialization(&header, &signal);
	FILE *outputFilePtr = openFile(OUTPUT_FILE_NAME, 1);
	writeHeader(&header, outputFilePtr);

	Coeffs coeffs;
	calcCoeffs(&coeffs);

	run(&signal, &coeffs, ringBuff, outputFilePtr);
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

static inline int32_t doubleToFixed31(double x)
{
	if (x >= 1)
	{
		return INT32_MAX;
	}
	else if (x < -1)
	{
		return INT32_MIN;
	}

	return (int32_t)(x * (double)(1LL << 31));
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

double fixed32ToDouble(int32_t x)
{
	return (double)(x / (double)(1LL << 31));
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
	signal->currAmplitude = dBtoGain(MAX_AMPLITUDE);
	signal->currDirection = 0;
	signal->maxAmplitude = dBtoGain(MAX_AMPLITUDE);
	signal->signalTime = SIGNAL_TIME;
	signal->timeCounter = 0;
	signal->samplesNum = (uint32_t)round(signal->signalTime * SAMPLE_RATE);
	signal->amplitudeStep = signal->maxAmplitude / signal->samplesNum * 2;
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
	header->bitsPerSample = BITS_PER_SAMPLE;
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

void ringInitialization(RingBuff *ringBuff, int32_t *samplesBuff)
{
	int i;
	ringBuff->currNum = 0;
	ringBuff->isFade = 0;

	for (i = 0; i < RING_BUFF_SIZE; i++)
	{
		ringBuff->samples[i] = samplesBuff[i * CHANNELS];
		samplesBuff[i * CHANNELS] = 0;
	}
}

int32_t generateToneSignal(Signal *signal)
{
	int32_t res = doubleToFixed31(signal->currAmplitude * sin(2 * PI * signal->frequency *
		(double)signal->timeCounter / SAMPLE_RATE));

	if (signal->currDirection)
	{
		signal->currAmplitude += signal->amplitudeStep;
	}
	else
	{
		signal->currAmplitude -= signal->amplitudeStep;
	}

	if (fabs(signal->currAmplitude - signal->maxAmplitude) < 0.00000000000000001)
	{
		signal->currDirection = 0;
	}

	if (fabs(signal->currAmplitude) < 0.00000000000000001)
	{
		signal->currDirection = 1;
	}

	signal->timeCounter++;

	return res;
}

//int16_t signalProc(RingBuff *ringBuff)
//{
//	int16_t maxSample = INT16_MIN;
//	uint16_t i;
//	uint16_t index;
//	int16_t tmp;
//	int32_t gain = doubleToFixed29(1.0);
//	int32_t res;
//
//	for (i = 0; i < RING_BUFF_SIZE; i++)
//	{
//		tmp = Abs(ringBuff->samples[i]);
//
//		if (tmp > maxSample)
//		{
//			maxSample = tmp;
//		}
//	}
//	
//	index = ringBuff->currNum & (RING_BUFF_SIZE - 1);
//
//	if (maxSample < doubleToFixed15(EXPANDER_THRESHOLD))
//	{
//		gain = 0;
//	}
//	else if (maxSample > doubleToFixed15(LIMITER_THR))
//	{
//		gain = Div(doubleToFixed29(LIMITER_THR), (int32_t)maxSample << 14);
//	}
//	else
//	{
//		if (maxSample < doubleToFixed15(COMPRESSOR_THRESHOLD))
//		{
//			res = Mul((int32_t)maxSample << 14, doubleToFixed29(RATIO));
//		}
//		else
//		{
//			res = Div((int32_t)maxSample << 14, doubleToFixed29(RATIO));
//		}
//
//		if (res > doubleToFixed29(LIMITER_THR))
//		{
//			res = doubleToFixed29(LIMITER_THR);
//		}
//
//		gain = Div(res, (int32_t)maxSample << 14);
//	}
//
//	ringBuff->currNum = (ringBuff->currNum + 1) & (RING_BUFF_SIZE - 1);
//
//	return (int16_t)(Mul(ringBuff->samples[index], gain));
//}

int32_t signalProcDouble(RingBuff *ringBuff)
{
	double maxSample = 0;
	double samples[RING_BUFF_SIZE];
	double sample;

	for (int i = 0; i < RING_BUFF_SIZE; i++)
	{
		samples[i] = (double)(ringBuff->samples[i] / (double)(1LL << 31));

		if (fabs(samples[i]) > maxSample)
		{
			maxSample = fabs(samples[i]);
		}
	}

	int index = ringBuff->currNum & (RING_BUFF_SIZE - 1);
	sample = samples[index];
	double gain = 1.0;

	if (maxSample < NOISE_THR)
	{
		gain = 0;
	}
	else if (maxSample > LIMITER_THR)
	{
		gain = LIMITER_THR / maxSample;
	}
	else
	{
		if (maxSample < EXPANDER_HIGH_THR)
		{
			//res = (EXPANDER_HIGH_THR - ((EXPANDER_HIGH_THR - maxSample) / EXPANDER_RATIO));
			//gain = res / maxSample;
			gain = ((EXPANDER_HIGH_THR - (EXPANDER_HIGH_THR / EXPANDER_RATIO)) / maxSample) + (1 / EXPANDER_RATIO);
		}
		else if (maxSample > COMPRESSOR_LOW_THR)
		{
			//res = (COMPRESSOR_LOW_THR + ((maxSample - COMPRESSOR_LOW_THR) / COMPRESSOR_RATIO));
			//gain = res / maxSample;
			gain = ((COMPRESSOR_LOW_THR - (COMPRESSOR_LOW_THR / COMPRESSOR_RATIO)) / maxSample) + (1 / COMPRESSOR_RATIO);
		}
	}

	sample *= gain;

	ringBuff->currNum = (ringBuff->currNum + 1) & (RING_BUFF_SIZE - 1);

	return (int32_t)(sample * (double)(1LL << 31));
}

void calcCoeffs(Coeffs *coeffs)
{
	coeffs->samplesAlphaAttack = (double)1 - exp((double)-1 / (SAMPLE_RATE * SAMPLES_ATTACK_TIME));
	coeffs->samplesAlphaRelease = (double)1 - exp((double)-1 / (SAMPLE_RATE * SAMPLES_RELEASE_TIME));
	coeffs->gainAlphaAttack = (double)1 - exp((double)-1 / (SAMPLE_RATE * GAIN_ATTACK_TIME));
	coeffs->gainAlphaRelease = (double)1 - exp((double)-1 / (SAMPLE_RATE * GAIN_RELEASE_TIME));
	coeffs->fadeAlphaAttack = (double)1 - exp((double)-1 / (SAMPLE_RATE * FADE_ATTACK_TIME));
	coeffs->fadeAlphaRelease = (double)1 - exp((double)-1 / (SAMPLE_RATE * FADE_RELEASE_TIME));
	coeffs->expC1 = ((double)1 / EXPANDER_RATIO) - 1;
	coeffs->comprC1 = (double)1 - ((double)1 / COMPRESSOR_RATIO);
	coeffs->expC2 = pow(EXPANDER_HIGH_THR, coeffs->expC1);
	coeffs->comprC2 = pow(COMPRESSOR_LOW_THR, coeffs->comprC1);
}

void updateMaxRingBuffValue(RingBuff *ringBuff)
{
	uint16_t i;
	ringBuff->maxSample = ringBuff->samples[0];

	for (i = 0; i < RING_BUFF_SIZE; i++)
	{
		if (abs(ringBuff->samples[i]) > ringBuff->maxSample)
		{
			ringBuff->maxSample = abs(ringBuff->samples[i]);
		}
	}
}

int32_t signalProcDouble1(const Coeffs *coeffs, RingBuff *ringBuff, double *prevSampleY, double *prevGainY)
{
	double gain = 1.0;
	double limGain = 0;
	double alpha = 0;
	double sampleN = 0;
	double gainN = 0;
	double res = 0;
	double sampleD = fixed32ToDouble(ringBuff->samples[ringBuff->currNum]);

	if (fabs((*prevGainY) - gain) <= 0.00000001)
	{
		ringBuff->isFade = 0;
	}
	
	if (LIMITER_IS_ACTIVE)
	{
		updateMaxRingBuffValue(ringBuff);
	}

	if (fabs((*prevSampleY)) <= fabs(sampleD))
	{
		alpha = coeffs->samplesAlphaAttack;
	}
	else
	{
		alpha = coeffs->samplesAlphaRelease;
	}

	sampleN = fabs(sampleD) * alpha + ((double)1 - alpha) * (*prevSampleY);
	
	if (sampleN < NOISE_THR && NOISE_GATE_IS_ACTIVE)
	{
		gain = 0;
		ringBuff->isFade = 1;
	}

	if ((sampleN > COMPRESSOR_LOW_THR && COMPRESSOR_IS_ACTIVE) || (fixed32ToDouble(ringBuff->maxSample) > LIMITER_THR && LIMITER_IS_ACTIVE))
	{
		if (COMPRESSOR_IS_ACTIVE)
		{
			gainN = coeffs->comprC2 * pow(sampleN, -(coeffs->comprC1));

			if ((*prevGainY) >= gainN)
			{
				alpha = coeffs->fadeAlphaAttack;
			}
			else
			{
				alpha = coeffs->fadeAlphaRelease;
			}

			gain = gainN * alpha + ((double)1 - alpha) * (*prevGainY);
		}

		if (LIMITER_IS_ACTIVE)
		{
			limGain = LIMITER_THR / fixed32ToDouble(ringBuff->maxSample);
			ringBuff->isFade = 1;

			if (limGain < gain)
			{
				gain = limGain;
				ringBuff->isFade = 0;
			}
		}
	}
	else if (sampleN < EXPANDER_HIGH_THR && (sampleN >= NOISE_THR || !NOISE_GATE_IS_ACTIVE)  && EXPANDER_IS_ACTIVE)
	{
		gainN = coeffs->expC2 * pow(sampleN, -(coeffs->expC1));

		if ((*prevGainY) >= gainN)
		{
			alpha = coeffs->gainAlphaAttack;
		}
		else
		{
			alpha = coeffs->gainAlphaRelease;
		}

		gain = gainN * alpha + ((double)1 - alpha) * (*prevGainY);
		
		ringBuff->isFade = 1;
	}
	else if (ringBuff->isFade)
	{
		if (fabs((*prevGainY) - gain) > 0.00000001)
		{
			if ((*prevGainY) < gain)
			{
				alpha = coeffs->fadeAlphaAttack;
			}
			else
			{
				alpha = coeffs->fadeAlphaRelease;
			}

			gain = gain * alpha + ((double)1 - alpha) * (*prevGainY);
		}
	}

	res = gain * sampleD;

	*prevSampleY = sampleN;
	*prevGainY = gain;

	return (int32_t)(res * (double)(1LL << 31));
}

void run(Signal *signal, const Coeffs *coeffs, RingBuff *ringBuff, FILE *outputFilePtr)
{
	int32_t dataBuff[DATA_BUFF_SIZE * CHANNELS];
	int32_t res;
	uint32_t i;
	uint32_t samples;
	uint32_t counter = signal->samplesNum;
	double prevSampleY = 0;
	double prevGainY = 0;
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
			dataBuff[i * CHANNELS] = generateToneSignal(signal);
			dataBuff[i * CHANNELS + 1] = dataBuff[i * CHANNELS];
		}

		if (isFirstIteration)
		{
			ringInitialization(&ringBuff[0], dataBuff);
			ringInitialization(&ringBuff[1], dataBuff);
			i = RING_BUFF_SIZE;
			isFirstIteration = 0;
		}
		else
		{
			i = 0;
		}

		for (i; i < samples; i++)
		{
			res =  signalProcDouble1(coeffs, ringBuff, &prevSampleY, &prevGainY);
			//buff[i * CHANNELS + 1] = signalProcDouble(&ringBuff[1]);

			ringBuff[0].samples[ringBuff[0].currNum] = dataBuff[i * CHANNELS];
			ringBuff[1].samples[ringBuff[1].currNum] = dataBuff[i * CHANNELS + 1];

			dataBuff[i * CHANNELS] = res;

			ringBuff->currNum = (ringBuff->currNum + 1) & (RING_BUFF_SIZE - 1);
		}

		fwrite(dataBuff, BYTES_PER_SAMPLE, samples * CHANNELS, outputFilePtr);
	}
}
