#include "pending.h"
#include "mgos.h"
#include <stdio.h>

struct timestamped_vote *votes = NULL;
int num_pending_votes = 0;
int vote_array_size = 200;

inline void init_unsent_votes()
{
  votes = malloc(sizeof(*votes) * vote_array_size);
}
inline void grow_vote_array()
{
  int new_size = vote_array_size * 2;
  votes = realloc(votes, sizeof(*votes) * new_size);
  vote_array_size = new_size;
}

void add_vote(enum VoteType vote, uint32_t timestamp)
{
  if (votes == NULL)
  {
    init_unsent_votes();
  }
  votes[num_pending_votes].timestamp = timestamp;
  votes[num_pending_votes].vote = vote;
  num_pending_votes++;

  if (num_pending_votes >= vote_array_size)
  {
    grow_vote_array();
  }
}

void write_pending_votes()
{
  FILE *f = fopen(filename, "w+");
  fwrite(&num_pending_votes, sizeof(num_pending_votes), 1, f);
  fwrite(votes, sizeof(*votes), num_pending_votes, f);
  fclose(f);
}

void read_pending_votes()
{
  if (votes == NULL)
  {
    init_unsent_votes();
  }
  FILE *f = fopen(filename, "r");
  if (f)
  {
    LOG(LL_INFO, ("file exists"));
    fread(&num_pending_votes, sizeof(num_pending_votes), 1, f);
    while (num_pending_votes > vote_array_size)
    {
      grow_vote_array();
    }
    fread(votes, sizeof(*votes), num_pending_votes, f);
  }
  fclose(f);
}

void clear_pending_votes(int clear_until)
{
  int to_move = num_pending_votes - clear_until;
  struct timestamped_vote tmp_buf[to_move];
  memmove(tmp_buf, votes + clear_until, to_move * sizeof(struct timestamped_vote));
  memset(votes, sizeof(*votes), vote_array_size);
  memmove(votes, tmp_buf, to_move * sizeof(struct timestamped_vote));
  num_pending_votes = to_move;
}

bool unsent_votes()
{
  return num_pending_votes > 0;
}