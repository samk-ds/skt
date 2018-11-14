/* 
 *     run_context.c
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <errno.h>
#include <curl/curl.h>

#include "conf.h"
#include "url.h"
#include "run_context.h"

#define MAX_HEADER_LEN 50

/* forward declaration */
static int
debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);
//static size_t 
//writefunction( void *ptr, size_t size, size_t nmemb, void *stream);
static size_t 
do_nothing_write_func (void *ptr, size_t size, size_t nmemb, void *stream);
static int setup_handle_appl (struct client_context* const ctx);

/*
* Description - Gets the statistics info from the run 
*
* input  -  the client specific context structure
* Output - On Success - 0, on Error -1
*/
int get_stats_info (client_context *ctx) {

	curl_off_t val;
	long response_status = 0;
	int res;
	char *ip;

	if (!ctx) {
		return -1;
	}

	ctx->error_buffer[0] = 0;

	if (setup_init (ctx) == -1) {
		fprintf (stderr,"%s - error: setup init failed.\n",__func__);
		return -1;
	}

	res = curl_easy_perform(ctx->handle);

	/* if the request did not complete correctly, show the error
	   information. if no detailed error information was written to errbuf
	   show the more generic information from curl_easy_strerror instead.  */

	if (res != CURLE_OK) {

		size_t len = strlen(ctx->error_buffer);

		if (len) {
			fprintf(stderr, "%s%s", ctx->error_buffer,
					((ctx->error_buffer[len - 1] != '\n') ? "\n" : ""));
		} else {
			fprintf(stderr, "%s\n", curl_easy_strerror(res));
		}

		return -1;
	}

	if (CURLE_OK == res) {
		/* total time for the execution */
		res = curl_easy_getinfo(ctx->handle, CURLINFO_TOTAL_TIME_T, &val);

		if ((CURLE_OK == res) && (val>0)) {
			ctx->st.total_time = val;
		}  else {
			fprintf(stderr, "Error geting info total time '%s' : %s\n", 
					ctx->url.url_str, curl_easy_strerror(res));

			return -1;
		}
	}

	/* check for name resolution time */ 
	res = curl_easy_getinfo(ctx->handle, CURLINFO_NAMELOOKUP_TIME_T, &val);

	if ((CURLE_OK == res) && (val>0)) {
		ctx->st.namelookup_time = val;
	} else {
		fprintf(stderr, "Error geting info name lookup time '%s' : %s\n",
				ctx->url.url_str, curl_easy_strerror(res));
		return -1;
	}

	/* check for connect time */ 
	res = curl_easy_getinfo(ctx->handle, CURLINFO_CONNECT_TIME_T, &val);

	if ((CURLE_OK == res) && (val>0)) {
		ctx->st.connect_time = val;
	} else {
		fprintf(stderr, "Error geting info connect time '%s' : %s\n", 
				ctx->url.url_str, curl_easy_strerror(res));
		return -1;
	}

	/* get the ip we are connected to */
	if ((res == CURLE_OK) && 
			!curl_easy_getinfo(ctx->handle, CURLINFO_PRIMARY_IP, &ip) 
			&& ip) {
		strncpy(ctx->st.server_ip,ip,15);	  
	} else {
		fprintf(stderr, "Error geting info IP '%s' : %s\n", 
				ctx->url.url_str, curl_easy_strerror(res));
		return -1;
	}

	/* the server sent us a response code */
	res = curl_easy_getinfo (ctx->handle, CURLINFO_RESPONSE_CODE, &response_status);

	if (CURLE_OK == res) {
		ctx->st.resp_code = response_status;
	} else {
		fprintf(stderr, "Error geting info response code '%s' : %s\n", 
				ctx->url.url_str, curl_easy_strerror(res));
		return -1;
	}

	/* Time the transfer started */
	res = curl_easy_getinfo(ctx->handle, CURLINFO_STARTTRANSFER_TIME_T, &val);

	if ((CURLE_OK == res) && (val>0)) {
		ctx->st.start_transfer_time = val;
	} else {
		fprintf(stderr, "Error geting info start transfer time '%s' : %s\n",
				ctx->url.url_str, curl_easy_strerror(res));
		return -1;
	}

	/* always cleanup */
	curl_easy_cleanup(ctx->handle);

	return 0; 
}


