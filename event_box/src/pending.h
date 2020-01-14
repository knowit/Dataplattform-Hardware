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

/**
 * Returns true if there are any votes that have not been sent yet.
 * */
bool unsent_votes();

/**
 * Adds a vote to the list of votes which are waiting to be sent.
 * */
void add_vote(enum VoteType vote, uint32_t timestamp);

/**
 * Removes votes from the pending list from index 0 and until (not including)
 * index clear_until
 * */
void clear_pending_votes(int clear_until);

/**
 * Reads any pending votes stored on the filesystem.
 * */
void read_pending_votes();

/**
 * Writes all pending votes to filesystem so that they are persisted in case of
 * a power loss.
 * */
void write_pending_votes();

#endif