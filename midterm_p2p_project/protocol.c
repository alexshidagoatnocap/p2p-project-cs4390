#include "protocol.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Remove trailing CR/LF. */
static void trim_newline(char *s) {
    if (s == NULL) {
        return;
    }
    s[strcspn(s, "\r\n")] = '\0';
}

/* Accept optional < ... > around commands. */
static void strip_angle_brackets(char *s) {
    if (s == NULL) {
        return;
    }

    size_t len = strlen(s);
    if (len == 0) {
        return;
    }

    if (s[0] == '<') {
        memmove(s, s + 1, len);
        len--;
    }

    if (len > 0 && s[len - 1] == '>') {
        s[len - 1] = '\0';
    }
}

/* Convert string to uppercase in place. */
static void to_upper_str(char *s) {
    if (s == NULL) {
        return;
    }

    while (*s) {
        *s = (char)toupper((unsigned char)*s);
        s++;
    }
}

const char *commandTypeToString(CommandType type) {
    switch (type) {
        case CMD_CREATE_TRACKER: return "CMD_CREATE_TRACKER";
        case CMD_UPDATE_TRACKER: return "CMD_UPDATE_TRACKER";
        case CMD_LIST:           return "CMD_LIST";
        case CMD_GET:            return "CMD_GET";
        case CMD_EXIT:           return "CMD_EXIT";
        default:                 return "CMD_UNKNOWN";
    }
}

const char *commandStatusToString(CommandStatus status) {
    switch (status) {
        case STATUS_OK:         return "STATUS_OK";
        case STATUS_FAIL:       return "STATUS_FAIL";
        case STATUS_FILE_ERROR: return "STATUS_FILE_ERROR";
        case STATUS_EXIT:       return "STATUS_EXIT";
        default:                return "STATUS_FAIL";
    }
}

CommandInfo parseCommand(const char *line) {
    CommandInfo info;
    memset(&info, 0, sizeof(info));

    info.Type = CMD_UNKNOWN;
    info.Status = STATUS_FAIL;

    if (line == NULL) {
        return info;
    }

    strncpy(info.rawLine, line, sizeof(info.rawLine) - 1);
    info.rawLine[sizeof(info.rawLine) - 1] = '\0';

    char buffer[BUFFER_SIZE];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    trim_newline(buffer);
    strip_angle_brackets(buffer);

    char *saveptr = NULL;
    char *first = strtok_r(buffer, " ", &saveptr);
    if (first == NULL) {
        return info;
    }

    char first_upper[128];
    strncpy(first_upper, first, sizeof(first_upper) - 1);
    first_upper[sizeof(first_upper) - 1] = '\0';
    to_upper_str(first_upper);

    if (strcmp(first_upper, "REQ") == 0) {
        char *second = strtok_r(NULL, " ", &saveptr);
        if (second != NULL) {
            char second_upper[128];
            strncpy(second_upper, second, sizeof(second_upper) - 1);
            second_upper[sizeof(second_upper) - 1] = '\0';
            to_upper_str(second_upper);

            if (strcmp(second_upper, "LIST") == 0) {
                info.Type = CMD_LIST;
                info.Status = STATUS_OK;
                return info;
            }
        }
        return info;
    }

    if (strcmp(first_upper, "GET") == 0) {
        char *filename = strtok_r(NULL, " ", &saveptr);
        info.Type = CMD_GET;

        if (filename == NULL) {
            return info;
        }

        strncpy(info.filename, filename, sizeof(info.filename) - 1);
        info.filename[sizeof(info.filename) - 1] = '\0';
        info.Status = STATUS_OK;
        return info;
    }

    if (strcmp(first, "createtracker") == 0 || strcmp(first_upper, "CREATETRACKER") == 0) {
        char *filename    = strtok_r(NULL, " ", &saveptr);
        char *filesize    = strtok_r(NULL, " ", &saveptr);
        char *description = strtok_r(NULL, " ", &saveptr);
        char *md5         = strtok_r(NULL, " ", &saveptr);
        char *ip          = strtok_r(NULL, " ", &saveptr);
        char *port        = strtok_r(NULL, " ", &saveptr);

        info.Type = CMD_CREATE_TRACKER;

        if (!filename || !filesize || !description || !md5 || !ip || !port) {
            return info;
        }

        strncpy(info.filename, filename, sizeof(info.filename) - 1);
        info.filename[sizeof(info.filename) - 1] = '\0';
        info.filesize = (size_t)strtoull(filesize, NULL, 10);
        strncpy(info.description, description, sizeof(info.description) - 1);
        info.description[sizeof(info.description) - 1] = '\0';
        strncpy(info.md5Hash, md5, sizeof(info.md5Hash) - 1);
        info.md5Hash[sizeof(info.md5Hash) - 1] = '\0';
        strncpy(info.ip, ip, sizeof(info.ip) - 1);
        info.ip[sizeof(info.ip) - 1] = '\0';
        info.port = (uint16_t)strtoul(port, NULL, 10);
        info.Status = STATUS_OK;
        return info;
    }

    if (strcmp(first, "updatetracker") == 0 || strcmp(first_upper, "UPDATETRACKER") == 0) {
        char *filename = strtok_r(NULL, " ", &saveptr);
        char *start    = strtok_r(NULL, " ", &saveptr);
        char *end      = strtok_r(NULL, " ", &saveptr);
        char *ip       = strtok_r(NULL, " ", &saveptr);
        char *port     = strtok_r(NULL, " ", &saveptr);

        info.Type = CMD_UPDATE_TRACKER;

        if (!filename || !start || !end || !ip || !port) {
            return info;
        }

        strncpy(info.filename, filename, sizeof(info.filename) - 1);
        info.filename[sizeof(info.filename) - 1] = '\0';
        info.startByte = (uint32_t)strtoul(start, NULL, 10);
        info.endByte = (uint32_t)strtoul(end, NULL, 10);
        strncpy(info.ip, ip, sizeof(info.ip) - 1);
        info.ip[sizeof(info.ip) - 1] = '\0';
        info.port = (uint16_t)strtoul(port, NULL, 10);
        info.Status = STATUS_OK;
        return info;
    }

    if (strcmp(first, "exit") == 0 || strcmp(first_upper, "EXIT") == 0) {
        info.Type = CMD_EXIT;
        info.Status = STATUS_EXIT;
        return info;
    }

    return info;
}