/*
* Description - Application/url-type specific setup for a single curl handle (client)
*
* Input -       *ctx- pointer to client context, containing CURL handle pointer;
*
* Return Code/Output - On Success - 0, on Error -1
****************************************************************************************/
static int setup_handle_appl (client_context* const ctx)
{

	if (!ctx) {
		return -1;
	}

	/* Follow possible HTTP-redirection */
	curl_easy_setopt (ctx->handle, CURLOPT_FOLLOWLOCATION, 1);

	/* Enable infinitive (-1) redirection number. */
	curl_easy_setopt (ctx->handle, CURLOPT_MAXREDIRS, -1);

	char buffer[MAX_HEADER_LEN+1];
	int i = 0;
	int ret = -1;

    /* this while loop exists just for the demo, so that we can run in a loop 
     * and collect the statistics 
     *
	 * It sends some extra header in each run we connect to the server.
	 * it also preserves the header added from the config file, the number of 
	 * header added is always less than the max defined header */

	while (i < ctx->current_run) {

		snprintf(buffer, MAX_HEADER_LEN, "Header-name-%d: Header-value-%d", i, i);

		if ((ret  = header_parser(ctx, buffer)) != 0)  {
			fprintf(stderr, "%s: Failed to add custom header \n", __func__);
			return -1;
		}

		ctx->url.custom_http_hdrs_num++; i++;
	}

	/* Setup the custom (HTTP) headers, if appropriate.  */
	curl_easy_setopt (ctx->handle, CURLOPT_HTTPHEADER, ctx->url.custom_http_hdrs);

	//curl_easy_setopt (ctx->handle, CURLOPT_HTTPHEADER, ctx->url.custom_http_hdrs);

	if (ctx->url.req_type == HTTP_REQ_TYPE_POST) {
		/* Make POST, using post buffer, if requested.*/
	} else if (ctx->url.req_type == HTTP_REQ_TYPE_PUT) {

	} else if (ctx->url.req_type == HTTP_REQ_TYPE_HEAD) {
		//curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "HEAD");
	} else if (ctx->url.req_type == HTTP_REQ_TYPE_DELETE) {
		//curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
	}

	return 0;
}


/*
 * Description - initialises client context kept CURL handle, also uses
 *               setup_handle_appl () function for the application-specific
 *               (HTTP/FTP) initialization.
 *
 * Input    -   *ctx- pointer to client context, containing CURL handle pointer;
 * Returns  - On Success - 0, on Error -1
 ******************************************************************************/
