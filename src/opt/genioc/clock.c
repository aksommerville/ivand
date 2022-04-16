#include "clock.h"
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

/* Current time.
 */
 
int64_t now_us() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000+tv.tv_usec;
}

double now_s() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

double now_cpu_s() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

/* Sleep.
 */

void sleep_us(int us) {
  usleep(us);
}

void sleep_s(double s) {
  usleep((int)(s*1000000.0));
}

/* Structured timer.
 */
 
void timer_next(struct timer *timer) {
  double now;
  switch (timer->mode) {
    case TIMER_MODE_CPU: now=now_cpu_s(); break;
    default: now=now_s(); break;
  }
  if (timer->c>=TIMER_SAMPLE_LIMIT) {
    memmove(timer->v+1,timer->v+2,sizeof(double)*(timer->c-2));
    timer->c--;
  }
  timer->v[timer->c++]=now;
}

double timer_get_total(const struct timer *timer) {
  if (timer->c<2) return 0.0;
  return timer->v[timer->c-1]-timer->v[0];
}

double timer_get_step(const struct timer *timer,int p) {
  if (p>=timer->c) p=timer->c-1;
  if (p<1) return 0.0;
  return timer->v[p]-timer->v[p-1];
}

double timer_get_time_to_step(const struct timer *timer,int p) {
  if (p>=timer->c) p=timer->c-1;
  if (p<1) return 0.0;
  return timer->v[p]-timer->v[0];
}
