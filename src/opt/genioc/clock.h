/* clock.h
 * Timing helpers.
 */
 
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

int64_t now_us();
double now_s();
double now_cpu_s();

void sleep_us(int us);
void sleep_s(double s);

#define TIMER_MODE_REAL 0
#define TIMER_MODE_CPU  1

#define TIMER_SAMPLE_LIMIT 16

struct timer {
  double v[TIMER_SAMPLE_LIMIT];
  int c;
  int mode;
};

// Initialize a timer to zero (or .mode=TIMER_MODE_CPU).
// Cleanup not required.

// Take another sample.
// We only keep so many, but will always preserve at least the first and last.
void timer_next(struct timer *timer);

// How much time between the first and last samples?
double timer_get_total(const struct timer *timer);

// How much time between (p-1) and (p)? Implicitly zero for (p==0).
double timer_get_step(const struct timer *timer,int p);

// How long from the beginning to sample (p)? Same as total for (p>=c-1).
double timer_get_time_to_step(const struct timer *timer,int p);

#endif
