/*
	luksrku - Tool to remotely unlock LUKS disks using TLS.
	Copyright (C) 2016-2016 Johannes Bauer

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
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


#include "log.h"
#include "openssl.h"
#include "util.h"
#include "msg.h"
#include "client.h"
#include "blacklist.h"
#include "keydb.h"
#include "uuid.h"
#include "udp.h"
#include "luks.h"

struct keyclient_t {
	const struct pgmopts_client_t *opts;
	keydb_t *keydb;
	bool volume_unlocked[MAX_VOLUMES_PER_HOST];
	unsigned char identifier[ASCII_UUID_BUFSIZE];
	double broadcast_start_time;
};

static int psk_client_callback(SSL *ssl, const EVP_MD *md, const unsigned char **id, size_t *idlen, SSL_SESSION **sessptr) {
	struct keyclient_t *key_client = (struct keyclient_t*)SSL_get_app_data(ssl);
	*id = key_client->identifier;
	*idlen = ASCII_UUID_CHARACTER_COUNT;

	return openssl_tls13_psk_establish_session(ssl, key_client->keydb->hosts[0].tls_psk, PSK_SIZE_BYTES, EVP_sha256(), sessptr);
}

static bool unlock_luks_volume(const volume_entry_t *volume, const struct msg_t *unlock_msg) {
	bool success = true;
	char luks_passphrase[LUKS_PASSPHRASE_TEXT_SIZE_BYTES];
	if (ascii_encode(luks_passphrase, sizeof(luks_passphrase), unlock_msg->luks_passphrase_raw, sizeof(unlock_msg->luks_passphrase_raw))) {
		bool allow_discards = volume->volume_flags & VOLUME_FLAG_ALLOW_DISCARDS;
		success = open_luks_device(volume->volume_uuid, volume->devmapper_name, luks_passphrase, strlen(luks_passphrase), allow_discards);
	} else {
		log_msg(LLVL_FATAL, "Failed to transcribe raw LUKS passphrase to text form.");
		success = false;
	}
	OPENSSL_cleanse(luks_passphrase, sizeof(luks_passphrase));
	return success;
}

static bool attempt_unlock_luks_volume(struct keyclient_t *keyclient, const struct msg_t *unlock_msg) {
	const host_entry_t *host = &keyclient->keydb->hosts[0];
	const volume_entry_t* volume = keydb_get_volume_by_uuid(host, unlock_msg->volume_uuid);
	char volume_uuid_str[ASCII_UUID_BUFSIZE];
	sprintf_uuid(volume_uuid_str, unlock_msg->volume_uuid);
	if (!volume) {
		log_msg(LLVL_WARNING, "Keyserver provided key for unlocking volume UUID %s, but this volume is not known on the client side.", volume_uuid_str);
		return false;
	}

	/* This is a valid volume which we need to unlock */
	int volume_index = keydb_get_volume_index(host, volume);
	if (volume_index != -1) {
		if (keyclient->opts->no_luks) {
			keyclient->volume_unlocked[volume_index] = true;
#ifdef DEBUG
			dump_hexline(stderr, "Raw key: ", unlock_msg->luks_passphrase_raw, LUKS_PASSPHRASE_RAW_SIZE_BYTES, false);
#endif
		} else {
			if (!keyclient->volume_unlocked[volume_index]) {
				bool success = unlock_luks_volume(volume, unlock_msg);
				keyclient->volume_unlocked[volume_index] = success;
				if (!success) {
					log_msg(LLVL_ERROR, "Unlocking of volume %s / %s failed with the server-provided passphrase.", volume->devmapper_name, volume_uuid_str);
					return false;
				}
			} else {
				log_msg(LLVL_WARNING, "Volume %s / %s already unlocked, not attemping to unlock again.", volume->devmapper_name, volume_uuid_str);
			}
		}
	} else {
		log_msg(LLVL_FATAL, "Error calculating volume offset for volume %p from base %p.", volume, host->volumes);
		return false;
	}

	return true;
}

