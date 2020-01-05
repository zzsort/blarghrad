#include "pch.h"
#include "verify.h"
#include "mathlib.h"


#ifdef ENABLE_VERIFICATION

#pragma warning(disable:4996)
FILE *f;
bool chk_on = true;
bool chk_ignore;
char rawline[1024];
int progress = 0;

void InitVerification() {
    f = fopen("dbglog.txt", "r");
    if (!f) {
        printf("failed to open dbglog.txt\n");
        exit(-1);
    }
}

void CHK_ENABLE() {
    chk_on = true;
}

void CHK_DISABLE() {
    chk_on = false;
}

bool NextLine() {
    bool result = fgets(rawline, _countof(rawline), f);
    if (!result) {
        printf("unexpected EOF\n");
        BREAK;
    }
    else {
        rawline[strcspn(rawline, "\r\n")] = 0;

        // skip windbg noise
        if (!strncmp(rawline, "*** ERROR: Symbol", 17)) {
            return NextLine();
        }
    }
    return result;
}

bool NextData(const char* name) {
    char data[8], dataname[100];
    while (NextLine()) {
        int c = sscanf(rawline, "%7s%99s", data, dataname);
        if (c >= 1 && !strcmp(data, "!data")) {
            printf("%.4x:argh:%s\n", progress, rawline);
            if (c == 2 && !strcmp(name, dataname))
                return NextLine();
            if (c == 1)
                printf("log data missing name");
            else 
                printf("data names do not match\n-this: %s\n-argh: %s\n", name, dataname);
            break;
        }
    }
    return false;
}

bool CheckVal(int intval) {
    char ignore[16];
    int val = 0;
    bool result = (2 == sscanf(rawline, "%15s %d", ignore, &val) && val == intval);
    if (!result) printf("int values do not match!\n-this: %d\n-argh: %d\n", intval, val);
    return result;
}

bool CheckVal(float floatval) {
    char ignore[16];
    float val = 0;

    bool result = (2 == sscanf(rawline, "%15s %f", ignore, &val) &&
        (abs(val - floatval) < EQUAL_EPSILON) &&
        ((val <= 0) == (floatval <= 0))); // check for sign difference
        // exact: val == floatval);

    if (!result)
        printf("float values do not match!\n-this: %f\n-argh: %f\n", floatval, val);
    return result;
}

bool CheckVal(const char* strval) {
    return !strcmp(rawline, strval);
}

bool CheckVal(bool b) {
    return !strcmp(rawline, b ? "true" : "false");
}

bool VectorCompareNOCHK(const vec3_t& v1, const vec3_t& v2)
{
    if (fabs(v1.x - v2.x) > 0.01 ||
        fabs(v1.y - v2.y) > 0.01 ||
        fabs(v1.z - v2.z) > 0.01)
        return false;

    // check sign difference
/*    if ((v1.x <= 0) != (v2.x <= 0) ||
        (v1.y <= 0) != (v2.y <= 0) ||
        (v1.z <= 0) != (v2.z <= 0))
        return false;*/

    return true;
}

bool CheckVal(const vec3_t& v) {
    char ignore[16];
    vec3_t v2;
    int c = sscanf(rawline, "%15s %f %f %f", ignore, &v2.x, &v2.y, &v2.z);
    bool result = (c == 4 && VectorCompareNOCHK(v, v2)); // v.x == v2.x && v.y == v2.y && v.z == v2.z);
    if (!result)
        printf("vectors do not match!\n this: %f %f %f\n argh: %f %f %f\n", v.x, v.y, v.z, v2.x, v2.y, v2.z);
    return result;
}

#endif  // ENABLE_VERIFICATION