#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <winsock2.h>
	#define PRIsize_t "Iu"
	#define htole16(x) (x)
	#define htole32(x) (x)
#define GETLINE_BUFFER 4096

//getline implementation from midimonster / https://github.com/cbdevnet/midimonster
static ssize_t getline(char** line, size_t* alloc, FILE* stream){
	size_t bytes_read = 0;
	char c;
	//sanity checks
	if(!line || !alloc || !stream){
		return -1;
	}

	//allocate buffer if none provided
	if(!*line || !*alloc){
		*alloc = GETLINE_BUFFER;
		*line = calloc(GETLINE_BUFFER, sizeof(char));
		if(!*line){
			fprintf(stderr, "Failed to allocate memory\n");
			return -1;
		}
	}

	if(feof(stream)){
		return -1;
	}

	for(c = fgetc(stream); 1; c = fgetc(stream)){
		//end of buffer, resize
		if(bytes_read == (*alloc) - 1){
			*alloc += GETLINE_BUFFER;
			*line = realloc(*line, (*alloc) * sizeof(char));
			if(!*line){
				fprintf(stderr, "Failed to allocate memory\n");
				return -1;
			}
		}

		//store character
		(*line)[bytes_read] = c;

		//end of line
		if(feof(stream) || c == '\n'){
			//terminate string
			(*line)[bytes_read + 1] = 0;
			return bytes_read;
		}

		//input broken
		if(ferror(stream)){
			return -1;
		}

		bytes_read++;
	}
}
#else
	#define PRIsize_t "lu"
#endif

typedef enum {
	fmt_i8,
	fmt_i16,
	fmt_i32,
	fmt_f32
} sample_format;

typedef struct {
	int64_t i64;
	float f32;
} sample_t;

#pragma pack(push, 1)
typedef struct /*_chunk_riff*/ {
	char magic_riff[4];
	uint32_t size;
	char magic_wave[4];	
} hdr_riff_t;

typedef struct /*_chunk_fmt_base*/ {
	char magic[4];
	uint32_t size;
	uint16_t fmt;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate; //(samplerate * sampleBits * channels) / 8.
	uint16_t bitdepth; //(sampleBits * channels) / 8
	uint16_t samplebits;
	uint16_t extsize; //only present if fmt != 1
} hdr_fmt_t;

typedef struct /*_chunk_fact*/ {
	char magic[4];
	uint32_t size;
	uint32_t samples;
} hdr_fact_t;

typedef struct /*_chunk_data*/ {
	char magic[4];
	uint32_t size;
} hdr_data_t;
#pragma pack(pop)

//TODO configurable csv delimiter

static int usage(char* fn){
	fprintf(stdout, "Call as %s <file.csv> <file.wav> <column> <samplerate> [<format>]\n", fn);
	fprintf(stdout, "Possible formats: i8, i16 (default), i32, f32");
	return EXIT_FAILURE;
}

static sample_format sampleformat_parse(char* fmt){
	if(!fmt){
		return fmt_i16;
	}

	if(!strcmp(fmt, "i8")){
		return fmt_i8;
	}

	else if(!strcmp(fmt, "i32")){
		return fmt_i32;
	}

	else if(!strcmp(fmt, "f32")){
		return fmt_f32;
	}

	return fmt_i16;
}

static size_t sampleformat_bits(sample_format fmt){
	switch(fmt){
		case fmt_i8:
			return 8;
		case fmt_i16:
			return 16;
		case fmt_i32:
		case fmt_f32:
			return 32;	
	}

	return 16;
}

static void push_sample(sample_t sample, sample_format fmt, int fd){
	uint8_t u8 = sample.i64;
	int16_t i16 = htole16(sample.i64);
	int32_t i32 = htole32(sample.i64);

	switch(fmt){
		case fmt_i8:
			write(fd, &u8, 1);
			break;
		case fmt_i16:
			write(fd, &i16, 2);
			break;
		case fmt_i32:
			write(fd, &i32, 4);
			break;
		case fmt_f32:
			write(fd, &sample.f32, 4);
			break;
	}
}

