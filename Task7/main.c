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
#define	EXPANDER_HIGH_THR	(dBtoGain(-6))
#define COMPRESSOR_LOW_THR	(dBtoGain(-5))
#define LIMITER_THR			(dBtoGain(-2.0))

// Active flags
#define NOISE_GATE_IS_ACTIVE	1
#define EXPANDER_IS_ACTIVE		1
#define	COMPRESSOR_IS_ACTIVE	1
#define LIMITER_IS_ACTIVE		1

#define EXPANDER_RATIO		0.65
#define COMPRESSOR_RATIO	1.5

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
	int32_t samples[RING_BUFF_SIZE];	// Q31
	int32_t maxSample;					// Q31
	int32_t prevSampleY;				// Q31
	int32_t prevGainY;					// Q27
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

	int32_t noiseThr;				// Q31
	int32_t expanderHighThr;		// Q31
	int32_t compressorLowThr;		// Q31
	int32_t limiterThr;				// Q31

	int32_t FsamplesAlphaAttack;	// Q31
	int32_t FsamplesAlphaRelease;	// Q31
	int32_t FgainAlphaAttack;		// Q31
	int32_t FgainAlphaRelease;		// Q31
	int32_t FfadeAlphaAttack;		// Q31
	int32_t FfadeAlphaRelease;		// Q31
	int32_t FexpC1;					// Q27
	int32_t FcomprC1;				// Q27
	int32_t FexpC2;					// Q27
	int32_t FcomprC2;				// Q27
} Coeffs;

