#ifndef VLM_JSON_TEST
#define VLM_JSON_TEST

/*
 * Run a test suite with the given short and long JSON files.
 * The short file is expected to be between 3k and 4k in length.
 * The long file is expected to be more than 5 MB long.
 * 
 * The (callback) is invoked some number of times over the data
 * extracted from the files. The callback is expected to initialize
 * and completely dispose of all of its JSON parser resources on each
 * invocation.
 *
 * When (zero_termination_required) flag is set, the json parameter is
 * made '\0'-terminated. Otherwise, an area of memory just abruptly ends up
 * with an unreadable chunk of memory using mprotect(2) right after
 * the last useful byte. This is done to check for buffer overruns.
 */
void vlm_json_perftest(const char *short_file_name,
                       const char *long_file_name,
                       void (*callback)(char *json, size_t len),
                       int zero_termination_required);

#endif  /* VLM_JSON_TEST */
