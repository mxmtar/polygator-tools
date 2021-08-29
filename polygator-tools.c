
#include <sys/stat.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "polygator-tools.h"

#include "x_sllist.h"
#include "x_timer.h"

enum {
	RADIO_MODULE_STATE_FAILED = -1,
	RADIO_MODULE_STATE_INIT = 0,
	RADIO_MODULE_STATE_CHECK_STATUS = 1,
	RADIO_MODULE_STATE_ENABLED = 2,
	RADIO_MODULE_STATE_HOLD_KEY = 3,
	RADIO_MODULE_STATE_CHECK_POWER_SUPPLY = 4,
	RADIO_MODULE_STATE_HOLD_POWER_SUPPLY = 5,
	RADIO_MODULE_STATE_DISABLED = 6,
};

struct board;
struct radio_channel {
	struct board *board;
	size_t position;
	char *module_type;
	int status;
	int state;
	size_t max_power_supply_delay;
	struct x_timer power_wait_timer;
	struct x_timer power_hold_timer;
	struct x_timer key_hold_timer;
	struct x_timer status_wait_timer;
	struct radio_channel *next;
};

struct board {
	char *driver;
	char *name;
	char *path;
	x_sllist_struct_declare(radio_channel_list, struct radio_channel);
	struct board *next;
};

int polygator_radio_channel_set_power_supply(const char *path, unsigned int position, unsigned int state)
{
	FILE *fp;
	int res = -1;

	if (path) {
		if ((fp = fopen(path, "w"))) {
			fprintf(fp, "channel[%u].power_supply(%d)", position, state);
			fclose(fp);
			res = 0;
		} else {
			errno = ENODEV;
		}
	} else {
		errno = ENODEV;
	}

	return res;
}

int polygator_radio_channel_get_power_supply(const char *path, unsigned int position)
{
	int res = -1;
	json_error_t err;
	json_t *board, *channel;
	const char *key;
	json_t *value;
	size_t index;

	if (path) {
		if ((board = json_load_file(path, 0, &err))) {
	    	json_object_foreach(board, key, value) {
				if (!strcmp(key, "channels") && json_is_array(value)) {
					json_array_foreach(value, index, channel) {
						if (index == position) {
							json_object_foreach(channel, key, value) {
								if (!strcmp(key, "power") && json_is_string(value)) {
									if (!strcmp("on", json_string_value(value))) {
										res = 1;
									} else {
										res = 0;
									}
									break;
								}
							}
							break;
						}
					}
					break;
				}
			}
			json_decref(board);
		} else {
			errno = ENODEV;
		}
	} else {
		errno = ENODEV;
	}

	return res;
}

int polygator_radio_channel_set_power_key(const char *path, unsigned int position, unsigned int state)
{
	FILE *fp;
	int res = -1;

	if (path) {
		if ((fp = fopen(path, "w"))) {
			fprintf(fp, "channel[%u].power_key(%d)", position, state);
			fclose(fp);
			res = 0;
		} else {
			errno = ENODEV;
		}
	} else {
		errno = ENODEV;
	}

	return res;
}

int polygator_radio_channel_get_power_key(const char *path, unsigned int position)
{
	int res = -1;
	json_error_t err;
	json_t *board, *channel;
	const char *key;
	json_t *value;
	size_t index;

	if (path) {
		if ((board = json_load_file(path, 0, &err))) {
	    	json_object_foreach(board, key, value) {
				if (!strcmp(key, "channels") && json_is_array(value)) {
					json_array_foreach(value, index, channel) {
						if (index == position) {
							json_object_foreach(channel, key, value) {
								if (!strcmp(key, "key") && json_is_string(value)) {
									if (!strcmp("on", json_string_value(value))) {
										res = 1;
									} else {
										res = 0;
									}
								}
							}
						}
					}
				}
			}
			json_decref(board);
		} else {
			errno = ENODEV;
		}
	} else {
		errno = ENODEV;
	}

	return res;
}

int polygator_radio_channel_get_status(const char *path, unsigned int position)
{
	int res = -1;
	json_error_t err;
	json_t *board, *channel;
	const char *key;
	json_t *value;
	size_t index;

	if (path) {
		if ((board = json_load_file(path, 0, &err))) {
	    	json_object_foreach(board, key, value) {
				if (!strcmp(key, "channels") && json_is_array(value)) {
					json_array_foreach(value, index, channel) {
						if (index == position) {
							json_object_foreach(channel, key, value) {
								if (!strcmp(key, "status") && json_is_string(value)) {
									if (!strcmp("on", json_string_value(value))) {
										res = 1;
									} else {
										res = 0;
									}
								}
							}
						}
					}
				}
			}
			json_decref(board);
		} else {
			errno = ENODEV;
		}
	} else {
		errno = ENODEV;
	}

	return res;
}

