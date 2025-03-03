/*
	luksrku - Tool to remotely unlock LUKS disks using TLS.
	Copyright (C) 2016-2019 Johannes Bauer

	This file is part of luksrku.

	luksrku is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; this program is ONLY licensed under
	version 3 of the License, later versions are explicitly excluded.

	luksrku is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with luksrku; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	Johannes Bauer <JohannesBauer@gmx.de>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <openssl/crypto.h>
#include "editor.h"
#include "util.h"
#include "keydb.h"
#include "uuid.h"
#include "log.h"
#include "vaulted_keydb.h"

#define MAX_COMMAND_ALIAS_COUNT			2

enum cmd_returncode_t {
	COMMAND_SUCCESS,
	COMMAND_FAILURE,
	COMMAND_TOO_FEW_PARAMETERS,
	COMMAND_TOO_MANY_PARAMETERS,
};

struct editor_context_t {
	bool running;
	keydb_t *keydb;
	char filename[MAX_FILENAME_LENGTH];
	char passphrase[MAX_PASSPHRASE_LENGTH];
};

struct editor_command_t {
	unsigned int min_params;
	unsigned int max_params;
	const char *cmdnames[MAX_COMMAND_ALIAS_COUNT];
	const char *param_names;
	const char *description;
	enum cmd_returncode_t (*callback)(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
};

static enum cmd_returncode_t cmd_help(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_new(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_list(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_add_host(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_del_host(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_rekey_host(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_host_param(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_add_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_del_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_rekey_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_showkey_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_flag_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_open(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_save(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_export(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
#ifdef DEBUG
static enum cmd_returncode_t cmd_test(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
static enum cmd_returncode_t cmd_rawdump(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params);
#endif

static const struct editor_command_t commands[] = {
	{
		.cmdnames = { "help", "?" },
		.callback = cmd_help,
		.description = "Shows a help page describing all available commands",
	},
	{
		.cmdnames = { "new" },
		.callback = cmd_new,
		.description = "Create a new database file",
	},
	{
		.cmdnames = { "list", "l" },
		.callback = cmd_list,
		.description = "List contents of database file",
	},
	{
		.cmdnames = { "add_host" },
		.callback = cmd_add_host,
		.param_names = "[hostname]",
		.min_params = 1,
		.max_params = 1,
		.description = "Add a new host to the database file",
	},
	{
		.cmdnames = { "del_host" },
		.callback = cmd_del_host,
		.param_names = "[hostname]",
		.min_params = 1,
		.max_params = 1,
		.description = "Removes a host from the database file",
	},
	{
		.cmdnames = { "rekey_host" },
		.callback = cmd_rekey_host,
		.param_names = "[hostname]",
		.min_params = 1,
		.max_params = 1,
		.description = "Re-keys the TLS PSK of a given host",
	},
	{
		.cmdnames = { "host_param" },
		.callback = cmd_host_param,
		.param_names = "[hostname] timeout [value]",
		.min_params = 3,
		.max_params = 3,
		.description = "Set a parameter of a host (currently only timeout supported)",
	},
	{
		.cmdnames = { "add_volume" },
		.callback = cmd_add_volume,
		.param_names = "[hostname] [devmappername] [volume-UUID]",
		.min_params = 3,
		.max_params = 3,
		.description = "Add a new volume to the hostname",
	},
	{
		.cmdnames = { "del_volume" },
		.callback = cmd_del_volume,
		.param_names = "[hostname] [devmappername]",
		.min_params = 2,
		.max_params = 2,
		.description = "Removes a volume from the given host",
	},
	{
		.cmdnames = { "rekey_volume" },
		.callback = cmd_rekey_volume,
		.param_names = "[hostname] [devmappername]",
		.min_params = 2,
		.max_params = 2,
		.description = "Re-keys the LUKS passphrase of a volume of a given hostname",
	},
	{
		.cmdnames = { "showkey_volume" },
		.callback = cmd_showkey_volume,
		.param_names = "[hostname] [devmappername]",
		.min_params = 2,
		.max_params = 2,
		.description = "Shows the LUKS passphrase of a volume of a hostname",
	},
	{
		.cmdnames = { "flag_volume" },
		.callback = cmd_flag_volume,
		.param_names = "[hostname] [devmappername] [(+-)(allow_discards)]",
		.min_params = 3,
		.max_params = 3,
		.description = "Edits the flags of a volume",
	},
	{
		.cmdnames = { "open", "load" },
		.callback = cmd_open,
		.param_names = "[filename]",
		.min_params = 1,
		.max_params = 1,
		.description = "Opens a database file",
	},
	{
		.cmdnames = { "save" },
		.callback = cmd_save,
		.param_names = "([filename])",
		.min_params = 0,
		.max_params = 1,
		.description = "Saves a database file",
	},
	{
		.cmdnames = { "export" },
		.callback = cmd_export,
		.param_names = "[hostname] [filename]",
		.min_params = 2,
		.max_params = 2,
		.description = "Export a host database file for a specific host",
	},
#ifdef DEBUG
	{
		.cmdnames = { "test", "t" },
		.callback = cmd_test,
		.min_params = 1,
		.max_params = 1,
		.description = "Test different aspects of luksrku. Used only for debugging.",
	},
	{
		.cmdnames = { "rawdump", "raw" },
		.callback = cmd_rawdump,
		.min_params = 0,
		.max_params = 0,
		.description = "Dumps the raw representation of a file",
	},
#endif
	{ 0 }
};

static void format_command(char dest[static 128], const struct editor_command_t *cmd, const char *command_name) {
	const char *used_command_name = command_name ? command_name : cmd->cmdnames[0];
	snprintf(dest, 128, "%s%s%s", used_command_name, cmd->param_names ? " " : "", cmd->param_names ? cmd->param_names : "");
}

static enum cmd_returncode_t cmd_help(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	printf("List of commands:\n");
	const struct editor_command_t *cmd = commands;
	while (cmd->cmdnames[0]) {
		char formatted_cmd[128];
		format_command(formatted_cmd, cmd, NULL);
		if (strlen(formatted_cmd) <= 40) {
			printf("    %-40s %s\n", formatted_cmd, cmd->description);
		} else {
			printf("    %s\n    %-40s %s\n", formatted_cmd, "", cmd->description);
		}
		cmd++;
	}
	return COMMAND_SUCCESS;
}

static enum cmd_returncode_t cmd_new(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (ctx->keydb) {
		keydb_free(ctx->keydb);
	}
	ctx->keydb = keydb_new();
	memset(ctx->passphrase, 0, sizeof(ctx->passphrase));
	return (ctx->keydb != NULL) ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

static enum cmd_returncode_t cmd_list(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (!ctx->keydb) {
		printf("No key database loaded.\n");
		return COMMAND_FAILURE;
	}
	printf("Keydb version %d, %s database, %d hosts.\n", ctx->keydb->common.keydb_version, ctx->keydb->server_database ? "server" : "client", ctx->keydb->host_count);
	for (unsigned int i = 0; i < ctx->keydb->host_count; i++) {
		const host_entry_t *host = &ctx->keydb->hosts[i];
		char uuid[48];
		sprintf_uuid(uuid, host->host_uuid);
		printf("    Host %d: \"%s\" UUID %s -- %d volumes, ", i + 1, host->host_name, uuid, host->volume_count);
		if (!host->client_default_timeout_secs) {
			printf("no default timeout");
		} else {
			printf("default timeout %d secs", host->client_default_timeout_secs);
		}
		printf(":\n");
		for (unsigned int j = 0; j < host->volume_count; j++) {
			const volume_entry_t *volume = &host->volumes[j];
			sprintf_uuid(uuid, volume->volume_uuid);
			printf("        Volume %d: \"%s\" UUID %s   ", j + 1, volume->devmapper_name, uuid);
			if (volume->volume_flags == 0) {
				printf("defaults");
			} else {
				if (volume->volume_flags & VOLUME_FLAG_ALLOW_DISCARDS) {
					printf("allow_discards ");
				}
			}
			printf("\n");
		}
	}
	return COMMAND_SUCCESS;
}

static enum cmd_returncode_t cmd_add_host(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (!ctx->keydb) {
		ctx->keydb = keydb_new();
		if (!ctx->keydb) {
			return COMMAND_FAILURE;
		}
	}
	const char *host_name = params[0];
	bool success = keydb_add_host(&ctx->keydb, host_name);
	return success ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

static enum cmd_returncode_t cmd_del_host(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (!ctx->keydb) {
		fprintf(stderr, "No key database loaded.\n");
		return COMMAND_FAILURE;
	}

	const char *host_name = params[0];
	bool success = keydb_del_host_by_name(&ctx->keydb, host_name);
	return success ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

static host_entry_t* cmd_gethost(struct editor_context_t *ctx, const char *host_name) {
	if (!ctx->keydb) {
		fprintf(stderr, "No key database loaded.\n");
		return NULL;
	}
	host_entry_t *host = keydb_get_host_by_name(ctx->keydb, host_name);
	if (!host) {
		fprintf(stderr, "No such host: %s\n", host_name);
		return NULL;
	}
	return host;
}

static volume_entry_t* cmd_getvolume(struct editor_context_t *ctx, const char *host_name, const char *devmapper_name) {
	host_entry_t *host = cmd_gethost(ctx, host_name);
	if (!host) {
		return NULL;
	}

	volume_entry_t *volume = keydb_get_volume_by_name(host, devmapper_name);
	if (!volume) {
		fprintf(stderr, "No such volume \"%s\" for host \"%s\"\n", devmapper_name, host_name);
		return NULL;
	}
	return volume;
}

static enum cmd_returncode_t cmd_rekey_host(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	host_entry_t *host = cmd_gethost(ctx, host_name);
	return host && keydb_rekey_host(host) ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

static enum cmd_returncode_t cmd_host_param(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	const char *param = params[1];
	const char *value = params[2];
	host_entry_t *host = cmd_gethost(ctx, host_name);
	if (!host) {
		return COMMAND_FAILURE;
	}

	if (!strcasecmp(param, "timeout")) {
		host->client_default_timeout_secs = atoi(value);
	} else {
		fprintf(stderr, "Invalid parameter: %s\n", param);
		return COMMAND_FAILURE;
	}

	return COMMAND_SUCCESS;
}

static enum cmd_returncode_t cmd_do_showkey_volume(volume_entry_t *volume) {
	char luks_passphrase[LUKS_PASSPHRASE_TEXT_SIZE_BYTES];
	if (!keydb_get_volume_luks_passphrase(volume, luks_passphrase, sizeof(luks_passphrase))) {
		OPENSSL_cleanse(luks_passphrase, sizeof(luks_passphrase));
		fprintf(stderr, "Could not determine LUKS passphrase.\n");
		return COMMAND_FAILURE;
	}
	char uuid[ASCII_UUID_BUFSIZE];
	sprintf_uuid(uuid, volume->volume_uuid);
	printf("LUKS passphrase of %s / %s: %s\n", volume->devmapper_name, uuid, luks_passphrase);

	OPENSSL_cleanse(luks_passphrase, sizeof(luks_passphrase));
	return COMMAND_SUCCESS;
}

static enum cmd_returncode_t cmd_add_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	host_entry_t *host = cmd_gethost(ctx, host_name);
	if (!host) {
		return COMMAND_FAILURE;
	}

	const char *devmapper_name = params[1];
	const char *volume_uuid_str = params[2];
	if (!is_valid_uuid(volume_uuid_str)) {
		fprintf(stderr, "Not a valid UUID: %s\n", volume_uuid_str);
		return COMMAND_FAILURE;
	}
	uint8_t volume_uuid[16];
	parse_uuid(volume_uuid, volume_uuid_str);
	volume_entry_t *volume = keydb_add_volume(host, devmapper_name, volume_uuid);
	if (volume) {
		return cmd_do_showkey_volume(volume);
	} else {
		return COMMAND_FAILURE;
	}
}

static enum cmd_returncode_t cmd_del_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	const char *devmapper_name = params[1];

	host_entry_t *host = cmd_gethost(ctx, host_name);
	if (!host) {
		return COMMAND_FAILURE;
	}
	if (!keydb_del_volume(host, devmapper_name)) {
		return COMMAND_FAILURE;
	}
	return COMMAND_SUCCESS;
}

static enum cmd_returncode_t cmd_rekey_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	const char *devmapper_name = params[1];

	volume_entry_t *volume = cmd_getvolume(ctx, host_name, devmapper_name);
	if (!volume) {
		return COMMAND_FAILURE;
	}
	if (!keydb_rekey_volume(volume)) {
		return COMMAND_FAILURE;
	}
	return cmd_do_showkey_volume(volume);
}

static enum cmd_returncode_t cmd_showkey_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	const char *devmapper_name = params[1];

	volume_entry_t *volume = cmd_getvolume(ctx, host_name, devmapper_name);
	if (!volume) {
		return COMMAND_FAILURE;
	}

	return cmd_do_showkey_volume(volume);
}

static enum cmd_returncode_t cmd_flag_volume(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	const char *devmapper_name = params[1];
	const char *flag_str = params[2];

	volume_entry_t *volume = cmd_getvolume(ctx, host_name, devmapper_name);
	if (!volume) {
		return COMMAND_FAILURE;
	}

	if ((flag_str[0] != '+') && (flag_str[0] != '-')) {
		fprintf(stderr, "Flag string must start with '+' or '-' for adding or removing a flag.\n");
		return COMMAND_FAILURE;
	}

	unsigned int flag_value = 0;
	if (!strcasecmp(flag_str + 1, "allow_discards")) {
		flag_value = VOLUME_FLAG_ALLOW_DISCARDS;
	} else {
		fprintf(stderr, "Invalid flag '%s': allowed is only 'allow_discards'.\n", flag_str + 1);
		return COMMAND_FAILURE;
	}

	if (flag_str[0] == '+') {
		volume->volume_flags |= flag_value;
	} else {
		volume->volume_flags &= ~flag_value;
	}

	return COMMAND_SUCCESS;
}

static enum cmd_returncode_t cmd_open(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (ctx->keydb) {
		keydb_free(ctx->keydb);
	}
	const char *filename = params[0];
	ctx->keydb = keydb_read(params[0]);
	if (ctx->keydb) {
		strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);
		return COMMAND_SUCCESS;
	} else {
		return COMMAND_FAILURE;
	}
}

static enum cmd_returncode_t cmd_save(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (!ctx->keydb) {
		fprintf(stderr, "No key database loaded.\n");
		return COMMAND_FAILURE;
	}
	const char *filename = (param_cnt == 1) ? params[0] : ctx->filename;
	if (strlen(filename) == 0) {
		fprintf(stderr, "No filename given.\n");
		return COMMAND_FAILURE;
	}
	if (param_cnt == 1) {
		strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);
	}

	if (strlen(ctx->passphrase) == 0) {
		if (!query_passphrase("Database passphrase: ", ctx->passphrase, sizeof(ctx->passphrase))) {
			fprintf(stderr, "Failed to read passphrase.\n");
			return COMMAND_FAILURE;
		}
	}
	bool success = keydb_write(ctx->keydb, ctx->filename, ctx->passphrase);
	return success ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

static enum cmd_returncode_t cmd_export(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *host_name = params[0];
	const char *filename = params[1];
	host_entry_t *host = cmd_gethost(ctx, host_name);
	if (!host) {
		return COMMAND_FAILURE;
	}

	keydb_t *pubdb = keydb_export_public(host);
	char passphrase[MAX_PASSPHRASE_LENGTH];
	if (!query_passphrase("Client passphrase: ", passphrase, sizeof(passphrase))) {
		fprintf(stderr, "Failed to read export passphrase.\n");
		keydb_free(pubdb);
		return COMMAND_FAILURE;
	}
	if (!keydb_write(pubdb, filename, passphrase)) {
		fprintf(stderr, "Failed to write export passphrase.\n");
		keydb_free(pubdb);
		return COMMAND_FAILURE;
	}
	keydb_free(pubdb);
	return COMMAND_SUCCESS;
}

#ifdef DEBUG

static enum cmd_returncode_t cmd_test(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	const char *cmd = params[0];
	if (!strcmp(cmd, "vault")) {
		if (!ctx->keydb) {
			fprintf(stderr, "No keydb.\n");
			return COMMAND_FAILURE;
		}
		struct vaulted_keydb_t *vkdb = vaulted_keydb_new(ctx->keydb);
		fprintf(stderr, "Vault created at %p.\n", vkdb);
		fprintf(stderr, "TLS-PSK vault %u bytes:\n", vkdb->tls_psk_vault->data_length);
		dump_hex_long(stderr, vkdb->tls_psk_vault->data, vkdb->tls_psk_vault->data_length);
		fprintf(stderr, "LUKS vault %u bytes:\n", vkdb->luks_passphrase_vault->data_length);
		dump_hex_long(stderr, vkdb->luks_passphrase_vault->data, vkdb->luks_passphrase_vault->data_length);

		fprintf(stderr, "~~~~~~~~~~~~~~~~ decrypted TLS-PSK ~~~~~~~~~~~~~~~~\n");
		vault_open(vkdb->tls_psk_vault);
		dump_hex_long(stderr, vkdb->tls_psk_vault->data, vkdb->tls_psk_vault->data_length);
		fprintf(stderr, "~~~~~~~~~~~~~~~~ decrypted LUKS ~~~~~~~~~~~~~~~~\n");
		vault_open(vkdb->luks_passphrase_vault);
		dump_hex_long(stderr, vkdb->luks_passphrase_vault->data, vkdb->luks_passphrase_vault->data_length);
		vaulted_keydb_free(vkdb);
		return COMMAND_SUCCESS;
	}
	fprintf(stderr, "Test accepts one of these commands: vault\n");
	return COMMAND_FAILURE;
}

static enum cmd_returncode_t cmd_rawdump(struct editor_context_t *ctx, const char *cmdname, unsigned int param_cnt, char **params) {
	if (!ctx->keydb) {
		return COMMAND_SUCCESS;
	}
	fprintf(stderr, "Version %d, %s, %d hosts.\n", ctx->keydb->common.keydb_version, ctx->keydb->server_database ? "server" : "client", ctx->keydb->host_count);
	for (unsigned int i = 0; i < ctx->keydb->host_count; i++) {
		host_entry_t *host = &ctx->keydb->hosts[i];
		fprintf(stderr, "Host %d:\n", i);
		{
			char host_uuid[ASCII_UUID_BUFSIZE];
			sprintf_uuid(host_uuid, host->host_uuid);

			char hex_psk[(PSK_SIZE_BYTES * 2) + 1];
			sprintf_hex(hex_psk, host->tls_psk, PSK_SIZE_BYTES);
			fprintf(stderr, "openssl s_client -connect 127.0.0.1:23170 -psk %s -psk_identity %s -curves X448:X25519 -ciphersuites TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384 -tls1_3\n", hex_psk, host_uuid);
		}
		dump_hexline(stderr, "    host_uuid    ", host->host_uuid, sizeof(host->host_uuid), false);
		dump_hexline(stderr, "    host_name    ", host->host_name, sizeof(host->host_name), true);
		dump_hexline(stderr, "    tls_psk      ", host->tls_psk, sizeof(host->tls_psk), false);
		fprintf(stderr, "    volume_count %u\n", host->volume_count);
		for (unsigned int j = 0; j < MAX_VOLUMES_PER_HOST; j++) {
			volume_entry_t *volume = &host->volumes[j];
			if (!is_zero(volume, sizeof(volume_entry_t))) {
				fprintf(stderr, "    Host %d / Volume %d:\n", i, j);
				dump_hexline(stderr, "        volume_uuid     ", volume->volume_uuid, sizeof(volume->volume_uuid), false);
				dump_hexline(stderr, "        devmapper_name  ", volume->devmapper_name, sizeof(volume->devmapper_name), true);
				dump_hexline(stderr, "        luks_passphrase ", volume->luks_passphrase_raw, sizeof(volume->luks_passphrase_raw), false);
			}
		}
	}
	return COMMAND_SUCCESS;
}
#endif

static const struct editor_command_t *find_command(const char *command_name) {
	const struct editor_command_t *cmd = commands;
	while (cmd->cmdnames[0]) {
		for (unsigned int cmd_index = 0; cmd_index < MAX_COMMAND_ALIAS_COUNT; cmd_index++) {
			const char *cmdname = cmd->cmdnames[cmd_index];
			if (!cmdname) {
				break;
			}
			if (!strcasecmp(cmdname, command_name)) {
				return cmd;
			}
		}
		cmd++;
	}
	return NULL;
}

static enum cmd_returncode_t execute_command(const struct editor_command_t *cmd, struct editor_context_t *ctx, unsigned int token_count, char **tokens) {
	/* Found correct command */
	const unsigned int parameter_count = token_count - 1;
	if (parameter_count < cmd->min_params) {
		return COMMAND_TOO_FEW_PARAMETERS;
	} else if (parameter_count > cmd->max_params) {
		return COMMAND_TOO_MANY_PARAMETERS;
	} else {
		return cmd->callback(ctx, tokens[0], parameter_count, tokens + 1);
	}
}

