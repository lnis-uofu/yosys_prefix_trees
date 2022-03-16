#include <iostream>
#include <getopt.h>
#include "version.h"

int main(int argc, char **argv) {
    int arg_idx = 0;
    while(arg_idx!=-1)
    {
        static struct option long_options[] = 
        {
            {"version", no_argument,    0,  'v'},
            {0,         0,              0,  0}
        };
        arg_idx = getopt_long (argc, argv, "v",
                               long_options, NULL);
        switch(arg_idx)
        {
            case 'v':
              std::cout << "Version " << Version() << std::endl;
              break;

            default:
              break;
        }
    }
}
