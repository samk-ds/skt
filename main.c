/* 
 *     main.c
 *
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <curl/curl.h>

#include "conf.h"
#include "url.h"
#include "run_context.h"

#define MAX_HEADER_LEN 50


int cmpfunc (const void * a, const void * b) {
    return ( *(long int*)a - *(long int*)b );
}

/* function to calculate the median of the array data */
float find_median(long int array[] , int n) {

    float median=0;

    /* if number of elements are even */
    if (n%2 == 0) {
        median = (array[(n-1)/2] + array[n/2])/2.0;
    } else {
        /* if number of elements are odd */
        median = array[n/2];
    }

    return median;
}

static void 
get_start_time_sorted(client_context *ctx, long int **st, client_stats *cs) 
{

    int count =0;

    for (count = 0 ; count < ctx->num_tries; count++) {
        *(*st + count) = cs[count].start_transfer_time;  
    }

    qsort(*st, ctx->num_tries, sizeof(long int), cmpfunc);
}

static void 
get_namelookup_time_sorted(client_context *ctx, long int **st, client_stats *cs) 
{

    int count =0;

    for (count = 0 ; count < ctx->num_tries; count++) {
        *(*st + count) = cs[count].namelookup_time;  
    }

    qsort(*st, ctx->num_tries, sizeof(long int), cmpfunc);
}

static void 
get_total_time_sorted(client_context *ctx, long int **st, client_stats *cs) 
{

    int count =0;

    for (count = 0 ; count < ctx->num_tries; count++) {
        *(*st + count) = cs[count].total_time;  
    }

    qsort(*st, ctx->num_tries, sizeof(long int), cmpfunc);
}

static void 
get_connect_time_sorted(client_context *ctx, long int **st, client_stats *cs) 
{

    int count =0;

    for (count = 0 ; count < ctx->num_tries; count++) {
        *(*st + count) = cs[count].connect_time;  
    }

    qsort(*st, ctx->num_tries, sizeof(long int), cmpfunc);
}

static void 
display_stats(client_context *ctx, client_stats *rt) {

    long int *temp;
    float m_c_time,m_t_time, m_nl_time,m_st_time;

    temp = (long int*) malloc( ctx->num_tries * sizeof (long int));

    get_connect_time_sorted(ctx, &temp, rt);
    m_c_time = find_median(temp, ctx->num_tries);
    memset(temp, 0, ctx->num_tries * sizeof(long int));

    get_total_time_sorted(ctx, &temp, rt);
    m_t_time = find_median(temp, ctx->num_tries);
    memset(temp, 0, ctx->num_tries * sizeof(long int));

    get_start_time_sorted(ctx, &temp, rt);
    m_st_time = find_median(temp, ctx->num_tries);
    memset(temp, 0, ctx->num_tries * sizeof(long int));

    get_namelookup_time_sorted(ctx, &temp, rt);
    m_nl_time = find_median(temp, ctx->num_tries);

    printf("Ip= %s; Response code = %ld;\n",ctx->st.server_ip, ctx->st.resp_code);
    printf("Median of: \n");
    printf("Total time = %06f secs; Connect time = %06f secs ; Start time = %06f secs; Name lookup time = %06f secs;\n",
             m_t_time/1000000, m_c_time/1000000, m_st_time/1000000, m_nl_time/100000);

    free(temp);
}


int main (int argc, char *argv []) {

    int config_param = -1;
    client_stats *rtime = NULL;
    client_context ctx;
    int ret = -1;

    memset(&ctx,0,sizeof(client_context));

    /* Parse the command line, and set the options */
    if (parse_command_line (argc, argv) == -1) {
        fprintf (stderr, 
                "%s - error: failed parsing of the command line.\n", __func__);
        return -1;
    }

    /* Parse the configuration file. Read the config params */
    if ((config_param = parse_config_file (config_file, &ctx)) < 0) {
        fprintf (stderr, "%s - error: parse_config_file () failed.\n", __func__);
        return -1;
    }

    if (setup_init (&ctx) == -1) {
        fprintf (stderr,"%s - error: init failed.\n",__func__);
        return -1;
    }

    int count = ctx.num_tries;

    rtime = (client_stats *) malloc( ctx.num_tries * sizeof (client_stats));

    if (!rtime) {
        fprintf (stderr,"%s - error: malloc failed.\n",__func__);
        return -1;
    }

    /* get the stats, for x number of runs */
    while (count) {

        if ((ret = get_stats_info(&ctx) !=0)) {
            fprintf (stderr,"%s - error: get stats info failed.\n",__func__);
            free(rtime);
            return -1;
        }

        rtime[ctx.current_run].total_time = ctx.st.total_time;
        rtime[ctx.current_run].namelookup_time = ctx.st.namelookup_time;
        rtime[ctx.current_run].connect_time = ctx.st.connect_time;
        rtime[ctx.current_run].start_transfer_time = ctx.st.start_transfer_time;
        count--; ctx.current_run++ ;
    }

    /* displays the results on screen */
    display_stats(&ctx, rtime);

    free(rtime);

    return 0;
}

/* vim: set ts=4 sw=4 et sts=4:  */
