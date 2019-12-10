#include "pending.h"
#include "mgos.h"

void write_pending(struct pending_events pending)
{
  FILE *f = fopen(filename, "w+");
  fwrite(&pending, sizeof(uint16_t), 3, f);
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