How to run the program;

Run 'make', which will produce the executable 'samk'

All the config params except the input file is read from the command line

The config file takes all the configuration related params, as name value pairs.

Please see custom-headers.conf file to get an idea of how to write a config file.
Feed this config file as input param to the program

for e.g './samk -f custom-headers.conf'


The number of extra headers that can be appended is limited to 1024 in each run
This will have an impact on number of times the program can run, or the value that can be set 
in the conf file for "NUM_TRIES". As the program implicitly adds extra headers in each run just for this demo.

Please have a read in run_context.c@setup_handle_appl:179
 
I have used clang on my Arch linux machine, assuming it should work with gcc as well.

The program has a dependency on libcurl.

This is a show put up in a limited time.There are some rooms for improvement.
For example handling diffrent protocols available in the libcurl,
handling multiple connections simultaneously, handling  different http methods, proxy, authentication, encryption,
multi-part data, e.t.c. 

I have designed it in a way all these functionality can be easily extened with the current code base.
This code is not unit tested or function tested or stability tested, which could have been done with more time.

The makefile is minimum, it doesnt use the autoconf feature to test the avability of 
dependent library, because of limited time.

Thanks







