#include <math.h>

#include "ttest.h"


bool tactyk_test__approximately_eq(double a, double b, double precision) {
    if ( a == b ) {
        return true;
    }
    else if (isnan(a)) {
        return isnan(b);    // For state transition tracking purposes (NAN == NAN) is TRUE
    }
    else if (isnan(b)) {
        return false;
    }
    else if (!isfinite(a)) {
        return false;
    }
    else if (!isfinite(b)) {
        return false;
    }
    else if (a == 0) {
        return fabs(b) < precision;
    }
    else if (b == 0) {
        return fabs(a) < precision;
    }
    //printf("pchk ... prec=%0.15f a=%f b=%f a/b=%0.15f 1-a/b=%0.15f\n", precision, a,b, a/b, 1.0-(a/b));
    return ( fabs(1.0-(a/b)) <= precision);
}

bool tactyk_test__approximately_eq__longdbl(long double a, long double b, long double precision) {
    if ( a == b ) {
        return true;
    }
    else if (isnan(a)) {
        return isnan(b);    // For state transition tracking purposes (NAN == NAN) is TRUE
    }
    else if (isnan(b)) {
        return false;
    }
    else if (!isfinite(a)) {
        return false;
    }
    else if (!isfinite(b)) {
        return false;
    }
    else if (a == 0) {
        return fabs(b) < precision;
    }
    else if (b == 0) {
        return fabs(a) < precision;
    }
    //printf("pchk ... prec=%0.15f a=%f b=%f a/b=%0.15f 1-a/b=%0.15f\n", precision, a,b, a/b, 1.0-(a/b));
    return ( fabs(1.0-(a/b)) <= precision);
}


uint64_t read_spec__binary_data(struct tactyk_dblock__DBlock **out, struct tactyk_dblock__DBlock *spec) {

    struct tactyk_dblock__DBlock *data_array = tactyk_dblock__new(0);
    *out = data_array;

    struct tactyk_dblock__DBlock *token = NULL;

    while (spec != NULL) {
        token = spec->token;
        if (tactyk_dblock__equals_c_string(token, "byte")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_byte(data_array, (uint8_t)val);
                token = token->next;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "word")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_word(data_array, (uint16_t)val);
                token = token->next;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "dword")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_dword(data_array, (uint32_t)val);
                token = token->next;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "qword")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_qword(data_array, (uint64_t)val);
                token = token->next;
            }
        }
        else {
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_qword(data_array, (uint64_t)val);
                token = token->next;
            }
        }
        spec = spec->next;
    }
    return TACTYK_TESTSTATE__PASS;

    parse_fail: {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, token);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "'%s' is not an integer", buf);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
}




void tactyk_test__mk_ccallarg_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->test = tactyk_test__TEST_CCALL_ARGUMENT;
    entry->offset = ofs;
}

void tactyk_test__mk_var_test(char *name, tactyk_test__VALUE_TESTER *tester) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->test = tester;
    entry->offset = 0;
}

void tactyk_test__mk_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->test = tactyk_test__TEST_REGISTER;
    entry->offset = ofs;
}
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->test = tactyk_test__TEST_XMM_REGISTER;
    entry->offset = ofs;
}
