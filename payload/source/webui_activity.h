#ifndef WEBUI_ACTIVITY_H
#define WEBUI_ACTIVITY_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "cJSON.h"
#include "codex_webui_types.h"

struct activity_item {
    char id[32];
    char name[128];
    char type[64];
    int order;
    char device_ids[16][32];
    int device_count;
    struct activity_step start_steps[MAX_ACTIVITY_STEPS];
    int start_count;
    struct activity_step stop_steps[MAX_ACTIVITY_STEPS];
    int stop_count;
};

struct activity_inventory {
    struct activity_item items[MAX_ACTIVITIES];
    int count;
    long max_id;
    char current_id[32];
    char current_name[128];
};

void activity_cache_invalidate(void);
int activity_is_transitioning(char *target_out, size_t target_len);
void broadcast_activity_progress(const char *activity_id, int status, int step, int total, const char *desc);
void activity_get_transition_step(int *step, int *total, char *desc, size_t desc_len);
void activity_set_transitioning(pid_t pid, const char *target_id);
void activity_clear_transitioning(void);
void activity_startup_cleanup(void);
void activity_query_current(char *cur_id, size_t cur_id_len, char *raw_out, size_t raw_len);
void parse_activity_sequence_steps_cjson(const cJSON *seq_arr, struct activity_step *steps, int *step_count, int max_steps);
int load_activity_inventory(struct activity_inventory *inv);
int load_activities(struct activity_step ***activities_out, int **counts_out, char ***names_out, int *num_activities_out);
void free_activities(struct activity_step **activities, int *counts, char **names, int num_activities);
int activity_save(const char *id, const char *name, const char *type, int order, const char *device_ids_text, const char *start_steps_text, const char *stop_steps_text, char *reply, size_t replylen, char *saved_id, size_t saved_id_len);
int activity_delete(const char *id, char *reply, size_t replylen);
int validate_sequence_text(const char *seq_text, char *err, size_t errlen);
int activity_test_sequence(const char *steps_text, char *reply, size_t replylen);
void execute_activity_step_list(const struct activity_step *steps, int count);
void execute_device_power_action(struct ir_device *dev, int is_power_on);
int activity_run_start(const char *activity_id, char *out, size_t outlen);
void activity_run_transition(const char *activity_id);
int activity_run_stop(char *out, size_t outlen);

/* Renderers & Handlers */
void render_activities_json(int fd);
void render_activity_start_json(int fd, const struct request *req);
void render_activity_stop_json(int fd);
void render_activity_save_json(int fd, const struct request *req);
void render_activity_delete_json(int fd, const struct request *req);
void render_activity_test_sequence_json(int fd, const struct request *req);

void handle_activity_start(int fd, const struct request *req);
void handle_activity_stop(int fd, const struct request *req);
void handle_activity_save(int fd, const struct request *req);
void handle_activity_delete(int fd, const struct request *req);

#endif /* WEBUI_ACTIVITY_H */
