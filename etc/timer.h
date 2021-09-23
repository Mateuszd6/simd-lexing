#ifndef TIMER_H_
#define TIMER_H_

#include <time.h>
#ifdef __linux__
#  include <unistd.h>
#endif

typedef struct timer timer;
struct timer
{
    struct timespec start;
    struct timespec end;
    long elapsed;
};

static inline long
timer_diff(timer* t)
{
    struct timespec temp;
    if ((t->end.tv_nsec - t->start.tv_nsec) < 0)
    {
        temp.tv_sec = t->end.tv_sec - t->start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + t->end.tv_nsec - t->start.tv_nsec;
    }
    else
    {
        temp.tv_sec = t->end.tv_sec - t->start.tv_sec;
        temp.tv_nsec = t->end.tv_nsec - t->start.tv_nsec;
    }

    return (long)temp.tv_sec * 1000000000 + temp.tv_nsec;
}

#ifdef _MSC_VER
#define CLOCK_PROCESS_CPUTIME_ID 1
#include "windows.h"
int clock_gettime(int ignore, struct timespec *spec)
{  
    __int64 wintime; 
    (void) ignore;
    GetSystemTimeAsFileTime((FILETIME*)&wintime);
    wintime -=116444736000000000i64;
    spec->tv_sec = wintime / 10000000i64;
    spec->tv_nsec = wintime % 10000000i64 *100;
    return 0;
}
#endif /* _MSC_VER */

#define TIMER_INIT(TIMER) (TIMER).elapsed = 0

#define TIMER_START(TIMER)                                                     \
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &((TIMER).start))

#define TIMER_STOP(TIMER)                                                      \
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &((TIMER).end));                   \
    (TIMER).elapsed += timer_diff(&(TIMER))


#endif // TIMER_H_