// Q27
const int32_t log2InputsTable[128] = {
	0x4000000,
	0x4081020,
	0x4102040,
	0x4183060,
	0x4204081,
	0x42850a1,
	0x43060c1,
	0x43870e1,
	0x4408102,
	0x4489122,
	0x450a142,
	0x458b162,
	0x460c183,
	0x468d1a3,
	0x470e1c3,
	0x478f1e3,
	0x4810204,
	0x4891224,
	0x4912244,
	0x4993264,
	0x4a14285,
	0x4a952a5,
	0x4b162c5,
	0x4b972e5,
	0x4c18306,
	0x4c99326,
	0x4d1a346,
	0x4d9b366,
	0x4e1c387,
	0x4e9d3a7,
	0x4f1e3c7,
	0x4f9f3e7,
	0x5020408,
	0x50a1428,
	0x5122448,
	0x51a3468,
	0x5224489,
	0x52a54a9,
	0x53264c9,
	0x53a74e9,
	0x542850a,
	0x54a952a,
	0x552a54a,
	0x55ab56a,
	0x562c58b,
	0x56ad5ab,
	0x572e5cb,
	0x57af5eb,
	0x583060c,
	0x58b162c,
	0x593264c,
	0x59b366c,
	0x5a3468d,
	0x5ab56ad,
	0x5b366cd,
	0x5bb76ed,
	0x5c3870e,
	0x5cb972e,
	0x5d3a74e,
	0x5dbb76e,
	0x5e3c78f,
	0x5ebd7af,
	0x5f3e7cf,
	0x5fbf7ef,
	0x6040810,
	0x60c1830,
	0x6142850,
	0x61c3870,
	0x6244891,
	0x62c58b1,
	0x63468d1,
	0x63c78f1,
	0x6448912,
	0x64c9932,
	0x654a952,
	0x65cb972,
	0x664c993,
	0x66cd9b3,
	0x674e9d3,
	0x67cf9f3,
	0x6850a14,
	0x68d1a34,
	0x6952a54,
	0x69d3a74,
	0x6a54a95,
	0x6ad5ab5,
	0x6b56ad5,
	0x6bd7af5,
	0x6c58b16,
	0x6cd9b36,
	0x6d5ab56,
	0x6ddbb76,
	0x6e5cb97,
	0x6eddbb7,
	0x6f5ebd7,
	0x6fdfbf7,
	0x7060c18,
	0x70e1c38,
	0x7162c58,
	0x71e3c78,
	0x7264c99,
	0x72e5cb9,
	0x7366cd9,
	0x73e7cf9,
	0x7468d1a,
	0x74e9d3a,
	0x756ad5a,
	0x75ebd7a,
	0x766cd9b,
	0x76eddbb,
	0x776eddb,
	0x77efdfb,
	0x7870e1c,
	0x78f1e3c,
	0x7972e5c,
	0x79f3e7c,
	0x7a74e9d,
	0x7af5ebd,
	0x7b76edd,
	0x7bf7efd,
	0x7c78f1e,
	0x7cf9f3e,
	0x7d7af5e,
	0x7dfbf7e,
	0x7e7cf9f,
	0x7efdfbf,
	0x7f7efdf,
	0x7ffffff
};
// Q27
const int32_t log2OutputsTable[128] = {
	0xf8000000,
	0xf8172c7b,
	0xf82e2acc,
	0xf844fba8,
	0xf85b9fc3,
	0xf87217c9,
	0xf8886466,
	0xf89e8640,
	0xf8b47dfa,
	0xf8ca4c33,
	0xf8dff187,
	0xf8f56e8d,
	0xf90ac3dc,
	0xf91ff204,
	0xf934f995,
	0xf949db19,
	0xf95e971b,
	0xf9732e1f,
	0xf987a0a9,
	0xf99bef38,
	0xf9b01a4c,
	0xf9c4225e,
	0xf9d807e8,
	0xf9ebcb60,
	0xf9ff6d3a,
	0xfa12ede7,
	0xfa264dd7,
	0xfa398d77,
	0xfa4cad32,
	0xfa5fad71,
	0xfa728e9b,
	0xfa855115,
	0xfa97f543,
	0xfaaa7b85,
	0xfabce43b,
	0xfacf2fc2,
	0xfae15e78,
	0xfaf370b5,
	0xfb0566d3,
	0xfb174128,
	0xfb29000a,
	0xfb3aa3cc,
	0xfb4c2cc2,
	0xfb5d9b3d,
	0xfb6eef8a,
	0xfb8029fa,
	0xfb914ad8,
	0xfba25271,
	0xfbb3410d,
	0xfbc416f7,
	0xfbd4d476,
	0xfbe579cf,
	0xfbf6074a,
	0xfc067d28,
	0xfc16dbaf,
	0xfc27231e,
	0xfc3753b8,
	0xfc476dbb,
	0xfc577168,
	0xfc675efa,
	0xfc7736af,
	0xfc86f8c3,
	0xfc96a570,
	0xfca63cf0,
	0xfcb5bf7d,
	0xfcc52d4d,
	0xfcd48699,
	0xfce3cb96,
	0xfcf2fc7a,
	0xfd02197a,
	0xfd1122c9,
	0xfd20189b,
	0xfd2efb21,
	0xfd3dca8e,
	0xfd4c8712,
	0xfd5b30dd,
	0xfd69c81e,
	0xfd784d05,
	0xfd86bfbe,
	0xfd952078,
	0xfda36f5f,
	0xfdb1ac9f,
	0xfdbfd863,
	0xfdcdf2d6,
	0xfddbfc22,
	0xfde9f471,
	0xfdf7dbeb,
	0xfe05b2b9,
	0xfe137904,
	0xfe212ef1,
	0xfe2ed4a7,
	0xfe3c6a4e,
	0xfe49f00a,
	0xfe576601,
	0xfe64cc57,
	0xfe722330,
	0xfe7f6ab0,
	0xfe8ca2fb,
	0xfe99cc32,
	0xfea6e678,
	0xfeb3f1ef,
	0xfec0eeb9,
	0xfecddcf5,
	0xfedabcc5,
	0xfee78e48,
	0xfef4519f,
	0xff0106e8,
	0xff0dae42,
	0xff1a47cc,
	0xff26d3a4,
	0xff3351e7,
	0xff3fc2b2,
	0xff4c2623,
	0xff587c56,
	0xff64c568,
	0xff710173,
	0xff7d3094,
	0xff8952e6,
	0xff956883,
	0xffa17187,
	0xffad6e0a,
	0xffb95e28,
	0xffc541f9,
	0xffd11998,
	0xffdce51c,
	0xffe8a49e,
	0xfff45837,
	0x0
};
// Q27
const int32_t powOf2InputsTable[255] = {
	0x0,
	0x102040,
	0x204081,
	0x3060c1,
	0x408102,
	0x50a142,
	0x60c183,
	0x70e1c3,
	0x810204,
	0x912244,
	0xa14285,
	0xb162c5,
	0xc18306,
	0xd1a346,
	0xe1c387,
	0xf1e3c7,
	0x1020408,
	0x1122448,
	0x1224489,
	0x13264c9,
	0x142850a,
	0x152a54a,
	0x162c58b,
	0x172e5cb,
	0x183060c,
	0x193264c,
	0x1a3468d,
	0x1b366cd,
	0x1c3870e,
	0x1d3a74e,
	0x1e3c78f,
	0x1f3e7cf,
	0x2040810,
	0x2142850,
	0x2244891,
	0x23468d1,
	0x2448912,
	0x254a952,
	0x264c993,
	0x274e9d3,
	0x2850a14,
	0x2952a54,
	0x2a54a95,
	0x2b56ad5,
	0x2c58b16,
	0x2d5ab56,
	0x2e5cb97,
	0x2f5ebd7,
	0x3060c18,
	0x3162c58,
	0x3264c99,
	0x3366cd9,
	0x3468d1a,
	0x356ad5a,
	0x366cd9b,
	0x376eddb,
	0x3870e1c,
	0x3972e5c,
	0x3a74e9d,
	0x3b76edd,
	0x3c78f1e,
	0x3d7af5e,
	0x3e7cf9f,
	0x3f7efdf,
	0x4081020,
	0x4183060,
	0x42850a1,
	0x43870e1,
	0x4489122,
	0x458b162,
	0x468d1a3,
	0x478f1e3,
	0x4891224,
	0x4993264,
	0x4a952a5,
	0x4b972e5,
	0x4c99326,
	0x4d9b366,
	0x4e9d3a7,
	0x4f9f3e7,
	0x50a1428,
	0x51a3468,
	0x52a54a9,
	0x53a74e9,
	0x54a952a,
	0x55ab56a,
	0x56ad5ab,
	0x57af5eb,
	0x58b162c,
	0x59b366c,
	0x5ab56ad,
	0x5bb76ed,
	0x5cb972e,
	0x5dbb76e,
	0x5ebd7af,
	0x5fbf7ef,
	0x60c1830,
	0x61c3870,
	0x62c58b1,
	0x63c78f1,
	0x64c9932,
	0x65cb972,
	0x66cd9b3,
	0x67cf9f3,
	0x68d1a34,
	0x69d3a74,
	0x6ad5ab5,
	0x6bd7af5,
	0x6cd9b36,
	0x6ddbb76,
	0x6eddbb7,
	0x6fdfbf7,
	0x70e1c38,
	0x71e3c78,
	0x72e5cb9,
	0x73e7cf9,
	0x74e9d3a,
	0x75ebd7a,
	0x76eddbb,
	0x77efdfb,
	0x78f1e3c,
	0x79f3e7c,
	0x7af5ebd,
	0x7bf7efd,
	0x7cf9f3e,
	0x7dfbf7e,
	0x7efdfbf,
	0x7ffffff,
	0xffefdfbf,
	0xffdfbf7f,
	0xffcf9f3e,
	0xffbf7efe,
	0xffaf5ebd,
	0xff9f3e7d,
	0xff8f1e3c,
	0xff7efdfc,
	0xff6eddbb,
	0xff5ebd7b,
	0xff4e9d3a,
	0xff3e7cfa,
	0xff2e5cb9,
	0xff1e3c79,
	0xff0e1c38,
	0xfefdfbf7,
	0xfeeddbb7,
	0xfeddbb76,
	0xfecd9b36,
	0xfebd7af5,
	0xfead5ab5,
	0xfe9d3a74,
	0xfe8d1a34,
	0xfe7cf9f3,
	0xfe6cd9b3,
	0xfe5cb972,
	0xfe4c9932,
	0xfe3c78f1,
	0xfe2c58b1,
	0xfe1c3870,
	0xfe0c1830,
	0xfdfbf7ef,
	0xfdebd7af,
	0xfddbb76e,
	0xfdcb972e,
	0xfdbb76ed,
	0xfdab56ad,
	0xfd9b366c,
	0xfd8b162c,
	0xfd7af5eb,
	0xfd6ad5ab,
	0xfd5ab56a,
	0xfd4a952a,
	0xfd3a74e9,
	0xfd2a54a9,
	0xfd1a3468,
	0xfd0a1428,
	0xfcf9f3e7,
	0xfce9d3a7,
	0xfcd9b366,
	0xfcc99326,
	0xfcb972e5,
	0xfca952a5,
	0xfc993264,
	0xfc891224,
	0xfc78f1e3,
	0xfc68d1a3,
	0xfc58b162,
	0xfc489122,
	0xfc3870e1,
	0xfc2850a1,
	0xfc183060,
	0xfc081020,
	0xfbf7efdf,
	0xfbe7cf9f,
	0xfbd7af5e,
	0xfbc78f1e,
	0xfbb76edd,
	0xfba74e9d,
	0xfb972e5c,
	0xfb870e1c,
	0xfb76eddb,
	0xfb66cd9b,
	0xfb56ad5a,
	0xfb468d1a,
	0xfb366cd9,
	0xfb264c99,
	0xfb162c58,
	0xfb060c18,
	0xfaf5ebd7,
	0xfae5cb97,
	0xfad5ab56,
	0xfac58b16,
	0xfab56ad5,
	0xfaa54a95,
	0xfa952a54,
	0xfa850a14,
	0xfa74e9d3,
	0xfa64c993,
	0xfa54a952,
	0xfa448912,
	0xfa3468d1,
	0xfa244891,
	0xfa142850,
	0xfa040810,
	0xf9f3e7cf,
	0xf9e3c78f,
	0xf9d3a74e,
	0xf9c3870e,
	0xf9b366cd,
	0xf9a3468d,
	0xf993264c,
	0xf983060c,
	0xf972e5cb,
	0xf962c58b,
	0xf952a54a,
	0xf942850a,
	0xf93264c9,
	0xf9224489,
	0xf9122448,
	0xf9020408,
	0xf8f1e3c7,
	0xf8e1c387,
	0xf8d1a346,
	0xf8c18306,
	0xf8b162c5,
	0xf8a14285,
	0xf8912244,
	0xf8810204,
	0xf870e1c3,
	0xf860c183,
	0xf850a142,
	0xf8408102,
	0xf83060c1,
	0xf8204081,
	0xf8102040,
	0xf8000000
};
// Q27
const int32_t powOf2OutputsTable[255] = {
	0x8000000,
	0x80b354f,
	0x8167a52,
	0x821cf1f,
	0x82d33cc,
	0x838a870,
	0x8442d20,
	0x84fc1f4,
	0x85b6701,
	0x8671c5f,
	0x872e224,
	0x87eb868,
	0x88a9f41,
	0x89696c7,
	0x8a29f11,
	0x8aeb836,
	0x8bae24f,
	0x8c71d73,
	0x8d369b9,
	0x8dfc73a,
	0x8ec360f,
	0x8f8b64e,
	0x9054811,
	0x911eb71,
	0x91ea085,
	0x92b6767,
	0x938402f,
	0x9452af7,
	0x95227d8,
	0x95f36ec,
	0x96c584b,
	0x9798c0f,
	0x986d253,
	0x9942b2f,
	0x9a196bf,
	0x9af151c,
	0x9bca661,
	0x9ca4aa8,
	0x9d8020c,
	0x9e5cca7,
	0x9f3aa95,
	0xa019bf0,
	0xa0fa0d4,
	0xa1db95c,
	0xa2be5a4,
	0xa3a25c8,
	0xa4879e2,
	0xa56e20f,
	0xa655e6c,
	0xa73ef14,
	0xa829425,
	0xa914db9,
	0xaa01bf0,
	0xaaefee4,
	0xabdf6b3,
	0xacd037b,
	0xadc2559,
	0xaeb5c6b,
	0xafaa8cd,
	0xb0a0a9e,
	0xb1981fd,
	0xb290f06,
	0xb38b1d9,
	0xb486a95,
	0xb583956,
	0xb681e3e,
	0xb78196a,
	0xb882afa,
	0xb98530e,
	0xba891c4,
	0xbb8e73c,
	0xbc95397,
	0xbd9d6f5,
	0xbea7175,
	0xbfb2338,
	0xc0bec5f,
	0xc1ccd0a,
	0xc2dc55a,
	0xc3ed571,
	0xc4ffd70,
	0xc613d79,
	0xc7295ac,
	0xc84062c,
	0xc958f1b,
	0xca7309b,
	0xcb8eacf,
	0xccabdd9,
	0xcdca9db,
	0xceeaefa,
	0xd00cd58,
	0xd130518,
	0xd25565f,
	0xd37c14f,
	0xd4a460d,
	0xd5ce4bd,
	0xd6f9d83,
	0xd827084,
	0xd955de4,
	0xda865c9,
	0xdbb8858,
	0xdcec5b6,
	0xde21e08,
	0xdf59175,
	0xe092022,
	0xe1cca36,
	0xe308fd6,
	0xe44712a,
	0xe586e58,
	0xe6c8788,
	0xe80bce0,
	0xe950e88,
	0xea97ca8,
	0xebe0767,
	0xed2aeee,
	0xee77365,
	0xefc54f4,
	0xf1153c5,
	0xf267000,
	0xf3ba9cf,
	0xf51015a,
	0xf6676cc,
	0xf7c0a4f,
	0xf91bc0c,
	0xfa78c2e,
	0xfbd7ae0,
	0xfd3884c,
	0xfe9b49d,
	0xfffffff,
	0x7f4da4e,
	0x7e9c426,
	0x7debd70,
	0x7d3c617,
	0x7c8de06,
	0x7be0527,
	0x7b33b66,
	0x7a880ad,
	0x79dd4e7,
	0x7933800,
	0x788a9e2,
	0x77e2a7a,
	0x773b9b2,
	0x7695777,
	0x75f03b3,
	0x754be54,
	0x74a8744,
	0x7405e70,
	0x73643c4,
	0x72c372c,
	0x7223895,
	0x71847eb,
	0x70e651b,
	0x7049011,
	0x6fac8ba,
	0x6f10f04,
	0x6e762db,
	0x6ddc42c,
	0x6d432e4,
	0x6caaef2,
	0x6c13842,
	0x6b7cec1,
	0x6ae725e,
	0x6a52306,
	0x69be0a7,
	0x692ab2f,
	0x689828c,
	0x68066ac,
	0x677577d,
	0x66e54ed,
	0x6655eec,
	0x65c7567,
	0x653984d,
	0x64ac78d,
	0x6420316,
	0x6394ad6,
	0x6309ebc,
	0x627feb8,
	0x61f6ab8,
	0x616e2ad,
	0x60e6685,
	0x605f62f,
	0x5fd919c,
	0x5f538ba,
	0x5eceb7a,
	0x5e4a9cb,
	0x5dc739e,
	0x5d448e2,
	0x5cc2987,
	0x5c4157d,
	0x5bc0cb5,
	0x5b40f1f,
	0x5ac1cab,
	0x5a4354a,
	0x59c58ec,
	0x5948783,
	0x58cc0fe,
	0x585054f,
	0x57d5466,
	0x575ae35,
	0x56e12ac,
	0x56681bd,
	0x55efb59,
	0x5577f72,
	0x5500df8,
	0x548a6dc,
	0x5414a12,
	0x539f78a,
	0x532af36,
	0x52b7107,
	0x5243cf1,
	0x51d12e4,
	0x515f2d2,
	0x50edcae,
	0x507d06a,
	0x500cdf8,
	0x4f9d54a,
	0x4f2e653,
	0x4ec0106,
	0x4e52554,
	0x4de5330,
	0x4d78a8e,
	0x4d0cb5f,
	0x4ca1597,
	0x4c36929,
	0x4bcc607,
	0x4b62c25,
	0x4af9b76,
	0x4a913ec,
	0x4a2957b,
	0x49c2017,
	0x495b3b3,
	0x48f5042,
	0x488f5b8,
	0x482a408,
	0x47c5b27,
	0x4761b07,
	0x46fe39d,
	0x469b4dc,
	0x4638eb9,
	0x45d7127,
	0x4575c1b,
	0x4514f88,
	0x44b4b63,
	0x4454fa0,
	0x43f5c34,
	0x4397112,
	0x4338e2f,
	0x42db380,
	0x427e0fa,
	0x4221690,
	0x41c5438,
	0x41699e6,
	0x410e78f,
	0x40b3d29,
	0x4059aa7,
	0x4000000
};