static bool contact_keyserver_socket(struct keyclient_t *keyclient, int sd) {
	struct generic_tls_ctx_t gctx;
	if (!create_generic_tls_context(&gctx, false)) {
		log_msg(LLVL_FATAL, "Failed to create OpenSSL client context.");
		return false;
	}
	SSL_CTX_set_psk_use_session_callback(gctx.ctx, psk_client_callback);

	SSL *ssl = SSL_new(gctx.ctx);
	if (ssl) {
		SSL_set_fd(ssl, sd);
		SSL_set_app_data(ssl, keyclient);

		if (SSL_connect(ssl) == 1) {
			struct msg_t msg;
			while (true) {
				int bytes_read = SSL_read(ssl, &msg, sizeof(msg));
				if (bytes_read == 0) {
					/* Server closed the connection. */
					break;
				}
				if (bytes_read != sizeof(msg)) {
					log_openssl(LLVL_FATAL, "SSL_read returned %d bytes when we expected to read %d", bytes_read, sizeof(msg));
					break;
				}
				char uuid_str[ASCII_UUID_BUFSIZE];
				sprintf_uuid(uuid_str, msg.volume_uuid);
				log_msg(LLVL_TRACE, "Received LUKS key to unlock volume with UUID %s", uuid_str);
				if (attempt_unlock_luks_volume(keyclient, &msg)) {
					log_msg(LLVL_DEBUG, "Successfully unlocked volume with UUID %s", uuid_str);
				} else {
					log_msg(LLVL_ERROR, "Failed to unlocked volume with UUID %s", uuid_str);
				}
			}
			OPENSSL_cleanse(&msg, sizeof(msg));
		} else {
			log_openssl(LLVL_FATAL, "SSL_connect failed");
		}

	} else {
		log_openssl(LLVL_FATAL, "Cannot establish SSL context when trying to connect to server");
	}

	SSL_free(ssl);
	free_generic_tls_context(&gctx);
	return true;
}

static bool contact_keyserver_ipv4(struct keyclient_t *keyclient, struct sockaddr_in *sockaddr_in, unsigned int port) {
	sockaddr_in->sin_port = htons(port);

	int sd = socket(sockaddr_in->sin_family, SOCK_STREAM, 0);
	if (sd == -1) {
		log_libc(LLVL_ERROR, "Failed to create socket(3)");
		return false;
	}

	if (connect(sd, (struct sockaddr*)sockaddr_in, sizeof(struct sockaddr_in)) == -1) {
		log_libc(LLVL_ERROR, "Failed to connect(3) to %d.%d.%d.%d:%d", PRINTF_FORMAT_IP(sockaddr_in), port);
		close(sd);
		return false;
	}

	bool success = contact_keyserver_socket(keyclient, sd);

	shutdown(sd, SHUT_RDWR);
	close(sd);
	return success;
}

static bool contact_keyserver_hostname(struct keyclient_t *keyclient, const char *hostname) {
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *result;
	int resolve_result = getaddrinfo(hostname, NULL, &hints, &result);
	if (resolve_result) {
		log_msg(LLVL_ERROR, "Failed to resolve hostname %s using getaddrinfo(3): %s", hostname, gai_strerror(resolve_result));
		return false;
	}

	if (result->ai_addr->sa_family != AF_INET) {
		freeaddrinfo(result);
		log_msg(LLVL_ERROR, "getaddrinfo(3) returned non-IPv4 entry");
		return false;
	}

	struct sockaddr_in *sin_address = (struct sockaddr_in*)result->ai_addr;
	log_msg(LLVL_TRACE, "Resolved %s to %d.%d.%d.%d", hostname, PRINTF_FORMAT_IP(sin_address));

	bool success = contact_keyserver_ipv4(keyclient, sin_address, keyclient->opts->port);

	freeaddrinfo(result);
	return success;
}

static unsigned int locked_volume_count(struct keyclient_t *keyclient) {
	unsigned int count = 0;
	const unsigned int volume_count = keyclient->keydb->hosts[0].volume_count;
	for (unsigned int i = 0; i < volume_count; i++) {
		if (!keyclient->volume_unlocked[i]) {
			count++;
		}
	}
	return count;
}

static bool all_volumes_unlocked(struct keyclient_t *keyclient) {
	return locked_volume_count(keyclient) == 0;
}

static unsigned int determine_timeout(struct keyclient_t *keyclient) {
	unsigned int client_timeout_secs = 0;
	if (keyclient->opts->timeout_seconds) {
		/* Command line always has precedence */
		client_timeout_secs = keyclient->opts->timeout_seconds;
	} else {
		/* Alternatively, take the one in the configuration file */
		client_timeout_secs = keyclient->keydb->hosts[0].client_default_timeout_secs;
	}
	return client_timeout_secs;
}

static bool abort_searching_for_keyserver(struct keyclient_t *keyclient) {
	if (all_volumes_unlocked(keyclient)) {
		log_msg(LLVL_DEBUG, "All volumes unlocked successfully.");
		return true;
	}

	unsigned int client_timeout_secs = determine_timeout(keyclient);
	if (client_timeout_secs) {
		double time_passed = now() - keyclient->broadcast_start_time;
		if (time_passed >= client_timeout_secs) {
			log_msg(LLVL_WARNING, "Could not unlock all volumes after %u seconds, giving up. %d volumes still locked.", client_timeout_secs, locked_volume_count(keyclient));
			return true;
		}
	}

	return false;
}

