/*
*     conf.h
*/

#ifndef CONF_H
#define CONF_H

#include <stddef.h>
#include <linux/limits.h> /* NAME_MAX, PATH_MAX */
#include <curl/curl.h>

#include "url.h"

#define RUN_NAME_SIZE 64


/* configuration parameter, from the command-line. Number of times to run  */
extern int num_run;

/* Configurable time to perform connect. If connection has not been established
   within this time limit then the connection attempt in progress will be terminated.  
*/  
extern int connect_timeout;

/* Flag; whether to perform verbose logging. Usefull for debugging. The files
   tend to become huge. Thus, they can be kept smaller by using logfile_rewind_size.
*/
extern int verbose_logging;

/* Name of the configuration file.  */
extern char config_file[PATH_MAX + 1];


/* HTTP requests: GET, POST and PUT.  */
typedef enum req_type {
	HTTP_REQ_TYPE_FIRST= 0,

	HTTP_REQ_TYPE_GET = 1,
	HTTP_REQ_TYPE_POST = 2,
	HTTP_REQ_TYPE_PUT = 3,
	HTTP_REQ_TYPE_HEAD = 4,
	HTTP_REQ_TYPE_DELETE = 5,

	HTTP_REQ_TYPE_LAST = 6,

} req_type;

/*Time info and the result info of the run*/
typedef struct client_stats {
	curl_off_t total_time;
	curl_off_t namelookup_time;
	curl_off_t connect_time;
	curl_off_t start_transfer_time;
	long int resp_code;
	char server_ip [16];
} client_stats;


/*Client context for a specific run*/
typedef struct client_context {

	/* GENERAL SECTION */

	/* Some non-empty name of a run  */
	char run_name[RUN_NAME_SIZE];
	/* Logfile <run-name>.log */
	char runtime_logfile[RUN_NAME_SIZE];
	/* Statistics file <run-name>.stat */
	char runtime_statistics[RUN_NAME_SIZE];
	/* Number of times to repeat the fetch urls  */
	long num_tries;
	/* identifies the current run, among previous ones */
	long current_run;
	/* Time to run in msec.  Zero means time to run is infinite.  */
	unsigned long run_time;
	/* User-agent string to appear in the HTTP 1/1 requests.  */
	char user_agent[256];

	/* URL SECTION - fetching urls */

	/* contains all specifics related to url */
	url_context url;

	/* statistics related */
	client_stats st;

	/* HELPER SECTION */

	/* Library handle, for using libcurl API.  */
	CURL* handle;

	/* Common error buffer for clients context */
	char error_buffer[CURL_ERROR_SIZE];

	/* Indicates that request scheduling is over */
	int requests_completed;

	/* STATISTICS  */

	/* The file to be used for statistics output */
	FILE* statistics_file;

	/* Timestamp, when the loading started */
	unsigned long start_time; 

	/* The last timestamp */
	unsigned long last_measure;

} client_context;


/* Parses command line and fills the configuration params. */
int parse_command_line (int argc, char *argv []);

/* Parses config file and fills the configuration params. */
int parse_config_file(char* const filename, client_context *ctx); 

/* Prints out usage of the program.  */
void print_help ();

/* Sets the header required for the run */
int header_parser (client_context* ctx, char* const value);

//void set_timer_handling_func (client_context* ctx, handle_timer);

#endif /* CONF_H */

/* vim: set ts=4 sw=4 et sts=4:  */
