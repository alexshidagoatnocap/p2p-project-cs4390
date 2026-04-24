// Pure string parsing layer for commands
#include "protocol.h"
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============ UTILITY FUNCTIONS (UNCHANGED) ============

static void trimWhitespaceInPlace(char *str) {
  if (str == NULL || *str == '\0') {
    return;
  }

  char *start = str;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }

  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }

  size_t len = strlen(str);
  while (len > 0 && isspace((unsigned char)str[len - 1])) {
    str[len - 1] = '\0';
    len--;
  }
}

static void stripOptionalAngleBrackets(char *str) {
  if (str == NULL) {
    return;
  }

  trimWhitespaceInPlace(str);
  size_t len = strlen(str);

  if (len > 0 && str[0] == '<') {
    memmove(str, str + 1, len);
  }

  len = strlen(str);
  if (len > 0 && str[len - 1] == '>') {
    str[len - 1] = '\0';
  }

  trimWhitespaceInPlace(str);
}

static int stringsEqualIgnoreCase(const char *a, const char *b) {
  if (a == NULL || b == NULL) {
    return 0;
  }

  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    a++;
    b++;
  }

  return (*a == '\0' && *b == '\0');
}

static int isNumberString(const char *s) {
  if (s == NULL || *s == '\0') {
    return 0;
  }

  while (*s) {
    if (!isdigit((unsigned char)*s)) {
      return 0;
    }
    s++;
  }
  return 1;
}

static void safeStringCopy(char *dest, size_t destSize, const char *src) {
  if (dest == NULL || destSize == 0) {
    return;
  }

  if (src == NULL) {
    dest[0] = '\0';
    return;
  }

  snprintf(dest, destSize, "%s", src);
}

static void removeTrackSuffix(const char *input, char *output,
                              size_t outputSize) {
  if (output == NULL || outputSize == 0) {
    return;
  }

  output[0] = '\0';

  if (input == NULL) {
    return;
  }

  safeStringCopy(output, outputSize, input);

  size_t len = strlen(output);
  if (len >= 6) {
    const char *suffix = ".track";
    size_t suffixLen = strlen(suffix);
    if (len >= suffixLen && strcmp(output + len - suffixLen, suffix) == 0) {
      output[len - suffixLen] = '\0';
    }
  }

  len = strlen(output);
  if (len >= 4) {
    const char *suffix = ".trk";
    size_t suffixLen = strlen(suffix);
    if (len >= suffixLen && strcmp(output + len - suffixLen, suffix) == 0) {
      output[len - suffixLen] = '\0';
    }
  }
}

static size_t splitTokens(char *buffer, char *tokens[], size_t maxTokens) {
  size_t count = 0;
  char *savePtr = NULL;
  char *tok = strtok_r(buffer, " \t\r\n", &savePtr);

  while (tok != NULL && count < maxTokens) {
    tokens[count++] = tok;
    tok = strtok_r(NULL, " \t\r\n", &savePtr);
  }

  return count;
}

// ============ COMMAND ARGUMENT PARSING ============

static int parseCreateTrackerArgs(const char *argStr, CreateTrackerArgs *args) {
  if (argStr == NULL || args == NULL) {
    return 0;
  }

  // Make a copy since we'll use strtok_r on it
  char buffer[BUFFER_SIZE];
  safeStringCopy(buffer, sizeof(buffer), argStr);
  trimWhitespaceInPlace(buffer);

  char *tokens[64];
  size_t tokenCount = splitTokens(buffer, tokens, 64);

  // Need at least: filename, filesize, md5, ip, port = 5
  // Can have description in between (at least 0 words)
  if (tokenCount < 5) {
    return 0;
  }

  const char *filename = tokens[0];
  const char *filesizeStr = tokens[1];
  const char *md5 = tokens[tokenCount - 3];
  const char *ip = tokens[tokenCount - 2];
  const char *portStr = tokens[tokenCount - 1];

  if (!isNumberString(filesizeStr) || !isNumberString(portStr)) {
    return 0;
  }

  unsigned long parsedPort = strtoul(portStr, NULL, 10);
  if (parsedPort > 65535UL) {
    return 0;
  }

  // Build description from tokens 2 to tokenCount-3
  char description[256] = {0};
  size_t descPos = 0;
  for (size_t i = 2; i < tokenCount - 3; i++) {
    int written = snprintf(description + descPos, sizeof(description) - descPos,
                           "%s%s", (i == 2 ? "" : " "), tokens[i]);

    if (written < 0 || (size_t)written >= sizeof(description) - descPos) {
      return 0; // Description too long
    }
    descPos += (size_t)written;
  }

  // Populate the args struct
  safeStringCopy(args->filename, sizeof(args->filename), filename);
  safeStringCopy(args->description, sizeof(args->description), description);
  safeStringCopy(args->md5, sizeof(args->md5), md5);
  safeStringCopy(args->ip, sizeof(args->ip), ip);

  args->filesize = (size_t)strtoull(filesizeStr, NULL, 10);
  args->port = (uint16_t)parsedPort;

  return 1;
}

