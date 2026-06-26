#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>

#define DEVICE_ID "thermostat1"

#define SERVER_BASE "http://3.17.65.32:3001"

#define TEMP_FILE "/tmp/temp"
#define STATUS_FILE "/tmp/status"

#define LOOP_DELAY_SECONDS 5
#define HEAT_DEADBAND 0.5

static volatile sig_atomic_t running = 1;

struct response_buffer {
    char data[4096];
    size_t size;
};

static void handle_signal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

static void setup_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

static void daemonize(void)
{
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(027);

    if (chdir("/") != 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

static int read_file_trimmed(const char *path, char *buffer, size_t size)
{
    FILE *fp;
    size_t len;

    fp = fopen(path, "r");

    if (fp == NULL) {
        return -1;
    }

    if (fgets(buffer, size, fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    len = strlen(buffer);

    while (len > 0 &&
           (buffer[len - 1] == '\n' ||
            buffer[len - 1] == '\r' ||
            buffer[len - 1] == ' ' ||
            buffer[len - 1] == '\t')) {
        buffer[len - 1] = '\0';
        len--;
    }

    return 0;
}

static int write_heater_status(const char *status)
{
    FILE *fp;

    fp = fopen(STATUS_FILE, "w");

    if (fp == NULL) {
        return -1;
    }

    fprintf(fp, "%s\n", status);
    fclose(fp);

    return 0;
}

/*
 * This function receives HTTP response data from libcurl.
 * Do not name this curl_write_callback because older libcurl headers
 * already use that name as a typedef.
 */
static size_t client_response_callback(char *contents, size_t size, size_t nmemb, void *userp)
{
    size_t real_size;
    size_t copy_size;
    struct response_buffer *response;

    real_size = size * nmemb;
    response = (struct response_buffer *)userp;

    if (response == NULL) {
        return real_size;
    }

    copy_size = real_size;

    if (response->size + copy_size >= sizeof(response->data) - 1) {
        copy_size = sizeof(response->data) - response->size - 1;
    }

    if (copy_size > 0) {
        memcpy(&(response->data[response->size]), contents, copy_size);
        response->size += copy_size;
        response->data[response->size] = '\0';
    }

    return real_size;
}

static int post_status(double temperature, const char *heater)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    struct response_buffer response;
    char url[256];
    char json[512];

    memset(&response, 0, sizeof(response));

    snprintf(url, sizeof(url), "%s/api/status", SERVER_BASE);

    snprintf(json, sizeof(json),
             "{\"device_id\":\"%s\",\"temperature\":%.2f,\"heater\":\"%s\"}",
             DEVICE_ID, temperature, heater);

    curl = curl_easy_init();

    if (curl == NULL) {
        syslog(LOG_ERR, "curl_easy_init failed for POST");
        return -1;
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, client_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        syslog(LOG_ERR, "POST failed: %s", curl_easy_strerror(res));
        return -1;
    }

    syslog(LOG_INFO, "posted status temp=%.2f heater=%s", temperature, heater);

    return 0;
}

static double get_target_temperature(void)
{
    CURL *curl;
    CURLcode res;
    struct response_buffer response;
    char url[256];
    char *target_ptr;
    double target = 72.0;

    memset(&response, 0, sizeof(response));

    snprintf(url, sizeof(url), "%s/api/program/%s", SERVER_BASE, DEVICE_ID);

    curl = curl_easy_init();

    if (curl == NULL) {
        syslog(LOG_ERR, "curl_easy_init failed for GET");
        return target;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, client_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        syslog(LOG_ERR, "GET program failed: %s", curl_easy_strerror(res));
        return target;
    }

    /*
     * Simple JSON parsing for the server response.
     * Example:
     * {"device_id":"thermostat1","mode":"auto","target_temp":75.0}
     */
    target_ptr = strstr(response.data, "target_temp");

    if (target_ptr != NULL) {
        target_ptr = strchr(target_ptr, ':');

        if (target_ptr != NULL) {
            target_ptr++;

            if (sscanf(target_ptr, "%lf", &target) == 1) {
                syslog(LOG_INFO, "received target temperature %.2f", target);
                return target;
            }
        }
    }

    syslog(LOG_WARNING, "could not parse target temperature, using default %.2f", target);

    return target;
}

int main(void)
{
    char temp_buffer[64];
    char heater_buffer[64];
    double temperature;
    double target;
    const char *new_heater_state;

    daemonize();

    openlog("thermostat_client", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    setup_signals();

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        syslog(LOG_ERR, "curl_global_init failed");
        closelog();
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "thermostat client started");

    while (running) {
        if (read_file_trimmed(TEMP_FILE, temp_buffer, sizeof(temp_buffer)) != 0) {
            syslog(LOG_ERR, "failed to read %s", TEMP_FILE);
            sleep(LOOP_DELAY_SECONDS);
            continue;
        }

        if (read_file_trimmed(STATUS_FILE, heater_buffer, sizeof(heater_buffer)) != 0) {
            syslog(LOG_ERR, "failed to read %s", STATUS_FILE);
            sleep(LOOP_DELAY_SECONDS);
            continue;
        }

        temperature = atof(temp_buffer);
        target = get_target_temperature();

        /*
         * Basic thermostat logic:
         * If the temperature is too low, turn the heater ON.
         * If the temperature is high enough, turn the heater OFF.
         * The deadband prevents rapid switching.
         */
        if (temperature < target - HEAT_DEADBAND) {
            new_heater_state = "ON";
        } else if (temperature > target + HEAT_DEADBAND) {
            new_heater_state = "OFF";
        } else {
            new_heater_state = heater_buffer;
        }

        if (strcmp(new_heater_state, heater_buffer) != 0) {
            if (write_heater_status(new_heater_state) == 0) {
                syslog(LOG_INFO, "changed heater from %s to %s",
                       heater_buffer, new_heater_state);

                strncpy(heater_buffer, new_heater_state, sizeof(heater_buffer) - 1);
                heater_buffer[sizeof(heater_buffer) - 1] = '\0';
            } else {
                syslog(LOG_ERR, "failed to write %s", STATUS_FILE);
            }
        }

        post_status(temperature, heater_buffer);

        sleep(LOOP_DELAY_SECONDS);
    }

    syslog(LOG_INFO, "thermostat client stopped");

    curl_global_cleanup();
    closelog();

    return EXIT_SUCCESS;
}