bool editor_start(const struct pgmopts_edit_t *opts) {
	struct editor_context_t editor_context = {
		.running = true,
	};

	if (opts->filename) {
		char *filename = strdup(opts->filename);
		if (!filename) {
			log_libc(LLVL_ERROR, "Unable to strdup(3)");
			return false;
		}
		char *tokens[2] = {
			"open",
			filename,
		};
		enum cmd_returncode_t result = execute_command(find_command(tokens[0]), &editor_context, 2, tokens);
		free(filename);
		if (result != COMMAND_SUCCESS) {
			return false;
		}
	}

	while (editor_context.running) {
		char command_buffer[256];
		printf("> ");
		if (!fgets(command_buffer, sizeof(command_buffer) - 1, stdin)) {
			break;
		}
		if (!truncate_crlf(command_buffer)) {
			/* Incomplete read? */
			break;
		}

		const unsigned int max_token_count = 16;
		unsigned int token_count = 0;
		char *tokens[max_token_count];
		char *strtok_inptr = command_buffer;
		char *strtok_saveptr = NULL;
		while (token_count < max_token_count) {
			char *next_token = strtok_r(strtok_inptr, " \t", &strtok_saveptr);
			if (!next_token) {
				break;
			}
			tokens[token_count] = next_token;
			token_count++;
			strtok_inptr = NULL;
		}

		if (token_count == 0) {
			continue;
		}

		const char *command_name = tokens[0];
		const struct editor_command_t *command = find_command(command_name);
		if (!command) {
			printf("No such command: \"%s\" -- type \"help\" to get a list of valid commands\n", command_name);
			continue;
		}

		enum cmd_returncode_t returncode = execute_command(command, &editor_context, token_count, tokens);
		if (returncode == COMMAND_FAILURE) {
			printf("Execution failed: %s\n", command_name);
		} else if ((returncode == COMMAND_TOO_FEW_PARAMETERS) || (returncode == COMMAND_TOO_MANY_PARAMETERS)) {
			char formatted_cmd[128];
			format_command(formatted_cmd, command, command_name);
			if (command->min_params == command->max_params) {
				printf("Wrong number of parameters: \"%s\" requires %d parameters -- %s\n", command_name, command->min_params, formatted_cmd);
			} else if (returncode == COMMAND_TOO_FEW_PARAMETERS) {
				printf("Too few parameters: \"%s\" requires at least %d parameters -- %s\n", command_name, command->min_params, formatted_cmd);
			} else if (returncode == COMMAND_TOO_MANY_PARAMETERS) {
				printf("Too many parameters: \"%s\" requires at most %d parameters -- %s\n", command_name, command->max_params, formatted_cmd);
			}
		}
	}

	if (editor_context.keydb) {
		keydb_free(editor_context.keydb);
	}
	OPENSSL_cleanse(&editor_context, sizeof(editor_context));
	return true;
}
