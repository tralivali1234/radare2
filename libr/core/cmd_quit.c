/* radare - LGPL - Copyright 2009-2016 - pancake */

#include "r_core.h"

static const char *help_msg_q[] = {
	"Usage:",  "q[!][!] [retval]", "",
	"q","","quit program",
	"q!","","force quit (no questions)",
	"q!!","","force quit without saving history",
	"q"," 1","quit with return value 1",
	"q"," a-b","quit with return value a-b",
	"q[y/n][y/n]","","quit, chose to kill process, chose to save project ",
	NULL
};

static void cmd_quit_init(RCore *core) {
	DEFINE_CMD_DESCRIPTOR (core, q);
}

static int cmd_Quit(void *data, const char *input) {
	RCore *core = (RCore *)data;
	if (input[0] == '!') {
		r_config_set (core->config, "scr.histsave", "false");
	}
	if (IS_DIGIT (input[0]) || input[0] == ' ') {
		core->num->value = r_num_math (core->num, input);
	} else {
		core->num->value = -1;
	}
	return -2;
}

static int cmd_quit(void *data, const char *input) {
	RCore *core = (RCore *)data;
	if (input)
	switch (*input) {
	case '?':
		r_core_cmd_help (core, help_msg_q);
		break;
	case '!':
		return cmd_Quit (core, input);
	case '\0':
		core->num->value = 0LL;
		return -2;
	default:
		while (*input == ' ') {
			input++;
		}
		if (*input) {
			r_num_math (core->num, input);
		} else {
			core->num->value = 0LL;
		}

		if (*input == 'y') {
			core->num->value = 5;
		} else if (*input == 'n') {
			core->num->value = 1;
		}

		if (input[1] == 'y') {
			core->num->value += 10;	
		} else if (input[1] == 'n') {
			core->num->value += 2;	
		}
		//exit (*input?r_num_math (core->num, input+1):0);
		//if (core->http_up) return false; // cancel quit when http is running
		return -2;
	}
	return false;
}
