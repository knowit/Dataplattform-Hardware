#include "pending.h"
#include "mgos.h"
#include <stdio.h>

void write_pending(struct pending_events pending)
{
  FILE *f = fopen(filename, "w+");
  fwrite(&pending, sizeof(uint16_t), 3, f);
  fputs(pending.event_id, f);
  fclose(f);
}

bool unsent_events(struct pending_events pending)
{
  return (pending.positive || pending.negative || pending.negative);
}

void read_pending(struct pending_events *pending)
{
  FILE *f = fopen(filename, "r");
  if (f)
  {
    LOG(LL_INFO, ("file exists"));
    fread(pending, sizeof(uint16_t), 3, f);
    fgets(pending->event_id, sizeof(pending->event_id), f);
  }
  fclose(f);
}

void increase_unsent_events(uint16_t positive, uint16_t neutral, uint16_t negative, struct pending_events *pending)
{
  mgos_rlock(pending->lock);
  pending->positive += positive;
  pending->neutral += neutral;
  pending->negative += negative;
  write_pending(*pending);
  mgos_runlock(pending->lock);
}

struct pending_events *init_pending_struct(uint16_t positive, uint16_t neutral,
                                           uint16_t negative, const char *event_id)
{
  struct pending_events *the_struct = malloc(sizeof(*the_struct) + sizeof(char[strlen(event_id)]));
  the_struct->positive = positive;
  the_struct->neutral = neutral;
  the_struct->negative = negative;
  the_struct->lock = mgos_rlock_create();
  return the_struct;
}

void free_pending_struct(struct pending_events *to_free)
{
  mgos_rlock_destroy(to_free->lock);
  free(to_free);
}