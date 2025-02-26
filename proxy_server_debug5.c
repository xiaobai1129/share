#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <libwebsockets.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BACKEND_HOST "127.0.0.1"
#define BACKEND_PORT 8080
#define BUFFER_SIZE 4096

typedef struct {
    char buffer[BUFFER_SIZE];
    size_t len;
} session_data_t;

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    session_data_t *data = (session_data_t *)user;
    switch (reason) {
        case LWS_CALLBACK_RECEIVE: {
            if (data->len + len > BUFFER_SIZE - 1) {
                lwsl_err("Buffer overflow\n");
                return -1;
            }
            memcpy(data->buffer + data->len, in, len);
            data->len += len;
            data->buffer[data->len] = '\0';

            json_error_t error;
            json_t *root = json_loads(data->buffer, 0, &error);
            if (!root) {
                if (error.position == data->len) {
                    return 0;
                }
                fprintf(stderr, "JSON decode error: %s\n", error.text);
                return -1;
            }

            const char *command = json_string_value(json_object_get(root, "command"));
            json_t *key = json_object_get(root, "key");

            printf("Command: %s, Key: %s\n", command, json_dumps(key, 0));

            char tcp_message[BUFFER_SIZE];
            if (strcmp(command, "get") == 0 && json_is_array(key)) {
                snprintf(tcp_message, sizeof(tcp_message), "%s [", command);

                size_t index;
                json_t *key_item;
                json_array_foreach(key, index, key_item) {
                    const char *key_str = json_string_value(key_item);
                    if (index > 0) {
                        strncat(tcp_message, ", ", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    }
                    strncat(tcp_message, "'", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    strncat(tcp_message, key_str, sizeof(tcp_message) - strlen(tcp_message) - 1);
                    strncat(tcp_message, "'", sizeof(tcp_message) - strlen(tcp_message) - 1);
                }
                strncat(tcp_message, "]", sizeof(tcp_message) - strlen(tcp_message) - 1);
            } else if (strcmp(command, "set") == 0 && json_is_object(key)) {
                snprintf(tcp_message, sizeof(tcp_message), "%s {", command);

                const char *key_str;
                json_t *value;
                int first = 1;
                json_object_foreach(key, key_str, value) {
                    if (!first) {
                        strncat(tcp_message, ", ", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    }
                    first = 0;
                    strncat(tcp_message, "'", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    strncat(tcp_message, key_str, sizeof(tcp_message) - strlen(tcp_message) - 1);
                    strncat(tcp_message, "': ", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    if (json_is_string(value)) {
                        strncat(tcp_message, "'", sizeof(tcp_message) - strlen(tcp_message) - 1);
                        strncat(tcp_message, json_string_value(value), sizeof(tcp_message) - strlen(tcp_message) - 1);
                        strncat(tcp_message, "'", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    } else if (json_is_boolean(value)) {
                        strncat(tcp_message, json_is_true(value) ? "True" : "False", sizeof(tcp_message) - strlen(tcp_message) - 1);
                    } else {
                        strncat(tcp_message, json_dumps(value, 0), sizeof(tcp_message) - strlen(tcp_message) - 1);
                    }
                }
                strncat(tcp_message, "}", sizeof(tcp_message) - strlen(tcp_message) - 1);
            } else {
                fprintf(stderr, "Unsupported command or key type\n");
                json_decref(root);
                return -1;
            }

            printf("Sending to TCP server: %s\n", tcp_message);

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("Socket creation error");
                json_decref(root);
                return -1;
            }

            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(BACKEND_PORT);

            if (inet_pton(AF_INET, BACKEND_HOST, &serv_addr.sin_addr) <= 0) {
                perror("Invalid address or address not supported");
                close(sock);
                json_decref(root);
                return -1;
            }

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection failed");
                close(sock);
                json_decref(root);
                return -1;
            }

            send(sock, tcp_message, strlen(tcp_message), 0);
            char response[BUFFER_SIZE] = {0};
            int bytes_received = recv(sock, response, BUFFER_SIZE, 0);
            if (bytes_received < 0) {
                perror("Receive error");
                close(sock);
                json_decref(root);
                return -1;
            }
            response[bytes_received] = '\0';
            printf("Received from TCP server: %s\n", response);

            lws_write(wsi, (unsigned char *)response, strlen(response), LWS_WRITE_TEXT);

            close(sock);
            json_decref(root);

            data->len = 0; // reset buffer
            break;
        }
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "example-protocol",
        callback_websocket,
        sizeof(session_data_t),
        BUFFER_SIZE,
    },
    {NULL, NULL, 0, 0}
};

int main(void) {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = 8880;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "lws init failed\n");
        return -1;
    }

    printf("Starting WebSocket server on ws://localhost:8880\n");
    while (1) {
        lws_service(context, 1000);
    }

    lws_context_destroy(context);
    return 0;
}






{
    "tasks": [
        {
            "type": "cppbuild",
            "label": "C/C++: gcc build active file",
            "command": "/usr/bin/gcc",
            "args": [
                "-fdiagnostics-color=always",
                "-g",
                "${file}",
                "-o",
                "${fileDirname}/${fileBasenameNoExtension}",
                ""
            ],
            "options": {
                "cwd": "${fileDirname}"
            },
            "problemMatcher": [
                "$gcc"
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "detail": "Task generated by Debugger."
        }
    ],
    "version": "2.0.0"
}

