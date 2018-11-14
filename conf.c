/*
*     conf.c
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "conf.h"

/* Command line configuration options. Setting defaults here.  */

/* Configurable number of tries */  
int num_run = 0;

/* Configurable time to cut connect () in prog */  
int connect_timeout = 4;

/* Flag, whether to perform verbose logging */
int verbose_logging = 0;

/* Output to logfile the details of request/response. */
int detailed_logging = 0;

/* Name of the configuration file */
char config_file[PATH_MAX + 1];


/* forward declaration */

static int clients_num_tries_parser (client_context* const cctx, char *const value);
static int timer_tcp_conn_setup_parser (client_context *const ctx , char*const value);
//static int timer_url_completion_parser (client_context* const cctx, char *const value);
static int add_param_to_ctx (char*const input, size_t input_length, client_context *ctx);
static char* skip_non_ws (char*ptr, size_t*const len);
static char* eat_ws (char*ptr, size_t*const len);
static int is_ws (char*const ptr);
static int is_non_ws (char*const ptr);
static int pre_parser (char** ptr, size_t* len);

/* url related */
static int keep_alive_parser (client_context* const cctx, char *const value); 
static int url_parser (client_context* const cctx, char *const value); 
static int user_agent_parser (client_context* const cctx, char *const value); 
static int run_name_parser (client_context* const cctx, char *const value); 
static int max_num_headers_parser (client_context* const cctx, char *const value);
static int request_type_parser (client_context* const cctx, char *const value);
//static int fresh_connect_parser (client_context* const cctx, char *const value); 


typedef int (*fparser) (client_context* const cctx, char* const value);

/* Used to map a tag to its value parser function.  */
typedef struct tag_parser_pair {
	char* tag; /* string name of the param */
	fparser parser;
} tag_parser_pair;


/* The mapping between tag strings and parsing functions.  */
static const tag_parser_pair tag_map [] = {

	/* GENERAL SECTION */
	{"RUN_NAME", run_name_parser},
	{"NUM_TRIES", clients_num_tries_parser},
	{"USER_AGENT", user_agent_parser},

	/* URL SECTION  */
	{"URL", url_parser},
	{"HEADER", header_parser},
	{"MAX_NUM_HEADERS", max_num_headers_parser},
	{"REQUEST_TYPE", request_type_parser},
	{"KEEP_ALIVE", keep_alive_parser},

	{"TIMER_TCP_CONN_SETUP", timer_tcp_conn_setup_parser},
	/* {"TIMER_URL_COMPLETION", timer_url_completion_parser}, */
	/* {"TIMER_AFTER_URL_FETCH_SLEEP", timer_after_url_sleep_parser}, */

	/* LOG SECTION  */
	/* {"DUMP_STATS", dump_stats_parser}, */
	/* {"LOG_RESP_HEADERS", log_resp_headers_parser},*/
	/* {"LOG_RESP_BODY", log_resp_body_parser}, */

	{NULL, 0}
};


/*
* Description - Makes a look-up of a tag value parser function for an input tag-string
*
* Input -       *tag - pointer to the tag string, coming from the configuration file
* Returns     - On success - parser function, on failure - NULL
*/
static fparser find_tag_parser (const char* tag)
{
    size_t index;

    for (index = 0; tag_map[index].tag; index++) {
        if (!strcmp (tag_map[index].tag, tag))
            return tag_map[index].parser;
    }

    return NULL;

}


/*
 * Description - Eats leading white space. Returns pointer to the start of
 *                    the non-white-space or NULL. Returns via len, a new length.
 *
 * Input       - *ptr - pointer to the url context
 * Input       - *len - pointer to a length
 * Returns     - Returns pointer to the start of the non-white-space or NULL
 */
char* eat_ws (char* ptr, size_t*const len) {

	if (!ptr || !*len)
		return NULL;

	while (*len && is_ws (ptr))
		++ptr, --(*len);

	return *len ? ptr : NULL;

}


/*
 * Description - Skips non-white space. Returns pointer to the start of
 *                    the white-space or NULL. Returns via len, a new length.
 *
 * Input -               *ptr - pointer to the url context
 * Input/Output- *len - pointer to a length
 * Return  - Returns pointer to the start of the white-space or NULL
 */