static bool broadcast_for_keyserver(struct keyclient_t *keyclient) {
	{
		unsigned int client_timeout_secs = determine_timeout(keyclient);
		if (client_timeout_secs) {
			log_msg(LLVL_DEBUG, "Searching luksrku keyserver, will give up after %u seconds", client_timeout_secs);
		} else {
			log_msg(LLVL_DEBUG, "Searching luksrku keyserver, will not give up until all volumes unlocked");
		}
	}

	int sd = create_udp_socket(0, true, 1000);
	if (sd == -1) {
		return false;
	}


	keyclient->broadcast_start_time = now();
	struct udp_query_t query;
	memcpy(query.magic, UDP_MESSAGE_MAGIC, sizeof(query.magic));
	memcpy(query.host_uuid, keyclient->keydb->hosts[0].host_uuid, 16);
	while (true) {
		log_msg(LLVL_TRACE, "Broadcasting search for luksrku keyserver");
		send_udp_broadcast_message(sd, keyclient->opts->port, &query, sizeof(query));

		struct sockaddr_in src = {
			.sin_family = AF_INET,
			.sin_port = htons(keyclient->opts->port),
			.sin_addr.s_addr = htonl(INADDR_ANY),
		};
		struct udp_response_t response;
		if (wait_udp_response(sd, &response, &src)) {
			if (!is_ip_blacklisted(src.sin_addr.s_addr)) {
				log_msg(LLVL_INFO, "Keyserver found at %d.%d.%d.%d", PRINTF_FORMAT_IP(&src));
				blacklist_ip(src.sin_addr.s_addr, BLACKLIST_TIMEOUT_CLIENT);
				if (!contact_keyserver_ipv4(keyclient, &src, keyclient->opts->port)) {
					log_msg(LLVL_WARNING, "Keyserver announced at %d.%d.%d.%d, but connection to it failed.", PRINTF_FORMAT_IP(&src));
				}
			} else {
				log_msg(LLVL_DEBUG, "Potential keyserver at %d.%d.%d.%d ignored, blacklist in effect.", PRINTF_FORMAT_IP(&src));
			}
		}

		if (abort_searching_for_keyserver(keyclient)) {
			break;
		}
	}
	return true;
}

bool keyclient_start(const struct pgmopts_client_t *opts) {
	/* Load key database first */
	struct keyclient_t keyclient = {
		.opts = opts,
	};
	bool success = true;

	do {
		keyclient.keydb = keydb_read(opts->filename);
		if (!keyclient.keydb) {
			log_msg(LLVL_FATAL, "Failed to load key database: %s", opts->filename);
			success = false;
			break;
		}

		if (keyclient.keydb->server_database) {
			log_msg(LLVL_FATAL, "Not an exported key database: %s -- this database contains LUKS passphrases, refusing to work with it!", opts->filename);
			success = false;
			break;
		}

		if (keyclient.keydb->host_count != 1) {
			log_msg(LLVL_FATAL, "Host count %d in %s -- expected exactly one host entry for an exported database.", keyclient.keydb->host_count, opts->filename);
			success = false;
			break;
		}

		host_entry_t *host = &keyclient.keydb->hosts[0];
		if (host->volume_count == 0) {
			log_msg(LLVL_FATAL, "No volumes found in exported database %s.", opts->filename);
			success = false;
			break;
		}

		/* Determine which of these volumes are already unlocked */
		for (unsigned int i = 0; i < host->volume_count; i++) {
			keyclient.volume_unlocked[i] = is_luks_device_opened(host->volumes[i].devmapper_name);
		}
		if (all_volumes_unlocked(&keyclient)) {
			log_msg(LLVL_INFO, "All %u volumes are unlocked already, not contacting luksrku key server.", host->volume_count);
			break;
		} else {
			log_msg(LLVL_DEBUG, "%u of %u volumes are currently locked.", locked_volume_count(&keyclient), host->volume_count);
		}

		/* Transcribe the host UUID to ASCII so we only have to do this once */
		sprintf_uuid((char*)keyclient.identifier, host->host_uuid);

		if (opts->hostname) {
			if (!contact_keyserver_hostname(&keyclient, opts->hostname)) {
				log_msg(LLVL_ERROR, "Failed to contact key server: %s", opts->hostname);
				success = false;
				break;
			}
		} else {
			if (!broadcast_for_keyserver(&keyclient)) {
				log_msg(LLVL_ERROR, "Failed to find key server using UDP broadcast.");
				success = false;
				break;
			}
		}
	} while (false);

	if (keyclient.keydb) {
		keydb_free(keyclient.keydb);
	}
	return success;
}
