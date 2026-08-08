/*
 * oracool_mpq_extract.c
 *
 * Minimal standalone MPQ file extractor, built against the same libmpq
 * sources DevilutionX itself vendors (3rdParty/libmpq via CMake FetchContent)
 * to read its own data files at runtime. Written for the Oracool Edition UI
 * asset pipeline: pulling original .CEL/.CL2/.PAL/.TRN files out of
 * diabdat.mpq/hellfire.mpq so they can be decoded to PNG (see
 * oracool_cel_to_png.ps1 in this same folder) and used as reference/source
 * art for the 960x720 UI overhaul.
 *
 * Usage:
 *   oracool_mpq_extract.exe <mpq_path> <manifest.txt> <output_dir>
 *
 * manifest.txt: one MPQ-internal path per line (backslashes, e.g.
 * "data\char\charbut.cel"), blank lines and lines starting with '#' ignored.
 * Each extracted file is written to <output_dir>\<same internal path>,
 * creating any needed subdirectories.
 *
 * Build (from a Visual Studio "x64 Native Tools" command prompt, or after
 * running vcvars64.bat):
 *
 *   cl /nologo /O2 /I "3rdParty\libmpq" /I "build\x64-Release\_deps\libmpq-src" ^
 *      /I "build\x64-Release\vcpkg_installed\x64-windows\include" ^
 *      tools\oracool_mpq_extract.c ^
 *      build\x64-Release\_deps\libmpq-src\libmpq\common.c ^
 *      build\x64-Release\_deps\libmpq-src\libmpq\explode.c ^
 *      build\x64-Release\_deps\libmpq-src\libmpq\extract.c ^
 *      build\x64-Release\_deps\libmpq-src\libmpq\huffman.c ^
 *      build\x64-Release\_deps\libmpq-src\libmpq\mpq.c ^
 *      build\x64-Release\_deps\libmpq-src\libmpq\wave.c ^
 *      /link /LIBPATH:"build\x64-Release\vcpkg_installed\x64-windows\lib" zlib.lib bz2.lib ^
 *      /out:tools\oracool_mpq_extract.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "libmpq/mpq.h"

static void MakeDirsForFile(const char *outputPath)
{
	char buf[MAX_PATH];
	size_t len = strlen(outputPath);
	size_t i;

	if (len >= sizeof(buf))
		return;
	memcpy(buf, outputPath, len + 1);

	for (i = 0; i < len; i++) {
		if (buf[i] == '\\' || buf[i] == '/') {
			char save = buf[i];
			buf[i] = '\0';
			if (buf[0] != '\0')
				CreateDirectoryA(buf, NULL);
			buf[i] = save;
		}
	}
}

static int ExtractOne(mpq_archive_s *archive, const char *internalPath, const char *outputDir)
{
	uint32_t fileNumber;
	libmpq__off_t unpackedSize;
	libmpq__off_t transferred;
	uint8_t *buffer;
	char outputPath[MAX_PATH];
	FILE *out;
	int32_t rc;

	rc = libmpq__file_number(archive, internalPath, &fileNumber);
	if (rc != 0) {
		fprintf(stderr, "  NOT FOUND: %s (rc=%d)\n", internalPath, rc);
		return 1;
	}

	rc = libmpq__file_size_unpacked(archive, fileNumber, &unpackedSize);
	if (rc != 0) {
		fprintf(stderr, "  SIZE ERROR: %s (rc=%d)\n", internalPath, rc);
		return 1;
	}

	buffer = (uint8_t *)malloc((size_t)unpackedSize);
	if (buffer == NULL) {
		fprintf(stderr, "  OUT OF MEMORY: %s (%lld bytes)\n", internalPath, (long long)unpackedSize);
		return 1;
	}

	rc = libmpq__file_read(archive, fileNumber, buffer, unpackedSize, &transferred);
	if (rc != 0 || transferred != unpackedSize) {
		fprintf(stderr, "  READ ERROR: %s (rc=%d, got %lld of %lld bytes)\n",
		    internalPath, rc, (long long)transferred, (long long)unpackedSize);
		free(buffer);
		return 1;
	}

	if (_snprintf(outputPath, sizeof(outputPath), "%s\\%s", outputDir, internalPath) < 0) {
		fprintf(stderr, "  PATH TOO LONG: %s\n", internalPath);
		free(buffer);
		return 1;
	}

	MakeDirsForFile(outputPath);

	out = fopen(outputPath, "wb");
	if (out == NULL) {
		fprintf(stderr, "  CANNOT WRITE: %s\n", outputPath);
		free(buffer);
		return 1;
	}
	fwrite(buffer, 1, (size_t)unpackedSize, out);
	fclose(out);
	free(buffer);

	printf("  OK: %s (%lld bytes) -> %s\n", internalPath, (long long)unpackedSize, outputPath);
	return 0;
}

int main(int argc, char **argv)
{
	const char *mpqPath;
	const char *manifestPath;
	const char *outputDir;
	FILE *manifest;
	mpq_archive_s *archive;
	char line[1024];
	int failures = 0;
	int32_t rc;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <mpq_path> <manifest.txt> <output_dir>\n", argv[0]);
		return 2;
	}
	mpqPath = argv[1];
	manifestPath = argv[2];
	outputDir = argv[3];

	CreateDirectoryA(outputDir, NULL);

	manifest = fopen(manifestPath, "r");
	if (manifest == NULL) {
		fprintf(stderr, "Cannot open manifest: %s\n", manifestPath);
		return 2;
	}

	rc = libmpq__archive_open(&archive, mpqPath, -1);
	if (rc != 0) {
		fprintf(stderr, "Cannot open MPQ: %s (rc=%d)\n", mpqPath, rc);
		fclose(manifest);
		return 2;
	}
	printf("Opened %s\n", mpqPath);

	while (fgets(line, sizeof(line), manifest) != NULL) {
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
			line[--len] = '\0';
		}
		if (len == 0 || line[0] == '#')
			continue;

		if (ExtractOne(archive, line, outputDir) != 0)
			failures++;
	}

	libmpq__archive_close(archive);
	fclose(manifest);

	printf("Done. %d failure(s).\n", failures);
	return failures == 0 ? 0 : 1;
}
