/* 
 *     url.h
 *
 */
#ifndef URL_H
#define URL_H

#define CUSTOM_HDRS_MAX_NUM 1024 

/* Application types of URLs.  */
typedef enum url_type_t {
	URL_UNDEF = 0, 
	URL_HTTP,
	URL_HTTPS,
	URL_FTP,
	URL_FTPS,
	URL_SFTP,
	URL_TELNET,
} url_type;


/* A structure, that contains all the knowledge about the url to fetch */
typedef struct url_context {

	/* URL buffer, string containing the url */
	char *url_str; 

	/* URL buffer length*/
	size_t url_str_len;

	/* Number of custom  HTTP headers */
	int custom_http_hdrs_num;

	/* The list of custom  HTTP headers */
	struct curl_slist *custom_http_hdrs;

	/* Request type/method used for http */
	size_t req_type;

	/* When true, an existing connection will be closed and connection
	   will be re-established */
	long fresh_connect; 

	/* Maximum time to establish TCP connection with a server (including resolving) */
	long connect_timeout;

	/* Logs headers of HTTP responses to files, when true. */
	int log_resp_headers;

	/* Logs bodies of HTTP responses to files, when true. */
	int log_resp_bodies;

	/* Application type of url, e.g. HTTP, HTTPS, FTP, etc */
	url_type urltype;

	/* Directory name to be used for logfiles of responses 
	   (headers and bodies).  */
	char* dir_log;

} url_context;

#endif
/* vim: set ts=4 sw=4 et sts=4:  */
