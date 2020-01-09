#ifndef PENDING_H
#define PENDING_H

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

enum VoteType
{
  positive,
  neutral,
  negative
};

struct timestamped_vote
{
  enum VoteType vote;
  uint32_t timestamp;
};

static const char *filename = "pending";
extern struct timestamped_vote *votes;
extern int num_pending_votes;

bool unsent_votes();
void add_vote(enum VoteType vote, uint32_t timestamp);
void clear_pending_votes(int clear_until);
void read_pending_votes();
void write_pending_votes();

#endif