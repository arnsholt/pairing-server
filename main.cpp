#include "types.pb.h"

extern "C" int real_main(int, char **);

/* To interoperate properly between C and C++ main has to be compiled by the
 * C++ compiler (this handles static intialization and such). But since I want
 * to write my code in C, this is just a placeholder dispatching off to the C
 * code. */
int main(int argc, char **argv) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    return real_main(argc, argv);
}