static char* skip_non_ws (char*ptr, size_t*const len)
{
	if (!ptr || !*len)
		return NULL;

	while (*len && is_non_ws (ptr))
		++ptr, --(*len);

	return *len ? ptr : NULL;
}


/*
 * Description - Determines, whether a char pointer points to a white space
 * Input -               *ptr - pointer to the url context
 * Return  - If white space - 1, else 0
 */
static int is_ws (char*const ptr)
{
	return (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') ? 1 : 0;
}


/*
 * Description - Determines, whether a char pointer points to a non-white space
 * Input -       *ptr - pointer to the url context
 * Return  - If non-white space - 1, else 0
 */
static int is_non_ws (char*const ptr)
{
	return ! is_ws (ptr);
}


/*
* Description - Takes configuration file string of the form TAG = value and extacts
*               configuration parameters from it.
* Input       - *str_buff   - pointer to the configuration file string of the form TAG = value
*               str_len     - length of the <str_buff> string
*               ctx         - the client specific context 
* Return      - On success - 0, on failure - (-1)
*/
static int 
add_param_to_ctx (char*const str_buff, size_t  str_len, client_context* const ctx)
{
	if (!str_buff || !str_len || !ctx)
		 return -1;

	char* equal = NULL;

	if ( ! (equal = strchr (str_buff, '='))) {
      fprintf (stderr,
               "%s - error: input string \"%s\" is short of '=' sign.\n",
               __func__, str_buff) ;
      return -1;
	} else {
      *equal = '\0'; 
	}

	long string_length = (long) str_len;
	long val_len = 0;

	if ((val_len = string_length - (long)(equal - str_buff) - 1) < 0) {
      *equal = '=' ;
      fprintf(stderr, "%s - error: in \"%s\" a valid name should follow '='.\n",
               __func__, str_buff);
      return -1;
    }

  str_len = strlen (str_buff) + 1;
  char* str_end = skip_non_ws (str_buff, &str_len);

  if (str_end)
      *str_end = '\0';

  /* Lookup for value parsing function for the input tag */
  fparser parser = 0;

  if (! (parser = find_tag_parser (str_buff))) {
      fprintf (stderr, "%s - error: unknown tag %s.\n",
               __func__, str_buff);
      return -1;
  }

  size_t value_len = (size_t) val_len;
  char* value = equal + 1;

  if (pre_parser (&value, &value_len) == -1) {
      fprintf (stderr,"%s - error: pre_parser () failed for tag %s and value \"%s\".\n",
               __func__, str_buff, equal + 1);
      return -1;
  }

  if (!strlen (value)) {
      fprintf (stderr,"%s - warning: tag %s has an empty value string.\n",
               __func__, str_buff);
      return 0;
  }

  /* Remove quotes from the value */
  if (*value == '"') {
      value++, value_len--;
      if (value_len < 2) {
          return 0;
      } else {
          if (*(value +value_len-2) == '"') {
              *(value +value_len-2) = '\0';
              value_len--;
          }
      }
  }

  if ((*parser) (ctx, value) == -1) {
      fprintf (stderr,"%s - parser failed for tag %s and value %s.\n",
               __func__, str_buff, equal + 1);
      return -1;
    }

	return 0;
}


/*
 * Description - Prepares value token from the configuration file to parsing. Removes LWS,
 *               cuts off comments, removes quotes.
 *
 * Input      - **ptr - second pointer to value string
 *              *len  - pointer to the length of the value string
 * Return     - On success - 0, on failure - (-1)
*/
static int 
pre_parser (char** ptr, size_t* len) {

    char* value_start = NULL;
    char* quotes_closing = NULL;
    char* value_end = NULL;

    /* remove LWS */
    if ( ! (value_start = eat_ws (*ptr, len))) {
        fprintf (stderr, "%s - error: only LWS found in the value \"%s\".\n",
                __func__, value_start);
        return -1;
    }

    /* Cut-off the comments in value string, starting from '#' */
    char* comments = NULL;

    if ((comments = strchr (value_start, '#'))) {
        *comments = '\0'; 
        if (! (*len = strlen (value_start))) {
            fprintf (stderr, "%s - error: value \"%s\" has only comments.\n",
                    __func__, value_start);
            return -1;
        }
    }

    /* Everything after quotes closing */
    if (*value_start == '"') {
        /* Enable usage of quotted strings with white spaces inside, line User-Agent strings. */
        if (*(value_start + 1)) {
            if ((quotes_closing = strchr (value_start + 1, '"')))
                value_end = quotes_closing + 1;
        } else {
            value_end = value_start;
        }
    }

    /* If not quotted strings, cut the value on the first white space */
    if (!value_end)
        value_end = skip_non_ws (value_start, len);

    if (value_end) {
        *value_end = '\0';
    }

    *ptr = value_start;
    *len = strlen (value_start) + 1;

    return 0;
}


/*  TAG PARSERS IMPLEMENTATION */


static int 
run_name_parser (client_context* const ctx, 
                 char *const value) 
{
    strncpy (ctx->run_name, value, RUN_NAME_SIZE);

    return 0;
}


static int 
clients_num_tries_parser (client_context* const ctx, 
                          char *const value) 
{

    ctx->num_tries  = 0;
    ctx->num_tries  = atoi(value);

    if (ctx->num_tries < 0) {
        fprintf (stderr, "%s - error: number of run (%ld) is not valid\n", 
                __func__, ctx->num_tries);
        return -1;
    }

    return 0;
}


static int 
timer_tcp_conn_setup_parser (client_context *const ctx ,
                             char*const value)
{
    long timer = atol(value);

    if (timer <= 0 || timer > 50) {
        fprintf(stderr, 
                "%s error: input of the timer is expected  to be from " 
                "1 up to 50 seconds.\n", __func__);
        return -1;
    }

    ctx->url.connect_timeout = timer;

    return 0;
}


static int 
keep_alive_parser (client_context* const ctx, 
                   char* const value)
{
    long bol = atol(value);

    if (bol < 0 || bol > 1) {
        fprintf(stderr,
                "%s error: boolean input 0 or 1 is expected\n", __func__);
        return -1;
    }

    ctx->url.fresh_connect = bol;

    return 0;
}


int header_parser (client_context* const ctx, char* const value) {

    size_t hdr_len;

    if (!value || !(hdr_len = strlen (value))) {
        fprintf (stderr, "%s - error: wrong input.\n", __func__);
        return -1;
    }

    const char colon = ':';

    if (!strchr (value, colon)) {
        fprintf (stderr, 
                "%s - error: HTTP protocol requires \"%c\" colomn symbol" 
                " in HTTP headers.\n", __func__, colon);
        return -1;
    }

    if (ctx->url.custom_http_hdrs_num >= CUSTOM_HDRS_MAX_NUM) {
        fprintf (stderr, 
                "%s - error: number of custom HTTP headers is limited to %d.\n", 
                __func__, CUSTOM_HDRS_MAX_NUM);
        return -1;
    }

    if (!(ctx->url.custom_http_hdrs = curl_slist_append (ctx->url.custom_http_hdrs, value))) {
        fprintf (stderr, "%s - error: failed to append the header \"%s\"\n", 
                __func__, value);
        return -1;
    }

    ctx->url.custom_http_hdrs_num++;

    return 0;
}


static int 
url_parser (client_context* const ctx, char* const value) {

    size_t url_length = 0;	

    url_length = strlen(value);

    if (!(url_length = strlen (value))) {
        fprintf (stderr, "%s - error: empty url \"%s\"\n", __func__, value);
        return -1;
    } else if (! (ctx->url.url_str = (char *) calloc (url_length +1, sizeof (char)))) {
        fprintf (stderr, "%s - error: allocation failed for url string \"%s\"\n",
                __func__, value);
        return -1;
    }

    strncpy(ctx->url.url_str, value, url_length);

    return 0;
}


static int 
user_agent_parser (client_context* const ctx, char* const value) {

    if (strlen (value) <= 0) {
        fprintf(stderr, "%s - warning: empty USER_AGENT "
                "\"%s\", taking the defaults\n", __func__, value);
        return 0;
    }

    strncpy (ctx->user_agent, value, sizeof(ctx->user_agent) - 1);

    return 0;
}


static int 
max_num_headers_parser (client_context* const ctx, char* const value) {

    //strncpy (ctx->url.custom_http_hdrs_num, value, CUSTOM_HDRS_MAX_NUM);

    return 0;
}


static int 
request_type_parser (client_context* const rctx, char* const value) {
    
    //strncpy (ctx->run_name, value, RUN_NAME_SIZE);
    
    return 0;
}


/* At the moment the command line arguments are ignored 
 * except the read file option -f
 *
 * All the config paramemters are read from the config file */
int parse_command_line (int argc, char *argv []) {

    int rget_opt = 0;

    while ((rget_opt = getopt (argc, argv, "c:n:hf:v")) != EOF) {
        switch (rget_opt) 
        {
            case 'c': /* Connection establishment timeout */
                if (!optarg || (connect_timeout = atoi (optarg)) <= 0) {
                    fprintf (stderr, "%s error: -c option should be a positive number in seconds.\n", __func__);
                    return -1;
                }
                break;

            case 'n': /* Number of times to run */
                if (!optarg || (num_run = atoi (optarg)) <= 0) {
                    fprintf (stderr, "%s error: -n option should be a positive number.\n", __func__);
                    return -1;
                }
                break;

            case 'h': /* Help */
                print_help ();
                exit (0);

            case 'f': /* Configuration file */
                if (optarg)
                    strcpy(config_file, optarg);
                else {
                    fprintf (stderr, "%s error: -f option should be followed by a filename.\n", __func__);
                    return -1;
                }
                break;

            case 'v': /* Accumulate verbosity */
                verbose_logging += 1; 
                break;

            default: 
                fprintf (stderr, "%s error: not supported option\n", __func__);
                print_help ();
                return -1;
        }
    }

    if (optind < argc) {

        fprintf (stderr, "%s error: non-option argv-elements: ", __func__);

        while (optind < argc)
            fprintf (stderr, "%s ", argv[optind++]);

        fprintf (stderr, "\n");
        print_help ();

        return -1;
    }

    return 0;
}

/*
* Description - Parses configuration file and fills the client contexts struct 
* Input       - *filename  - name of the configuration file to parse.
* Input       - *ctx       - client contexts to be filled on parsing
* Return      - On Success - 0, on Error -1
*/
int 
parse_config_file(char* const filename, client_context *ctx) 
{
    char fgets_buff[1024*8];
    FILE* fp;
    struct stat statbuf;
    int line_no;

    /* Check, if the configuration file exists. */
    if (stat (filename, &statbuf) == -1) {
        fprintf (stderr,
                "%s - failed to find configuration file \"%s\" with errno %d.\n"
                ,__func__, filename, errno);
        print_help();
        return -1;
    }

    if (!(fp = fopen (filename, "r"))) {
        fprintf (stderr, 
                "%s - fopen() failed to open for reading filename \"%s\", errno %d.\n", 
                __func__, filename, errno);
        return -1;
    }

    while (fgets (fgets_buff, sizeof (fgets_buff) - 1, fp)) {
        size_t string_len = 0;
        line_no++;
        if ((string_len = strlen (fgets_buff))) {
            /* Line may be commented out by '#'.*/
            if (fgets_buff[0] == '#') {
                continue;
            }

            if (add_param_to_ctx (fgets_buff, string_len, ctx) == -1) {
                fprintf (stderr,
                        "%s - error: add_param_to_ctx () failed processing buffer \"%s\"\n",
                        __func__, fgets_buff);
                fclose (fp);
                return -1 ;
            }
        }
    }

    fclose (fp);

    return 0;
}

/* Print usage */
void print_help () {

  fprintf (stderr, "\nNote: to run your this program, create a configuration file.\n\n");

  fprintf (stderr, "Note: only -f option is used at the moment, rest of the config is read from the file \n\n");
  fprintf (stderr, "./samk -f <configuration file name> with [other options below]:\n\n");
  fprintf (stderr, " -c[onnection establishment timeout, seconds]\n");
  fprintf (stderr, " -d[etailed logging; outputs to logfile headers and bodies of requests/responses.]\n");
  fprintf (stderr, " -v[erbose output to the logfiles; includes info about headers sent/received]\n");
  fprintf (stderr, "\n");

  fprintf (stderr, "For examples of configuration files please, look at custom-headers.conf file in current dir \n");
  fprintf (stderr, "\n");
}

/* vim: set ts=4 sw=4 et sts=4:  */
