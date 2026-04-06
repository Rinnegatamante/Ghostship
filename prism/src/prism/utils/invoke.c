#include "invoke.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-non-prototype"
#endif

uintptr_t invoke(uintptr_t (*native_code)(), uintptr_t* args, size_t length) {
    switch (length) {
        case 0:
            return native_code();
        case 1:
            return native_code(args[0]);
        case 2:
            return native_code(args[0], args[1]);
        case 3:
            return native_code(args[0], args[1], args[2]);
        case 4:
            return native_code(args[0], args[1], args[2], args[3]);
        case 5:
            return native_code(args[0], args[1], args[2], args[3], args[4]);
        case 6:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5]);
        case 7:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        case 8:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        case 9:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
        case 10:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8],
                               args[9]);
        case 11:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10]);
        case 12:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11]);
        case 13:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12]);
        case 14:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13]);
        case 15:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14]);
        case 16:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14], args[15]);
        case 17:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14], args[15], args[16]);
        case 18:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14], args[15], args[16], args[17]);
        case 19:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14], args[15], args[16], args[17],
                               args[18]);
        case 20:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14], args[15], args[16], args[17], args[18],
                               args[19]);
        case 21:
            return native_code(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                               args[10], args[11], args[12], args[13], args[14], args[15], args[16], args[17], args[18],
                               args[19], args[20]);
    }

    return (uintptr_t) NULL;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif