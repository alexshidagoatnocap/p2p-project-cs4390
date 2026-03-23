#include "protocol.h"
#include <stdio.h>
#include <string.h>

static CommandStatus createTrackerHandler(const char *arg) {
  /* YOUR CODE BEGINS HERE */
  printf("Inside createTracker handler\n");
  return STATUS_FAIL;
}
static CommandStatus updateTrackerHandler(const char *arg) {
  /* YOUR CODE BEGINS HERE */
  printf("Inside updateTracker handler\n");
  return STATUS_FAIL;
}
static CommandStatus listHandler(const char *arg) {
  /* YOUR CODE BEGINS HERE */
  printf("Inside list handler\n");
  return STATUS_FAIL;
}
static CommandStatus getHandler(const char *arg) {
  /* YOUR CODE BEGINS HERE */
  printf("Inside get handler\n");
  return STATUS_FAIL;
}
static CommandStatus exitHandler(const char *line) {
  printf("Inside exit handler\n");
  return STATUS_EXIT;
}
static CommandStatus unknownHandler(const char *line) {
  printf("Inside unknown handler\n");
  return STATUS_FAIL;
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
  CommandInfo Info = {.Type = CMD_UNKNOWN, .Status = STATUS_FAIL};

  commandStr = strtok_r(line, " ", &beginArgs);
  printf("Parsed Command: %s\n", commandStr);
  if (commandStr == NULL) {
    printf("No command given.");
    return Info;
  }

  CommandType cmd = identifyCommand(commandStr);
  Info.Type = cmd;
  Info.Status = commands[cmd](beginArgs);

  return Info;
}