static size_t process(FILE* src, size_t column, sample_format fmt, int dst){
	char* line, *value;
	size_t offset = 0;
	size_t bytes_alloc = 0, samples = 0;
	ssize_t bytes_read = 0;
	sample_t sample;

	for(bytes_read = getline(&line, &bytes_alloc, src); bytes_read >= 0; bytes_read = getline(&line, &bytes_alloc, src)){
		offset = 0;
		for(value = line; *value; value++){
			if(offset == column){
				break;
			}

			if(*value == ','){
				offset++;
			}
		}

		if(!*value){
			fprintf(stderr, "Input row %" PRIsize_t " does not provide a sample column\n", samples);
			continue;
		}

		sample.i64 = strtoll(value, NULL, 0);
		sample.f32 = strtof(value, NULL);

		push_sample(sample, fmt, dst);

		samples++;
	}

	return samples;
}

static float* float_reference(int fd, size_t samples){
	float* data = calloc(samples, sizeof(float)), max = 0;
	size_t n = 0;

	for(n = 0; n < samples; n++){
		read(fd, data + n, 4);
		if(fabsf(data[n]) > fabsf(max)){
			max = data[n];
		}
	}

	fprintf(stdout, "Determined maximum sample value as %f\n", max);

	for(n = 0; n < samples; n++){
		data[n] /= fabsf(max);
	}
	return data;
}

static void write_headers(int fd, sample_format fmt, size_t samples, size_t samplerate){
	hdr_riff_t riff_hdr = {
		.magic_riff = "RIFF",
		.size = /*riff*/ 4 +
			/*fmt header*/ sizeof(hdr_fmt_t) - (fmt == fmt_f32 ? 0 : 2)
			+ /*fact header*/ (fmt == fmt_f32 ? sizeof(hdr_fact_t) : 0)
			+ /*data header */ sizeof(hdr_data_t)
			+ /*data*/ samples * (sampleformat_bits(fmt) / 8),
		.magic_wave = "WAVE",
	};

	hdr_fmt_t fmt_hdr = {
		.magic = "fmt ",
		.size = (fmt == fmt_f32) ? 18 : 16, //add extsize for floating point format
		.fmt = (fmt == fmt_f32) ? 3 : 1,
		.channels = 1,
		.sample_rate = samplerate,
		.byte_rate = (samplerate * sampleformat_bits(fmt)) / 8,
		.bitdepth = sampleformat_bits(fmt) / 8,
		.samplebits = sampleformat_bits(fmt)
	};

	hdr_fact_t fact_hdr = {
		.magic = "fact",
		.size = 4,
		.samples = samples
	};

	hdr_data_t data_hdr = {
		.magic = "data",
		.size = samples * (sampleformat_bits(fmt) / 8)
	};

	//fprintf(stdout, "Wrote %" PRIu32 " bytes, %" PRIu32 " samples\n", data_hdr.size, fact_hdr.samples);

	write(fd, &riff_hdr, sizeof(riff_hdr));
	write(fd, &fmt_hdr, sizeof(fmt_hdr) - (fmt == fmt_f32 ? 0 : 2));
	if(fmt == fmt_f32){
		write(fd, &fact_hdr, sizeof(fact_hdr));
	}
	write(fd, &data_hdr, sizeof(data_hdr));
}

int main(int argc, char** argv){
	if(argc < 5){
		return usage(argv[0]);
	}

	sample_format fmt = sampleformat_parse(argv[5]);

	FILE* in = fopen(argv[1], "r");
	if(!in){
		fprintf(stdout, "Failed to open input file %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	int output_fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if(output_fd < 0){
		fprintf(stderr, "Failed to open output file %s\n", argv[2]);
		close(output_fd);
		return EXIT_FAILURE;
	}

	write_headers(output_fd, fmt, 0, strtoul(argv[4], NULL, 10));
	size_t samples = process(in, strtoul(argv[3], NULL, 10), fmt, output_fd);
	fclose(in);

	fprintf(stdout, "Finalizing output file (%" PRIsize_t " samples)\n", samples);

	lseek(output_fd, 0, SEEK_SET);
	write_headers(output_fd, fmt, samples, strtoul(argv[4], NULL, 10));
	
	//floating point pcm needs to be normalized...
	if(fmt == fmt_f32){
		float* normalized = float_reference(output_fd, samples);
		lseek(output_fd, 0, SEEK_SET);
		write_headers(output_fd, fmt, samples, strtoul(argv[4], NULL, 10));
		for(size_t n = 0; n < samples; n++){
			write(output_fd, normalized + n, 4);
			//fprintf(stdout, "Normalized sample %f\n", normalized[n]);
		}
		free(normalized);
	}
	
	close(output_fd);
	return EXIT_SUCCESS;
}
