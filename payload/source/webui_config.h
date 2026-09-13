#ifndef WEBUI_CONFIG_H
#define WEBUI_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "codex_webui_types.h"

void load_mqtt(struct mqtt_config *cfg);
int save_mqtt(const struct mqtt_config *cfg);
void parse_wpa_quoted(const char *raw, const char *key, char *out, size_t outlen);
void load_wifi(struct wifi_config *cfg);
void wpa_write_quoted(FILE *f, const char *s);
int save_wifi(const struct wifi_config *cfg);
int load_hub_id(char *hub_id, size_t hub_id_len);
void trigger_mqtt_discover(void);
void request_resource_reload(void);
void backup_resources(void);
void backup_settings(void);
int tcp_established(const char *host, int port);
void send_bundle_download(int fd);

/* Form Handlers */
void handle_mqtt(int fd, const struct request *req);
void handle_wifi(int fd, const struct request *req);
void handle_system(int fd, const struct request *req);
void handle_import(int fd, const struct request *req);
void render_import_validate_json(int fd, const struct request *req);

#endif /* WEBUI_CONFIG_H */