int polygator_radio_channel_power_up(struct radio_channel *channel)
{
	int res;

	switch (channel->state) {
		case RADIO_MODULE_STATE_INIT:
			x_timer_stop(channel->power_wait_timer);
			x_timer_stop(channel->power_hold_timer);
			x_timer_stop(channel->status_wait_timer);
			x_timer_stop(channel->key_hold_timer);
			res = polygator_radio_channel_set_power_key(channel->board->path, channel->position, 0);
			if (res == 0) {
				channel->state = RADIO_MODULE_STATE_CHECK_STATUS;
				res = 0;
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_CHECK_STATUS:
			res = polygator_radio_channel_get_status(channel->board->path, channel->position);
			if (res == 1) {
				if (is_x_timer_enable(channel->status_wait_timer)) {
					x_timer_stop(channel->status_wait_timer);
					x_timer_set_second(channel->key_hold_timer, 1);
					channel->state = RADIO_MODULE_STATE_HOLD_KEY;
					res = 0;
				} else {
					channel->state = RADIO_MODULE_STATE_ENABLED;
					res = 1;
				}
			} else if (res == 0) {
				if (is_x_timer_enable(channel->status_wait_timer)) {
					if (is_x_timer_fired(channel->status_wait_timer)) {
						x_timer_stop(channel->status_wait_timer);
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -2;
					}
				} else {
					channel->state = RADIO_MODULE_STATE_CHECK_POWER_SUPPLY;
					res = 0;
				}
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_CHECK_POWER_SUPPLY:
			res = polygator_radio_channel_get_power_supply(channel->board->path, channel->position);
			if (res == 0) {
				if (is_x_timer_enable(channel->power_wait_timer)) {
					if (is_x_timer_fired(channel->power_wait_timer)) {
						x_timer_stop(channel->power_wait_timer);
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -3;
					} else {
						res = 0;
					}
				} else {
					res = polygator_radio_channel_set_power_supply(channel->board->path, channel->position, 1);
					x_timer_set_second(channel->power_wait_timer, channel->max_power_supply_delay);
					if (res) {
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -1;
					}
				}
			} else if (res == 1) {
				x_timer_stop(channel->power_wait_timer);
				x_timer_set_second(channel->power_hold_timer, 1);
				channel->state = RADIO_MODULE_STATE_HOLD_POWER_SUPPLY;
				res = 0;
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_HOLD_POWER_SUPPLY:
			if (is_x_timer_enable(channel->power_hold_timer)) {
				if (is_x_timer_fired(channel->power_hold_timer)) {
					x_timer_stop(channel->power_hold_timer);
					res = polygator_radio_channel_set_power_key(channel->board->path, channel->position, 1);
					x_timer_set_second(channel->status_wait_timer, 8);
					if (res == 0) {
						channel->state = RADIO_MODULE_STATE_CHECK_STATUS;
						res = 0;
					} else {
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -1;
					}
				} else {
					res = 0;
				}
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_HOLD_KEY:
			if (is_x_timer_enable(channel->key_hold_timer)) {
				if (is_x_timer_fired(channel->key_hold_timer)) {
					res = polygator_radio_channel_set_power_key(channel->board->path, channel->position, 0);
					if (res == 0) {
						channel->state = RADIO_MODULE_STATE_ENABLED;
						res = 1;
					} else {
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -1;
					}
				} else {
					res = 0;
				}
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_ENABLED:
			res = 1;
			break;
		default:
			channel->state = RADIO_MODULE_STATE_FAILED;
			res = -1;
			break;
	}

	return res;
}

int polygator_radio_channel_power_down(struct radio_channel *channel)
{
	int res;

	switch (channel->state) {
		case RADIO_MODULE_STATE_INIT:
			x_timer_stop(channel->power_wait_timer);
			x_timer_stop(channel->power_hold_timer);
			x_timer_stop(channel->status_wait_timer);
			x_timer_stop(channel->key_hold_timer);
			res = polygator_radio_channel_set_power_key(channel->board->path, channel->position, 0);
			if (res == 0) {
				channel->state = RADIO_MODULE_STATE_CHECK_STATUS;
				res = 0;
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_CHECK_STATUS:
			res = polygator_radio_channel_get_status(channel->board->path, channel->position);
			if (res == 1) {
				if (is_x_timer_enable(channel->status_wait_timer)) {
					if (is_x_timer_fired(channel->status_wait_timer)) {
						x_timer_stop(channel->status_wait_timer);
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -2;
					} else {
						res = 0;
					}
				} else {
					res = polygator_radio_channel_set_power_key(channel->board->path, channel->position, 1);
					if (res == 0) {
						if (!strcmp(channel->module_type, "SIM5215")) {
							x_timer_set_ms(channel->key_hold_timer, 2000);
						} else if (!strcmp(channel->module_type, "M10")) {
							x_timer_set_ms(channel->key_hold_timer, 800);
						} else {
							x_timer_set_second(channel->key_hold_timer, 1);
						}
						channel->state = RADIO_MODULE_STATE_HOLD_KEY;
						res = 0;
					} else {
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -1;
					}
				}
			} else if (res == 0) {
				x_timer_stop(channel->status_wait_timer);
				res = polygator_radio_channel_set_power_supply(channel->board->path, channel->position, 0);
				if (res == 0) {
					channel->state = RADIO_MODULE_STATE_DISABLED;
					res = 1;
				} else {
					channel->state = RADIO_MODULE_STATE_FAILED;
					res = -1;
				}
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_HOLD_KEY:
			if (is_x_timer_enable(channel->key_hold_timer)) {
				if (is_x_timer_fired(channel->key_hold_timer)) {
					x_timer_stop(channel->key_hold_timer);
					res = polygator_radio_channel_set_power_key(channel->board->path, channel->position, 0);
					if (res == 0) {
						if (!strcmp(channel->module_type, "SIM5215")) {
							x_timer_set_second(channel->status_wait_timer, 5);
						} else if (!strcmp(channel->module_type, "M10")) {
							x_timer_set_second(channel->status_wait_timer, 12);
						} else {
							x_timer_set_second(channel->status_wait_timer, 8);
						}
						channel->state = RADIO_MODULE_STATE_CHECK_STATUS;
						res = 0;
					} else {
						channel->state = RADIO_MODULE_STATE_FAILED;
						res = -1;
					}
				} else {
					res = 0;
				}
			} else {
				channel->state = RADIO_MODULE_STATE_FAILED;
				res = -1;
			}
			break;
		case RADIO_MODULE_STATE_DISABLED:
			res = 1;
			break;
		default:
			channel->state = RADIO_MODULE_STATE_FAILED;
			res = -1;
			break;
	}

	return res;
}

void json_print_recursively(size_t level, const char *key, json_t *value)
{
	size_t i, e, i_index;
	const char *i_key;
	json_t *i_value;

	char pfx[64];

	for (i = 0, e = (level > 63) ? 63 : level; i < e; ++i) {
		pfx[i] = '\t';
	}
	pfx[i] = '\0';

	if (json_is_string(value)) {
		printf("%s%s: \"%s\"\n", pfx, key, json_string_value(value));
	} else if (json_is_integer(value)) {
		printf("%s%s: %ld\"\n", pfx, key, (long int)json_integer_value(value));
	} else if (json_is_array(value)) {
		if (strlen(key)) {
			printf("%s%s: [\n", pfx, key);
		} else {
			printf("%s[\n", pfx);
		}
		json_array_foreach(value, i_index, i_value) {
// 			printf("%lu: %s\n", (unsigned long int)i_index, json_is_object(i_value) ? "{" : "");
			json_print_recursively(level + 1, "", i_value);
// 			printf("%s\n", json_is_object(i_value) ? "}" : "");
		}
		printf("%s]\n", pfx);
	} else if (json_is_object(value)) {
		if (strlen(key)) {
			printf("%s%s: {\n", pfx, key);
		} else {
			printf("%s{\n", pfx);
		}
		json_object_foreach(value, i_key, i_value) {
			json_print_recursively(level + 1, i_key, i_value);
		}
		printf("%s}\n", pfx);
	}
}

int main (int argc, char **argv)
{
	int exit_status = EXIT_SUCCESS;
	char path[PATH_MAX];
	char *cp;
	json_error_t err;
	json_t *ss, *boards = NULL, *j_board, *j_channel, *ch_value;
	const char *key, *ch_key;
	json_t *value;
	size_t i, index;
	int run;
	int res;
	
	struct board *board;
	struct radio_channel *radio_channel;
	size_t total_channel_number;

	x_sllist_struct_declare(board_list, struct board);
	x_sllist_init(board_list);

	if ((ss = json_load_file(POLYGATOR_SUBSYSTEM_FILE_PATH, 0, &err))) {
	    json_object_foreach(ss, key, value) {
			if (!strcmp(key, "version") && json_is_string(value)) {
				printf("Polygator subsystem version=%s\n", json_string_value(value));
			}
			if (!strcmp(key, "boards") && json_is_array(value)) {
				boards = json_incref(value);
			}
		}
		json_decref(ss);
	} else {
		printf("json_load_file(%s) failed: %s\n", POLYGATOR_SUBSYSTEM_FILE_PATH, err.text);
		goto main_error;
	}

	// create board list
	json_array_foreach(boards, index, j_board) {
		if (!(board = calloc(1, sizeof(struct board)))) {
			printf("can't get memory for struct board\n");
			goto main_error;
		}
		// add board into board list
		x_sllist_insert_tail(board_list, board);
		json_object_foreach(j_board, key, value) {
			if (!strcmp(key, "driver") && json_is_string(value)) {
				board->driver = strdup(json_string_value(value));
			}
			if (!strcmp(key, "path") && json_is_string(value)) {
				cp =  strrchr(json_string_value(value), '!');
				board->name = strdup((cp) ? (cp + 1) : (json_string_value(value)));
				snprintf(path, PATH_MAX, "/dev/%s", json_string_value(value));
				for (i = 0; i < strlen(path); i++) {
					if (path[i] == '!') {
						path[i] = '/';
					}
				}
				board->path = strdup(path);
			}
		}
	}
	json_decref(boards);

	// get boards channels
	int j_fd;
	int j_rc;
	uint8_t j_buf[0x10000];
	total_channel_number = 0;
	for (board = board_list.head; board; board = board->next) {
//		if ((j_board = json_load_file(board->path, 0, &err))) {
                if ((-1 < (j_fd = open(board->path, O_RDONLY))) && (0 < (j_rc = read(j_fd, j_buf, sizeof(j_buf)))) && (j_board = json_loadb((const char *)j_buf, j_rc, 0, &err))) {
			json_object_foreach(j_board, key, value) {
				if (!strcmp(key, "channels") && json_is_array(value)) {
					json_array_foreach(value, index, j_channel) {
						if (!(radio_channel = calloc(1, sizeof(struct radio_channel)))) {
							printf("can't get memory for struct radio_channel\n");
							goto main_error;
						}
						// add channel into board channel list
						x_sllist_insert_tail(board->radio_channel_list, radio_channel);
						radio_channel->board = board;
						radio_channel->position = index;
						json_object_foreach(j_channel, ch_key, ch_value) {
							if (!strcmp(ch_key, "module") && json_is_string(ch_value)) {
								radio_channel->module_type = strdup(json_string_value(ch_value));
							}
						}
						++total_channel_number;
					}
					break;
				}
			}
			json_decref(j_board);
		} else {
			printf("json_load_file(%s) failed line=%d column=%d position=%d: %s\n", board->path, err.line, err.column, err.position, err.text);
			goto main_error;
		}
	}

	for (board = board_list.head; board; board = board->next) {
		for (radio_channel = board->radio_channel_list.head; radio_channel; radio_channel = radio_channel->next) {
			radio_channel->state = RADIO_MODULE_STATE_INIT;
			radio_channel->max_power_supply_delay = total_channel_number;
		}
	}

	// get subcommand
	if (argc >= 2) {
		if (!strcmp(argv[1], "enable")) {
			run = 1;
			while (run) {
				run = 0;
				for (board = board_list.head; board; board = board->next) {
					for (radio_channel = board->radio_channel_list.head; radio_channel; radio_channel = radio_channel->next) {
						res = polygator_radio_channel_power_up(radio_channel);
						if (res < 0) {
							if (radio_channel->status != res) {
								radio_channel->status = res;
								printf("%s-radio%lu power up failed - %d\n", board->name, (unsigned long int)radio_channel->position, res);
							}
						} else if (res == 0) {
							run = 1;
						} else {
							if (radio_channel->status != res) {
								radio_channel->status = res;
								printf("%s-radio%lu power up done\n", board->name, (unsigned long int)radio_channel->position);
							}
						}
					}
				}
				usleep(1000);
			}
		} if (!strcmp(argv[1], "disable")) {
			run = 1;
			while (run) {
				run = 0;
				for (board = board_list.head; board; board = board->next) {
					for (radio_channel = board->radio_channel_list.head; radio_channel; radio_channel = radio_channel->next) {
						res = polygator_radio_channel_power_down(radio_channel);
						if (res < 0) {
							if (radio_channel->status != res) {
								radio_channel->status = res;
								printf("%s-radio%lu power down failed - %d\n", board->name, (unsigned long int)radio_channel->position, res);
							}
						} else if (res == 0) {
							run = 1;
						} else {
							if (radio_channel->status != res) {
								radio_channel->status = res;
								printf("%s-radio%lu power down done\n", board->name, (unsigned long int)radio_channel->position);
							}
						}
					}
				}
				usleep(1000);
			}
		}
	}

	goto main_end;

main_error:
	exit_status = EXIT_FAILURE;

main_end:
	// destroy board list
	while ((board = x_sllist_remove_head(board_list))) {
		while ((radio_channel = x_sllist_remove_head(board->radio_channel_list))) {
			if (radio_channel->module_type) {
				free(radio_channel->module_type);
			}
			free(radio_channel);
		}
		if (board->driver) {
			free(board->driver);
		}
		if (board->name) {
			free(board->name);
		}
		if (board->path) {
			free(board->path);
		}
		free(board);
	}

	exit(exit_status);
}