int setup_init (client_context* ctx) {

	if (!ctx) {
		return -1;
	}

	/* init libcurl */ 
	curl_global_init(CURL_GLOBAL_ALL);

	/* init the curl session */ 
	ctx->handle = curl_easy_init();

	/* Set the url */
	if (ctx->url.url_str) {
		curl_easy_setopt (ctx->handle, CURLOPT_URL, ctx->url.url_str);
	} else {
		fprintf (stderr,"%s - error: empty url provided.\n", __func__);
		return -1;
	}

	/* disable dns caching */
	curl_easy_setopt (ctx->handle, CURLOPT_DNS_CACHE_TIMEOUT, 0);

	/* lets work with only ipv4 */
	curl_easy_setopt(ctx->handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

	/* Set the connection timeout */
	curl_easy_setopt (ctx->handle, CURLOPT_CONNECTTIMEOUT, 
			ctx->url.connect_timeout ? ctx->url.connect_timeout : connect_timeout);

	/* Define the connection re-use policy. When passed 1, re-establish */
	curl_easy_setopt (ctx->handle, CURLOPT_FRESH_CONNECT, ctx->url.fresh_connect);

	if (ctx->url.fresh_connect) {
		curl_easy_setopt (ctx->handle, CURLOPT_FORBID_REUSE, 1);
	}

	/* enable verbose output  */
	curl_easy_setopt (ctx->handle, CURLOPT_VERBOSE, 0);
	curl_easy_setopt (ctx->handle, CURLOPT_DEBUGFUNCTION, debug_callback);
	curl_easy_setopt (ctx->handle, CURLOPT_DEBUGDATA, ctx);

	/* write data */
	curl_easy_setopt (ctx->handle, CURLOPT_WRITEFUNCTION, do_nothing_write_func);
	//curl_easy_setopt (ctx->handle, CURLOPT_WRITEFUNCTION, writefunction);
	//curl_easy_setopt (ctx->handle, CURLOPT_WRITEDATA, create_file(log_file));

	curl_easy_setopt (ctx->handle, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt (ctx->handle, CURLOPT_SSL_VERIFYHOST, 0);

	/* Set the private pointer to pass around */
	//curl_easy_setopt (ctx->handle, CURLOPT_PRIVATE, ctx);

	/* Without the buffer set, we do not get any errors in tracing function. */
	curl_easy_setopt (ctx->handle, CURLOPT_ERRORBUFFER, ctx->error_buffer);

	/* Application (url) specific setups, like HTTP-specific, FTP-specific, etc.  */
	if (setup_handle_appl (ctx) == -1) {
		fprintf (stderr, "%s - error: setup_curl_handle_appl () failed .\n", __func__);
		return -1;
	}

	return 0;
}


static void 
dump(const char *text, FILE *stream, unsigned char *ptr, size_t size) {
	size_t i;
	size_t c;
	unsigned int width=0x10;

	fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
			text, (long)size, (long)size);

	for(i=0; i<size; i+= width) {
		fprintf(stream, "%4.4lx: ", (long)i);

		/* show hex to the left */
		for(c = 0; c < width; c++) {
			if(i+c < size)
				fprintf(stream, "%02x ", ptr[i+c]);
			else
				fputs("   ", stream);
		}

		/* show data on the right */
		for(c = 0; (c < width) && (i+c < size); c++) {
			char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
			fputc(x, stream);
		}

		fputc('\n', stream); /* newline */
	}
}


static int
debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
	const char *text;
	(void)handle; /* prevent compiler warning */
	(void)userp;

	switch (type) {
		case CURLINFO_TEXT:
			fprintf(stderr, "== Info: %s", data);
		default: /* in case a new one is introduced to shock us */
			return 0;

		case CURLINFO_HEADER_OUT:
			text = "=> Send header";
			break;
		case CURLINFO_DATA_OUT:
			text = "=> Send data";
			break;
		case CURLINFO_SSL_DATA_OUT:
			text = "=> Send SSL data";
			break;
		case CURLINFO_HEADER_IN:
			text = "<= Recv header";
			break;
		case CURLINFO_DATA_IN:
			text = "<= Recv data";
			break;
		case CURLINFO_SSL_DATA_IN:
			text = "<= Recv SSL data";
			break;
	}

	dump(text, stderr, (unsigned char *)data, size);
	return 0;
}


/* The callback to libcurl to write all bytes to ptr. */
/*
static size_t 
writefunction( void *ptr, size_t size, size_t nmemb, void *stream) {
	fwrite (ptr, size, nmemb, stream);
	return(nmemb * size);
}
*/


/* The callback to libcurl to skip all body bytes of the fetched urls. */
static size_t 
do_nothing_write_func (void *ptr, size_t size, size_t nmemb, void *stream) {

	(void)ptr;
	(void)stream;

	/* Overwriting the default behavior to write body bytes to stdout and 
	   just skipping the body bytes without any output.  */
	return (size*nmemb);
}

/* vim: set ts=4 sw=4 et sts=4:  */
