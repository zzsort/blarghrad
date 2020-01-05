#pragma once

#define BREAK __debugbreak();

//#define ENABLE_VERIFICATION

#ifdef ENABLE_VERIFICATION

void InitVerification();
void CHK_ENABLE();
void CHK_DISABLE();

// ex cmd: ?? (double)*(int*)addr
bool CheckVal(int intval);

// ex cmd: df addr L1
// ex cmd: ?? *(float*)addr
bool CheckVal(float floatval);

// ex cmd: .echo str
bool CheckVal(const char* strval);

// ex cmd: .echo true
bool CheckVal(bool b);

// ex cmd: df addr L3
bool CheckVal(const vec3_t& v);

bool NextData(const char* name);

extern bool chk_on;
extern char rawline[1024];
extern int progress;

#define CHKVAL_IMPL(name,val) \
{ \
    if (chk_on) { \
        progress++; \
        printf("%.4x:argh:%s\n", progress, rawline); \
        if (!NextData(name) || !CheckVal(val)) { \
            BREAK \
        } \
    } \
}

#define CHKVAL
#define CHKVAL2 CHKVAL_IMPL


#else
#define CHKVAL
#define CHKVAL2
#define CHK_ENABLE()
#define CHK_DISABLE()
#endif  // ENABLE_VERIFICATION
