/*
 * ECE 531 HTTP Client Assignment
 * Supports GET, POST, PUT, and DELETE using libcurl.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <getopt.h>

/*
 * Enum representing supported HTTP methods.
 */
typedef enum {

    METHOD_NONE,
    METHOD_GET,
    METHOD_POST,
    METHOD_PUT,
    METHOD_DELETE

} http_method_t;

/*
 * Structure used to store response data from libcurl.
 */
struct memory {

    char *response;
    size_t size;
};

/*
 * Callback function used by libcurl to store
 * returned HTTP response data.
 */
size_t write_callback(void *contents,
                      size_t size,
                      size_t nmemb,
                      void *userp)
{
    size_t total_size = size * nmemb;

    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response,
                        mem->size + total_size + 1);

    if(ptr == NULL) {

        return 0;
    }

    mem->response = ptr;

    memcpy(&(mem->response[mem->size]),
           contents,
           total_size);

    mem->size += total_size;

    mem->response[mem->size] = '\0';

    return total_size;
}

/*
 * Print program usage/help information.
 */
void print_help()
{
    printf("Usage:\n");

    printf("./arm-hw --get --url <url>\n");

    printf("./arm-hw --post --url <url> <data>\n");

    printf("./arm-hw --put --url <url> <data>\n");

    printf("./arm-hw --delete --url <url> <data>\n\n");

    printf("Options:\n");

    printf("-u --url\n");
    printf("-g --get\n");
    printf("-o --post\n");
    printf("-p --put\n");
    printf("-d --delete\n");
    printf("-h --help\n");
}

int main(int argc, char *argv[])
{
    CURL *curl;

    CURLcode res;

    long response_code;

    char *url = NULL;

    char data[2048] = "";

    int opt;

    int verb_count = 0;

    http_method_t method = METHOD_NONE;

    struct memory chunk;

    chunk.response = malloc(1);

    chunk.size = 0;

    /*
     * Define long command line options.
     */
    static struct option long_options[] = {

        {"url", required_argument, 0, 'u'},
        {"get", no_argument, 0, 'g'},
        {"post", no_argument, 0, 'o'},
        {"put", no_argument, 0, 'p'},
        {"delete", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},

        {0, 0, 0, 0}
    };

    /*
     * Parse command line arguments.
     */
    while((opt = getopt_long(argc,
                             argv,
                             "u:gopdh",
                             long_options,
                             NULL)) != -1)
    {
        switch(opt) {

            case 'u':

                url = optarg;

                break;

            case 'g':

                method = METHOD_GET;

                verb_count++;

                break;

            case 'o':

                method = METHOD_POST;

                verb_count++;

                break;

            case 'p':

                method = METHOD_PUT;

                verb_count++;

                break;

            case 'd':

                method = METHOD_DELETE;

                verb_count++;

                break;

            case 'h':

                print_help();

                return 0;

            default:

                print_help();

                return 1;
        }
    }

    /*
     * Verify exactly one HTTP method was selected.
     */
    if(verb_count != 1) {

        fprintf(stderr,
                "Error: specify exactly one HTTP method.\n");

        return 1;
    }

    /*
     * Verify URL was provided.
     */
    if(url == NULL) {

        fprintf(stderr,
                "Error: URL required.\n");

        return 1;
    }

    /*
     * Collect remaining arguments into data string.
     */
    for(int i = optind; i < argc; i++) {

        strcat(data, argv[i]);

        if(i < argc - 1) {

            strcat(data, " ");
        }
    }

    /*
     * POST and PUT require data.
     */
    if((method == METHOD_POST ||
        method == METHOD_PUT) &&
        strlen(data) == 0)
    {
        fprintf(stderr,
                "Error: data required.\n");

        return 1;
    }

    /*
     * Initialize libcurl.
     */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();

    if(curl == NULL) {

        fprintf(stderr,
                "Failed to initialize curl.\n");

        return 1;
    }

    /*
     * Configure URL and callback.
     */
    curl_easy_setopt(curl,
                     CURLOPT_URL,
                     url);

    curl_easy_setopt(curl,
                     CURLOPT_WRITEFUNCTION,
                     write_callback);

    curl_easy_setopt(curl,
                     CURLOPT_WRITEDATA,
                     (void *)&chunk);

    /*
     * Configure libcurl based on selected method.
     */
    switch(method) {

        case METHOD_GET:

            curl_easy_setopt(curl,
                             CURLOPT_HTTPGET,
                             1L);

            break;

        case METHOD_POST:

            curl_easy_setopt(curl,
                             CURLOPT_POST,
                             1L);

            curl_easy_setopt(curl,
                             CURLOPT_POSTFIELDS,
                             data);

            break;

        case METHOD_PUT:

            curl_easy_setopt(curl,
                             CURLOPT_CUSTOMREQUEST,
                             "PUT");

            curl_easy_setopt(curl,
                             CURLOPT_POSTFIELDS,
                             data);

            break;

        case METHOD_DELETE:

            curl_easy_setopt(curl,
                             CURLOPT_CUSTOMREQUEST,
                             "DELETE");

            curl_easy_setopt(curl,
                             CURLOPT_POSTFIELDS,
                             data);

            break;

        default:

            fprintf(stderr,
                    "Invalid HTTP method.\n");

            return 1;
    }

    /*
     * Execute HTTP request.
     */
    res = curl_easy_perform(curl);

    if(res != CURLE_OK) {

        fprintf(stderr,
                "curl error: %s\n",
                curl_easy_strerror(res));

        curl_easy_cleanup(curl);

        free(chunk.response);

        return 1;
    }

    /*
     * Retrieve HTTP response code.
     */
    curl_easy_getinfo(curl,
                      CURLINFO_RESPONSE_CODE,
                      &response_code);

    /*
     * Print response code.
     */
    printf("HTTP CODE: %ld\n",
           response_code);

    /*
     * Print response body if present.
     */
    if(chunk.response != NULL) {

        printf("%s\n",
               chunk.response);
    }

    /*
     * Cleanup resources.
     */
    curl_easy_cleanup(curl);

    curl_global_cleanup();

    free(chunk.response);

    return 0;
}