static int parseUpdateTrackerArgs(const char *argStr, UpdateTrackerArgs *args) {
  if (argStr == NULL || args == NULL) {
    return 0;
  }

  char buffer[BUFFER_SIZE];
  safeStringCopy(buffer, sizeof(buffer), argStr);
  trimWhitespaceInPlace(buffer);

  char *tokens[16];
  size_t tokenCount = splitTokens(buffer, tokens, 16);

  if (tokenCount != 5) {
    return 0;
  }

  const char *filename = tokens[0];
  const char *startStr = tokens[1];
  const char *endStr = tokens[2];
  const char *ip = tokens[3];
  const char *portStr = tokens[4];

  if (!isNumberString(startStr) || !isNumberString(endStr) ||
      !isNumberString(portStr)) {
    return 0;
  }

  size_t startByte = strtoull(startStr, NULL, 10);
  size_t endByte = strtoull(endStr, NULL, 10);
  unsigned long parsedPort = strtoul(portStr, NULL, 10);

  if (parsedPort > 65535UL || startByte > endByte) {
    return 0;
  }

  // Populate the args struct
  safeStringCopy(args->filename, sizeof(args->filename), filename);
  safeStringCopy(args->ip, sizeof(args->ip), ip);

  args->startByte = startByte;
  args->endByte = endByte;
  args->port = (uint16_t)parsedPort;

  return 1;
}

static int parseGetTrackerArgs(const char *argStr, GetTrackerArgs *args) {
  if (argStr == NULL || args == NULL) {
    return 0;
  }

  char buffer[BUFFER_SIZE];
  safeStringCopy(buffer, sizeof(buffer), argStr);
  trimWhitespaceInPlace(buffer);

  if (buffer[0] == '\0') {
    return 0;
  }

  safeStringCopy(args->query, sizeof(args->query), buffer);

  // Check if it's a numeric ID or filename
  if (isNumberString(buffer)) {
    args->isId = 1;
  } else {
    args->isId = 0;
    // Remove .track or .trk suffix if present
    char filename[256];
    removeTrackSuffix(buffer, filename, sizeof(filename));
    safeStringCopy(args->query, sizeof(args->query), filename);
  }

  return 1;
}

static int parseCommandArguments(CommandType type, const char *argStr,
                                 CommandArgs *args) {
  if (args == NULL) {
    return 0;
  }

  switch (type) {
  case CMD_CREATE_TRACKER:
    return parseCreateTrackerArgs(argStr, &args->createTracker);
  case CMD_UPDATE_TRACKER:
    return parseUpdateTrackerArgs(argStr, &args->updateTracker);
  case CMD_GET:
    return parseGetTrackerArgs(argStr, &args->getTracker);
  case CMD_LIST:
  case CMD_EXIT:
  case CMD_UNKNOWN:
  default:
    return 1; // These don't need arguments
  }
}

// ============ COMMAND IDENTIFICATION ============

CommandType identifyCommand(char *command) {
  if (command == NULL) {
    return CMD_UNKNOWN;
  }

  if (stringsEqualIgnoreCase(command, "createtracker")) {
    return CMD_CREATE_TRACKER;
  } else if (stringsEqualIgnoreCase(command, "updatetracker")) {
    return CMD_UPDATE_TRACKER;
  } else if (stringsEqualIgnoreCase(command, "list")) {
    return CMD_LIST;
  } else if (stringsEqualIgnoreCase(command, "get")) {
    return CMD_GET;
  } else if (stringsEqualIgnoreCase(command, "exit")) {
    return CMD_EXIT;
  }

  return CMD_UNKNOWN;
}

// ============ MAIN PARSING FUNCTION ============

ParsedCommand parseCommand(char *line) {
  ParsedCommand result = {
      .type = CMD_UNKNOWN,
      .parseSuccess = 0,
  };
  result.parseError[0] = '\0';

  if (line == NULL || line[0] == '\0') {
    snprintf(result.parseError, sizeof(result.parseError), "Empty input");
    return result;
  }

  char *savePtr = NULL;
  char *commandStr = strtok_r(line, " \t\r\n", &savePtr);

  if (commandStr == NULL) {
    snprintf(result.parseError, sizeof(result.parseError), "No command given");
    return result;
  }

  /*
   * Support PDF-style "REQ LIST"
   */
  if (stringsEqualIgnoreCase(commandStr, "REQ")) {
    char *subCommand = strtok_r(NULL, " \t\r\n", &savePtr);
    if (subCommand != NULL && stringsEqualIgnoreCase(subCommand, "LIST")) {
      result.type = CMD_LIST;
      result.parseSuccess = 1;
      return result;
    }

    snprintf(result.parseError, sizeof(result.parseError),
             "Unknown REQ command");
    return result;
  }

  result.type = identifyCommand(commandStr);

  if (result.type == CMD_UNKNOWN) {
    snprintf(result.parseError, sizeof(result.parseError), "Unknown command");
    return result;
  }

  // Parse arguments based on command type
  result.parseSuccess =
      parseCommandArguments(result.type, savePtr, &result.args);

  if (!result.parseSuccess) {
    char *cmdTypeStr = NULL;
    switch (result.type) {
    case CMD_GET:
      cmdTypeStr = "get";
      break;
    case CMD_CREATE_TRACKER:
      cmdTypeStr = "createtracker";
      break;
    case CMD_UPDATE_TRACKER:
      cmdTypeStr = "updatetracker";
      break;
    default:
      cmdTypeStr = "Command does not accept arguments";
      break;
    }
    snprintf(result.parseError, sizeof(result.parseError),
             "Invalid arguments for command: %s", cmdTypeStr);
  }

  return result;
}
