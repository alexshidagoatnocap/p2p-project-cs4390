#include "protocol.h"
#include <stdio.h>
#include <string.h>

static CommandOutput createTrackerHandler(const char *arg) {
  /*
   * Parse the args string based on the requirements in the PDF, populate a
   * TrackerInfo struct, and store it in trackerArray_g with trackerId being
   * the index of the array.
   *
   * ! USE A MUTEX WHEN UPDATING THE GLOBAL TRACKER ARRAY !
   *
   * For the trackerId, increment an ! ATOMIC ! counter that lasts for as long
   * as the tracker server is running.
   *
   * Prefer to use threads.h over POSIX threads.
   */

  /* YOUR CODE BEGINS HERE */
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  printf("Inside createTracker handler\n");
  return result;
}
static CommandOutput updateTrackerHandler(const char *arg) {
  /*
   * Parse the args string based on the requirements in the PDF, update the
   * contents of the TrackerInfo struct in the trackerArray_g array based
   * on its trackerId element.
   *
   * ! USE A MUTEX WHEN UPDATING THE GLOBAL TRACKER ARRAY !
   *
   * Prefer to use threads.h over POSIX threads.
   */

  /* YOUR CODE BEGINS HERE */
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  printf("Inside updateTracker handler\n");
  return result;
}
static CommandOutput listHandler(const char *arg) {
  (void)arg;
  /*
   * Print out the elements of the global trackerArray_g based on the format
   * required in the PDF. Also print out the trackerId so the peer user can
   * request a trackerfile based on id instead of name.
   *
   * ! USE A MUTEX WHEN ACCESSING THE GLOBAL TRACKER ARRAY !
   *
   * Prefer to use threads.h over POSIX threads.
   */

  /* YOUR CODE BEGINS HERE */
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  printf("Inside list handler\n");
  return result;
}
static CommandOutput getHandler(const char *arg) {
  /*
   * Request a tracker file from trackerArray_g, you can either request by
   * filename or trackerId.
   *
   * ! USE A MUTEX WHEN ACCESSING THE GLOBAL TRACKER ARRAY !
   *
   * Prefer to use threads.h over POSIX threads.
   */
  /* YOUR CODE BEGINS HERE */
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  printf("Inside get handler\n");
  return result;
}
static CommandOutput exitHandler(const char *line) {
  printf("Peer Exited!");
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  return result;
}
static CommandOutput unknownHandler(const char *line) {
  printf("Unknown Command!\n");
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  return result;
}

static CommandType identifyCommand(char *command) {
  if (strcmp(command, "createtracker") == 0) {
    printf("return CMD_CREATE_TRACKER\n");
    return CMD_CREATE_TRACKER;
  } else if (strcmp(command, "updatetracker") == 0) {
    printf("return CMD_UPDATE_TRACKER\n");
    return CMD_UPDATE_TRACKER;
  } else if (strcmp(command, "list") == 0) {
    printf("return CMD_LIST\n");
    return CMD_LIST;
  } else if (strcmp(command, "get") == 0) {
    printf("return CMD_GET\n");
    return CMD_GET;
  } else if (strcmp(command, "exit") == 0) {
    printf("return CMD_EXIT\n");
    return CMD_EXIT;
  }
  printf("return CMD_UNKNOWN\n");
  return CMD_UNKNOWN;
}

CommandHandler commands[] = {unknownHandler,       createTrackerHandler,
                             updateTrackerHandler, listHandler,
                             getHandler,           exitHandler};

CommandInfo parseCommand(char *line) {
  char *commandStr = NULL;
  char *beginArgs = NULL;
  CommandInfo Info = {.Type = CMD_UNKNOWN,
                      .Output.Status = STATUS_FAIL,
                      .Output.TrackerPtr = NULL};

  commandStr = strtok_r(line, " ", &beginArgs);
  printf("Parsed Command: %s\n", commandStr);
  if (commandStr == NULL) {
    printf("No command given.");
    return Info;
  }

  CommandType cmd = identifyCommand(commandStr);
  CommandOutput out = commands[cmd](beginArgs);
  Info.Type = cmd;
  Info.Output.Status = out.Status;
  if (cmd == CMD_GET) {
    Info.Output.TrackerPtr = out.TrackerPtr;
  }

  return Info;
}
