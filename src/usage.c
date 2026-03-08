#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>

void printUsage(void) {
  struct rusage r;
  if (getrusage(RUSAGE_SELF, &r) < 0) {
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "%lds %d𝜇s (%s)\n", r.ru_utime.tv_sec, r.ru_utime.tv_usec,
         "user CPU time used");
  fprintf(stderr, "%lds %d𝜇s (%s)\n", r.ru_stime.tv_sec, r.ru_stime.tv_usec,
         "system CPU time used");
  fprintf(stderr, "%ld (%s)\n", r.ru_maxrss, "maximum resident set size");
  fprintf(stderr, "%ld (%s)\n", r.ru_ixrss, "integral shared memory size");
  fprintf(stderr, "%ld (%s)\n", r.ru_idrss, "integral unshared data size");
  fprintf(stderr, "%ld (%s)\n", r.ru_isrss, "integral unshared stack size");
  fprintf(stderr, "%ld (%s)\n", r.ru_minflt, "page reclaims (soft page faults)");
  fprintf(stderr, "%ld (%s)\n", r.ru_majflt, "page faults (hard page faults)");
  fprintf(stderr, "%ld (%s)\n", r.ru_nswap, "swaps");
  fprintf(stderr, "%ld (%s)\n", r.ru_inblock, "block input operations");
  fprintf(stderr, "%ld (%s)\n", r.ru_oublock, "block output operations");
  fprintf(stderr, "%ld (%s)\n", r.ru_msgsnd, "IPC messages sent");
  fprintf(stderr, "%ld (%s)\n", r.ru_msgrcv, "IPC messages received");
  fprintf(stderr, "%ld (%s)\n", r.ru_nsignals, "signals received");
  fprintf(stderr, "%ld (%s)\n", r.ru_nvcsw, "voluntary context switches");
  fprintf(stderr, "%ld (%s)\n", r.ru_nivcsw, "involuntary context switches");

  return;
}
