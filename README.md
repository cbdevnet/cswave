# cswave 

`cswave` is a small, cross-platform utility to convert CSV sample data to WAVE files. This can be used to do further analysis or processing on the
data using audio tooling.

Any column of the input file can be used as sample data for a resulting monophonic wave file, in one of the supported formats

* `i8`: 8 bit unsigned integer data
* `i16`: 16bit signed little-endian data
* `i32`: 32bit signed little-endian data
* `f32`: 32bit IEEE float data
* `nf32`: 32bit IEEE float data, but without the normalization step

Note that the integer output data formats only use the integer part of the input data, ie. when using the `i32` output format, the input sample
value `1.23` is truncated to the output sample value `1`.

When using the integer output data formats, the exact sample value is preserved (as long as it fits the storage size). For the `f32` format, the
specification requires that the samples be normalized to the range `-1.0` to `1.0`, thus the sample values will not be preserved. Use the `nf32`
sample format when your input data is already normalized and you want to preserve the exact sample values.

Quoted text embedding the separation character within CSV columns will confuse the CSV parser. Don't do that.

## Building

To build `cswave`, running `make` within the project directory on a machine with a working C compiler should work.

To build the windows executable `cswave.exe`, the `mingw-w64` crosscompiler is required. Use `make cswave.exe` to build it.

## Usage

Call the tool using the invocation

```
./cswave <file.csv> <file.wav> <column> <samplerate> [<format>] [<delimiter>]
```

* `<file.csv>`: CSV input file
* `<file.wav>`: Output file, will be overwritten without question if it already exists
* `<column>`: Zero-based column index within the CSV (e.g., `0`)
* `<samplerate>`: Integer sample rate of the resulting output file (e.g., `44100`)
* `<format>`: Output file sample format as listed above, default `i16`
* `<delimiter>`: Specify a single character to use as the delimiter within the CSV input, default `,`
