#ifndef PENDING_H
#define PENDING_H

#include <stdbool.h>
#include <inttypes.h>

struct pending_events
{
  uint16_t positive;
  uint16_t neutral;
  uint16_t negative;
  struct mgos_rlock_type *lock;
};

static const char *filename = "pending";

void write_pending(struct pending_events pending);
bool unsent_events(struct pending_events pending);
void read_pending(struct pending_events *pending);
void increase_unsent_events(uint16_t positive, uint16_t neutral, uint16_t negative, struct pending_events *pending);

#endif