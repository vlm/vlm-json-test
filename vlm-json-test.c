#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <assert.h>

/*
 * Descriptor of a memory block with a guarded page.
 */
typedef struct {
    void *p;        /* Useful pointer to a memory of exactly (size) bytes. */
    size_t size;
    void *guard_p;  /* Pointes at the guard page */

    /* Data for future munmap(2) */
    void *unmap_p;
    size_t unmap_size;
} guarded_mem_block;

/*
 * Allocate a given amount of memory and place a guard page right after it.
 * The call returns the pointer to exactly (expected_size) number of
 * accessible bytes. Access to [expected_size] byte is going to generate
 * a segmentation fault.
 */
static guarded_mem_block mmap_with_guard_page(size_t expected_size) {
    size_t rounded_size = (expected_size + 4095) & ~4095;
    guarded_mem_block gmem;

    gmem.unmap_size = rounded_size + 4096;
    gmem.unmap_p = mmap(0, gmem.unmap_size, PROT_READ | PROT_WRITE,
                                            MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(gmem.unmap_p);
    gmem.guard_p = gmem.unmap_p + rounded_size;

    // Establish a guard page which can not be accessed.
    int ret = mprotect(gmem.unmap_p + rounded_size, 4096, PROT_NONE);
    assert(ret == 0);

    gmem.size = expected_size;
    gmem.p = gmem.unmap_p + 4096 - (expected_size & 4095);

    return gmem;
}

/*
 * Dispose of mmapped region of memory.
 */
static void unmap_guarded_block(guarded_mem_block gmem) {
    munmap(gmem.unmap_p, gmem.unmap_size);
}

/*
 * For every prefix of the given short json file, do the following:
 * 1) Place it at the very end of the guarded memory block.
 * 2) Change the last meaningful byte to a value in [-128..127] range.
 * 3) Attempt to call a parsing callback.
 * 4) Repeat with the shorter prefix.
 */
static void munch_short_json(const char *json, size_t json_length, void (*callback)(char *json, size_t len), int zero_termination_required) {

    guarded_mem_block gmem = mmap_with_guard_page(json_length + 1);

    int pfxlen;
    for(pfxlen = json_length; pfxlen > 1; pfxlen--) {
        char *copy = gmem.guard_p - pfxlen - (zero_termination_required?1:0);
        memcpy(copy, json, pfxlen);

        unsigned char *munchchar = (unsigned char *)&copy[pfxlen - 1];
        if(zero_termination_required)
            copy[pfxlen] = '\0';

        int j;
        for(j = 0; j <= 255; j++) {
            *munchchar = j;
            callback(copy, pfxlen);
        }

        assert(memcmp(copy, json, pfxlen - 2) == 0);
    }

    unmap_guarded_block(gmem);
}

/*
 * Slurp the whole file into memory.
 * 
 * RETURN VALUES:
 *   A pointer to the newly allocated 0-terminated buffer is returned.
 *   The (*size) argument is filled with the total length of meaningful data.
 */
static char *load_file(const char *filename, size_t *size) {
    FILE *f = fopen(filename, "rb");
    if(!f) {
        *size = 0;
        return NULL;
    } else {
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *buf = malloc(file_size + 1);
        assert(buf);
        size_t r = fread(buf, 1, file_size, f);
        fclose(f);

        assert(r == (size_t)file_size);
        buf[file_size] = '\0';
        *size = file_size;
        return buf;
    }
}

/*
 * Run a test suite with the given short and long JSON files.
 * See the corresponding .h file for details.
 */
void vlm_json_perftest(const char *short_file_name, const char *long_file_name, void (*callback)(char *json, size_t len), int zero_termination_required) {

    int i;
    struct timeval tvStart, tvEnd;
    double elapsed;

    /*
     * Deal with the long file first. Iterate a few times and discover
     * the megabytes/second performance.
     */
    size_t long_json_length;
    char *long_json = load_file(long_file_name, &long_json_length);
    assert(long_json_length > 5000000); // 5M, more than a typical L2 cache.

    printf("Parse %s of %ld bytes\n",
           long_file_name, (long)long_json_length);
    gettimeofday(&tvStart, 0);
    for(i = 0; i < 100; i++) {
        callback(long_json, long_json_length);
    }
    gettimeofday(&tvEnd, 0);
    elapsed = (tvEnd.tv_sec + tvEnd.tv_usec/1000000.0)
                   - (tvStart.tv_sec + tvStart.tv_usec/1000000.0);
    printf("Long file parsing %.1f MB/sec.\n",
        (i * long_json_length) / elapsed / (1024 * 1024));

    /*
     * Deal with the short file first. Modify file to attempt to corrupt it
     * and feed to the decoder repeatedly.
     */
    size_t short_json_length;
    char *short_json = load_file(short_file_name, &short_json_length);
    assert(short_json_length > 3000 && short_json_length < 4096);

    printf("Munch %s of %ld bytes\n",
           short_file_name, (long)short_json_length);

    gettimeofday(&tvStart, 0);
    munch_short_json(short_json, short_json_length,
                     callback, zero_termination_required);
    gettimeofday(&tvEnd, 0);
    elapsed = (tvEnd.tv_sec + tvEnd.tv_usec/1000000.0)
                   - (tvStart.tv_sec + tvStart.tv_usec/1000000.0);
    printf("Short file munching in %.1f s.\n", elapsed);
}
