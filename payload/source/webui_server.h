#ifndef WEBUI_SERVER_H
#define WEBUI_SERVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int webui_server_init(int port);
void webui_server_close(int fd);
void webui_handle_client(int client);

/* Async IR capture select-loop integration */
int webui_capture_fd(void);
int webui_capture_is_active(void);
void webui_capture_tick(int is_readable);

#ifdef __cplusplus
}
#endif

#endif /* WEBUI_SERVER_H */