double dBtoGain(double dB);
int16_t doubleToFixed15(double x);
int32_t doubleToFixed29(double x);
static inline int32_t doubleToFixed31(double x);
double fixed32ToDouble(int32_t x);
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

int32_t fixedLog2(int32_t x);
int32_t fixedPowOf2(int32_t x);
int32_t LeftShift(int32_t x, int8_t shift);
int32_t RightShift(int32_t x, int8_t shift);

double NRDivDouble(double x, double y);
int32_t NRDiv(int32_t x, int32_t y);

int main()
{
	//double step = fixed32ToDouble(0x100000) * 16;
	//int32_t fixedStep = doubleToFixed31(step / 16);
	//double inputs[260];
	//double outputs[260];
	//double curr = -1.0;
	//
	//printf("double step = %f\n", step);
	//printf("fixed step = %d\n", fixedStep);
	////printf("1. 0x%x\n", doubleToFixed31(pow(2.0, 0.36851) / 16));
	////printf("2. 0x%x\n", doubleToFixed31(pow(2.0, -0.6846) / 16));
	////printf("%f -> %f -> 0x%x\n\n", pow(2.0, fixed32ToDouble(0x7fffffff)), pow(2.0, fixed32ToDouble(0x7fffffff)) / 16, doubleToFixed31(pow(2.0, fixed32ToDouble(0x7fffffff)) / 16));
	//
	////printf("inputs = \n");
	//
	//for (int i = 0; i < 257; i++)
	//{
	//	inputs[i] = curr;
	//	outputs[i] = pow(2.0, curr);// / 16;
	//	curr += step;
	//
	//	//printf("%d. 0x%x,\n", i, RightShift(doubleToFixed31(inputs[i]), 4));
	//}
	//
	////curr = 0 - step;
	////
	////for (int i = 129; i < 257; i++)
	////{
	////	inputs[i] = curr;
	////	outputs[i] = pow(2.0, curr);// / 16;
	////	curr -= step;
	////
	////	//printf("0x%x,\n", RightShift(doubleToFixed31(inputs[i]), 4));
	////}
	//
	////printf("\noutputs = \n");
	//
	//for (int i = 0; i < 257; i++)
	//{
	//	printf("%d. %f -> %f,\n", i, inputs[i], outputs[i]);//RightShift(doubleToFixed31(outputs[i]), 4));
	//}
	//
	//system("pause");

	//double inX = -1.0;
	//double inY = 2.0;
	//double inputX = inX / 16;
	//double inputY = inY / 16;
	//int32_t x = doubleToFixed31(inputX);
	//int32_t y = doubleToFixed31(inputY);
	//double doublePow = pow(inX, inY);
	////int32_t xFixedLog = doubleToFixed31(xLog);
	//double res = fixed32ToDouble(fixedPow(x, y)) * 16;

	//Signal signal;
	//RingBuff ringBuff[2];
	//signalInitialization(&signal);
	//
	//WavHeader header;
	//fileHeaderInitialization(&header, &signal);
	//FILE *outputFilePtr = openFile(OUTPUT_FILE_NAME, 1);
	//writeHeader(&header, outputFilePtr);
	//
	//Coeffs coeffs;
	//calcCoeffs(&coeffs);
	//
	//run(&signal, &coeffs, ringBuff, outputFilePtr);
	//fclose(outputFilePtr);

	//double c1d = 140.0 / 33.0;
	//double c2d = -64.0 / 11.0;
	//double c3d = 256.0 / 99.0;
	//
	//int32_t c1 = doubleToFixed31(c1d / 8);
	//int32_t c2 = doubleToFixed31(c2d / 8);
	//int32_t c3 = doubleToFixed31(c3d / 8);

	double ud1;
	double ud2;
	//ud = NRDivDouble(0.324, 0.73);
	//u = fixed32ToDouble(NRDiv(doubleToFixed31(0.324), doubleToFixed31(0.73)));
	//ud = NRDivDouble(-0.59, 0.73);
	//u = fixed32ToDouble(NRDiv(doubleToFixed31 (-0.59), doubleToFixed31(0.73)));
	//ud = NRDivDouble(0.324, -0.329);
	//u = fixed32ToDouble(NRDiv(doubleToFixed31(0.324), doubleToFixed31(-0.329)));
	//ud = NRDivDouble(0.324, 0.324);
	//u = fixed32ToDouble(NRDiv(doubleToFixed31(0.324), doubleToFixed31(0.324)));
	//ud = NRDivDouble(0, 0.73);
	//u = fixed32ToDouble(NRDiv(doubleToFixed31(0), doubleToFixed31(0.73)));
	//ud = NRDivDouble(0.324, 0);
	//u = fixed32ToDouble(NRDiv(doubleToFixed31(0.324), doubleToFixed31(0)));

	ud1 = pow(2, 0.07863);
	ud2 = pow(2, -0.07863);

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

int32_t roundFixed63To31(int64_t x)
{
	return (int32_t)((x + (1LL << 30)) >> 31);
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

	return roundFixed63To31((int64_t)x * y);
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

double NRDivDouble(double x, double y)
{
	// coeffs
	double c1 = 140.0 / 33.0;
	double c2 = -64.0 / 11.0;
	double c3 = 256.0 / 99.0;

	double r;
	double e;
	double k;
	double res;

	int8_t i;
	int8_t resIsNegative = 0;

	// sign calculation
	if ((x < 0 && y >= 0) || (x >= 0 && y < 0))
	{
		resIsNegative = 1;
	}

	// get absolute values of x and y
	x = fabs(x);
	y = fabs(y);

	// special cases handling
	if (x == 0)
	{
		return 0;
	}

	if (y == 0)
	{
		return 1;
	}

	if (x > y)
	{
		x = y;
	}

	// normalization to [0.5, 1.0]
	while (y < 0.5)
	{
		x *= 2;
		y *= 2;
	}

	while (y > 1.0)
	{
		x /= 2;
		y /= 2;
	}

	// precalculation
	r = c1 + y * (c2 + y * c3);

	// loop calculation
	for (i = 0; i < 2; i++)
	{
		e = 1.0 - y * r;
		k = r * e;
		r = r + k + k * e;
	}

	// final multiplication
	res = x * r;

	// if result must be negative
	if (resIsNegative)
	{
		res = 0 - res;
	}

	return res;
}

int32_t NRDiv(int32_t x, int32_t y)
{
	// coeffs
	int32_t c1 = 0x43e0f83e;		// Q28
	int32_t c2 = 0xa2e8ba2f;		// Q28
	int32_t c3 = 0x295fad40;		// Q28

	int32_t r;						// Q28
	int32_t e;						// Q28
	int32_t k;						// Q28
	int32_t res;

	int8_t i;
	int8_t resIsNegative = 0;

	// sign calculation
	if ((x ^ y) < 0)
	{
		resIsNegative = 1;
	}

	// get absolute values of x and y
	x = abs(x);
	y = abs(y);

	// special cases handling
	if (x == 0)
	{
		return 0;
	}

	if (y == 0)
	{
		return INT32_MAX;
	}

	if (x > y)
	{
		x = y;
	}

	// normalization to [0.5, 1.0]
	while (y < 0x40000000)
	{
		x <<= 1;
		y <<= 1;
	}

	// precalculation
	r = Add(c1, Mul(y, Add(c2, Mul(y, c3))));		// Q28

	// loop calculation
	for (i = 0; i < 2; i++)
	{
		e = Sub(0x0fffffff, Mul(y, r));
		k = LeftShift(Mul(r, e), 3);
		r = Add(r, Add(k, LeftShift(Mul(k, e), 3)));
	}

	// final multiplication
	res = LeftShift(Mul(x, r), 3);

	// if result must be negative
	if (resIsNegative)
	{
		res = Sub(0x0, res);
	}

	return res;
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
	ringBuff[0].currNum = 0;
	ringBuff[0].isFade = 0;
	ringBuff[0].prevSampleY = 0;
	ringBuff[0].prevGainY = 0;

	ringBuff[1].currNum = 0;
	ringBuff[1].isFade = 0;
	ringBuff[1].prevSampleY = 0;
	ringBuff[1].prevGainY = 0;

	for (i = 0; i < RING_BUFF_SIZE; i++)
	{
		ringBuff[0].samples[i] = samplesBuff[i * CHANNELS];
		ringBuff[1].samples[i] = samplesBuff[i * CHANNELS + 1];
		samplesBuff[i * CHANNELS] = 0;
		samplesBuff[i * CHANNELS + 1] = 0;
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
	coeffs->samplesAlphaAttack	 = (double)1 - exp((double)-1 / (SAMPLE_RATE * SAMPLES_ATTACK_TIME));
	coeffs->samplesAlphaRelease  = (double)1 - exp((double)-1 / (SAMPLE_RATE * SAMPLES_RELEASE_TIME));
	coeffs->gainAlphaAttack		 = (double)1 - exp((double)-1 / (SAMPLE_RATE * GAIN_ATTACK_TIME));
	coeffs->gainAlphaRelease	 = (double)1 - exp((double)-1 / (SAMPLE_RATE * GAIN_RELEASE_TIME));
	coeffs->fadeAlphaAttack		 = (double)1 - exp((double)-1 / (SAMPLE_RATE * FADE_ATTACK_TIME));
	coeffs->fadeAlphaRelease	 = (double)1 - exp((double)-1 / (SAMPLE_RATE * FADE_RELEASE_TIME));
	coeffs->expC1				 = ((double)1 / EXPANDER_RATIO) - 1;
	coeffs->comprC1				 = (double)1 - ((double)1 / COMPRESSOR_RATIO);
	coeffs->expC2				 = pow(EXPANDER_HIGH_THR, coeffs->expC1);
	coeffs->comprC2				 = pow(COMPRESSOR_LOW_THR, coeffs->comprC1);

	coeffs->noiseThr			 = doubleToFixed31(NOISE_THR);
	coeffs->expanderHighThr		 = doubleToFixed31(EXPANDER_HIGH_THR);
	coeffs->compressorLowThr	 = doubleToFixed31(COMPRESSOR_LOW_THR);
	coeffs->limiterThr			 = doubleToFixed31(LIMITER_THR);

	coeffs->FsamplesAlphaAttack	 = doubleToFixed31(coeffs->samplesAlphaAttack);
	coeffs->FsamplesAlphaRelease = doubleToFixed31(coeffs->samplesAlphaRelease);
	coeffs->FgainAlphaAttack	 = doubleToFixed31(coeffs->gainAlphaAttack);
	coeffs->FgainAlphaRelease	 = doubleToFixed31(coeffs->gainAlphaRelease);
	coeffs->FfadeAlphaAttack	 = doubleToFixed31(coeffs->fadeAlphaAttack);
	coeffs->FfadeAlphaRelease	 = doubleToFixed31(coeffs->fadeAlphaRelease);
	coeffs->FexpC1				 = doubleToFixed31(coeffs->expC1 / 16);
	coeffs->FcomprC1			 = doubleToFixed31(coeffs->comprC1 / 16);
	coeffs->FexpC2				 = doubleToFixed31(coeffs->expC2 / 16);
	coeffs->FcomprC2			 = doubleToFixed31(coeffs->comprC2 / 16);
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

int32_t LeftShift(int32_t x, int8_t shift)
{
	return Saturation((int64_t)x << shift);
}

int32_t RightShift(int32_t x, int8_t shift)
{
	if (shift > 31)
	{
		return 0;
	}

	return x >> shift;
}

int32_t fixedLog2(int32_t x)
{
	// Input/Output in Q27

	uint8_t i = 0;
	int32_t res = 0;
	int32_t remainder = 0;

	if (x == 0)
	{
		return INT32_MIN;
	}

	x = Abs(x);

	while (x < 0x4000000)
	{
		x = LeftShift(x, 1);
		res = Sub(res, 0x8000000);
	}

	while (x > 0x7ffffff)
	{
		x = RightShift(x, 1);
		res = Add(res, 0x8000000);
	}

	while (Abs(Sub(x, log2InputsTable[i])) > 0x40810)
	{
		i++;
	}

	return res + log2OutputsTable[i];
}

int32_t fixedPowOf2(int32_t x)
{
	// Input/Output in Q27

	int32_t res = 0x08000000;
	int32_t index = 0;

	while (x < (int32_t)0xf8000000)
	{
		x = Add(x, 0x8000000);
		res = RightShift(res, 1);
	}

	while (x > (int32_t)0x7ffffff)
	{
		x = Sub(x, 0x8000000);
		res = LeftShift(res, 1);
	}

	index = x / 0x102040;

	if (index < 0)
	{
		index = abs(index) + 127;
	}

	return LeftShift(Mul(res, powOf2OutputsTable[index]), 4);
}

int32_t fixedPow(int32_t x, int32_t y)
{
	// Input/Output in Q27

	return fixedPowOf2(LeftShift(Mul(y, fixedLog2(x)), 4));
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
				alpha = coeffs->gainAlphaAttack;
			}
			else
			{
				alpha = coeffs->gainAlphaRelease;
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
	else if (sampleN < EXPANDER_HIGH_THR && (sampleN >= NOISE_THR || !NOISE_GATE_IS_ACTIVE) && EXPANDER_IS_ACTIVE)
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

int32_t signalProc1(const Coeffs *coeffs, RingBuff *ringBuff)
{
	int32_t gain = INT32_MAX;										// Q27
	int32_t limiterGain = 0;										// Q27
	int32_t compressorGain = 0;										// Q27
	int32_t expanderGain = 0;										// Q27
	int32_t alpha;													// Q31
	int32_t sample = ringBuff->samples[ringBuff->currNum];			// Q31
	int32_t sampleN;												// Q31
	int32_t gainN = 0x08000000;										// Q27
	int32_t res;													// Q31
	int32_t limCheck;
	_Bool isNoiseGate = 0;
	_Bool isExpander = 0;
	_Bool isCompressor = 0;
	_Bool isLimiter = 0;

	if (Abs(ringBuff->prevSampleY) <= Abs(sample))
	{
		alpha = coeffs->FsamplesAlphaAttack;
	}
	else
	{
		alpha = coeffs->FsamplesAlphaRelease;
	}

	sampleN = Add(Mul(Abs(sample), alpha), Mul(Sub(0x7fffffff, alpha), ringBuff->prevSampleY));

	if (NOISE_GATE_IS_ACTIVE && sampleN < coeffs->noiseThr)
	{
		gain = 0;
		isNoiseGate = 1;
		ringBuff->isFade = 1;
	}

	if (COMPRESSOR_IS_ACTIVE && sampleN > coeffs->compressorLowThr)
	{
		gainN = LeftShift(Mul(coeffs->FcomprC2, fixedPow(RightShift(sampleN, 4), -(coeffs->FcomprC1))), 4);

		if (ringBuff->prevGainY >= gainN)
		{
			alpha = coeffs->FgainAlphaAttack;
		}
		else
		{
			alpha = coeffs->FgainAlphaRelease;
		}

		compressorGain = Add(Mul(gainN, alpha), Mul(Sub(0x7fffffff, alpha), ringBuff->prevGainY));

		if (compressorGain < gain)
		{
			gain = compressorGain;
			ringBuff->isFade = 1;
			isCompressor = 1;
		}
	}

	if (EXPANDER_IS_ACTIVE && sampleN < coeffs->expanderHighThr)
	{
		gainN = LeftShift(Mul(coeffs->FexpC2, fixedPow(RightShift(sampleN, 4), -(coeffs->FexpC1))), 4);

		if (ringBuff->prevGainY >= gainN)
		{
			alpha = coeffs->FgainAlphaAttack;
		}
		else
		{
			alpha = coeffs->FgainAlphaRelease;
		}

		expanderGain = Add(Mul(gainN, alpha), Mul(Sub(0x7fffffff, alpha), ringBuff->prevGainY));

		if (expanderGain < gain)
		{
			gain = expanderGain;
			ringBuff->isFade = 1;
			isExpander = 1;
		}
	}

	if (!isCompressor && !isExpander && !isNoiseGate)
	{
		gain = 0x08000000;
	}

	if (ringBuff->prevGainY - gain == 0)
	{
		ringBuff->isFade = 0;
	}

	//if (!isCompressor && !isExpander && !isLimiter && ringBuff->isFade)
	//{
	//	if (ringBuff->prevGainY <= gain)
	//	{
	//		alpha = coeffs->FfadeAlphaAttack;
	//	}
	//	else
	//	{
	//		alpha = coeffs->FfadeAlphaRelease;
	//	}

	//	gain = Add(Mul(gain, alpha), Mul(Sub(0x7fffffff, alpha), ringBuff->prevGainY));
	//}

	updateMaxRingBuffValue(ringBuff);
	limCheck = LeftShift(Mul(ringBuff->maxSample, gain), 4);

	if (LIMITER_IS_ACTIVE && limCheck > coeffs->limiterThr)
	{
		gain = Mul(gain, Div(coeffs->limiterThr, limCheck));
	}

	res = LeftShift(Mul(gain, sample), 4);

	ringBuff->prevSampleY = sampleN;
	ringBuff->prevGainY = gain;

	return res;
}

void run(Signal *signal, const Coeffs *coeffs, RingBuff *ringBuff, FILE *outputFilePtr)
{
	int32_t dataBuff[DATA_BUFF_SIZE * CHANNELS];
	int32_t res;
	int32_t res2;
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
			ringInitialization(ringBuff, dataBuff);
			i = RING_BUFF_SIZE;
			isFirstIteration = 0;
		}
		else
		{
			i = 0;
		}

		for (i; i < samples; i++)
		{
			res = signalProcDouble1(coeffs, &ringBuff[0], &prevSampleY, &prevGainY);
			res2 = signalProc1(coeffs, &ringBuff[1]);
			//dataBuff[i * CHANNELS + 1] = signalProcDouble(&ringBuff[1]);

			ringBuff[0].samples[ringBuff[0].currNum] = dataBuff[i * CHANNELS];
			ringBuff[1].samples[ringBuff[1].currNum] = dataBuff[i * CHANNELS + 1];

			dataBuff[i * CHANNELS] = res;
			dataBuff[i * CHANNELS + 1] = res2;

			ringBuff[0].currNum = (ringBuff[0].currNum + 1) & (RING_BUFF_SIZE - 1);
			ringBuff[1].currNum = (ringBuff[1].currNum + 1) & (RING_BUFF_SIZE - 1);
		}

		fwrite(dataBuff, BYTES_PER_SAMPLE, samples * CHANNELS, outputFilePtr);
	}
}
