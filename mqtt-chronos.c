/*
 * Copyright (c) 2013 Jan-Piet Mens <jpmens()gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of mosquitto nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <signal.h>
#include <mosquitto.h>

#define DEFAULT_PREFIX	"system/%s/chronos"	/* Up to three %s are replaced by nodename */
#ifndef TRUE
# define TRUE (1)
#endif
#ifndef FALSE
# define FALSE (0)
#endif

time_t start_tics;
struct mosquitto *m = NULL;
int do_utc = TRUE, do_local = FALSE;

void catcher(int sig)
{
	fprintf(stderr, "Going down on signal %d\n", sig);

	if (m) {
		mosquitto_disconnect(m);
		mosquitto_loop_stop(m, false);
		mosquitto_lib_cleanup();
	}
	exit(1);
}

void pub(char *prefix ,char *zone, char *topic, char *str, int retain)
{
	char *fulltopic;
	int rc;
	
	fulltopic = malloc(strlen(prefix) + 
		((zone) ? strlen(zone) : 0) +
		strlen(topic) + 12);

	if (zone) {
		sprintf(fulltopic, "%s/%s/%s", prefix, zone, topic);
	} else {
		sprintf(fulltopic, "%s/%s", prefix, topic);
	}
	// printf("%s %s\n", fulltopic, str); fflush(stdout);
	rc = mosquitto_publish(m, NULL, fulltopic, strlen(str), str, 0, retain);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Cannot publish: %s\n", mosquitto_strerror(rc));
	}
	free(fulltopic);
}

void pub_i(char *prefix, char *zone, char *topic, int value, int retain)
{
	char buf[128];

	sprintf(buf, "%d", value);
	pub(prefix, zone, topic, buf, retain);
}

void pub_ftime(char *prefix, char *zone, char *topic, char *fmt, struct tm *tm, int retain)
{
	char tbuf[256];

	if (strftime(tbuf, sizeof(tbuf)-1, fmt, tm) != 0) {
		pub(prefix, zone, topic, tbuf, retain);
	}
}

void slices(char *prefix, char *zone, struct tm *initt, struct tm *tm, time_t tics)
{
	if (initt->tm_year != tm->tm_year) {
		initt->tm_year = tm->tm_year;
		pub_i(prefix, zone, "year", tm->tm_year + 1900, TRUE);
	}
	if (initt->tm_mon != tm->tm_mon) {
		initt->tm_mon = tm->tm_mon;
		pub_i(prefix, zone, "month", tm->tm_mon + 1, TRUE);
	}
	if (initt->tm_mday != tm->tm_mday) {
		pub_i(prefix, zone, "day", initt->tm_mday = tm->tm_mday, TRUE);
		pub_ftime(prefix, zone, "weekday", "%A", tm, TRUE);
		pub_ftime(prefix, zone, "dow", "%u", tm, TRUE);
		pub_ftime(prefix, zone, "week", "%V", tm, TRUE);
		pub_ftime(prefix, zone, "isodate", "%F", tm, TRUE);
	}
	if (initt->tm_hour != tm->tm_hour) {
		pub_i(prefix, zone, "hour", initt->tm_hour = tm->tm_hour, TRUE);
		pub_ftime(prefix, zone, "ampm", "%p", tm, TRUE);
	}
	if (initt->tm_min != tm->tm_min) {
		pub_i(prefix, zone, "minute", initt->tm_min = tm->tm_min, FALSE);
	}
	if (initt->tm_sec != tm->tm_sec) {
		pub_i(prefix, zone, "second", initt->tm_sec = tm->tm_sec, FALSE);
	}
	pub_i(prefix, zone, "tics", tics, FALSE);
	pub_ftime(prefix, zone, "time", "%H:%M:%S", tm, FALSE);
}

/*
 * The _init structs must be all 0 at first call.
 */

void pubtime(char *prefix, struct tm *gmt_init, struct tm *local_init)
{
	time_t tics;
	struct tm gtm, ltm;

	time(&tics);

	if (gmtime_r(&tics, &gtm) == NULL)
		return;
	if (localtime_r(&tics, &ltm) == NULL)
		return;

	pub_i(prefix, NULL, "uptime", tics - start_tics, FALSE);

	if (do_utc)
		slices(prefix, "utc", gmt_init, &gtm, tics);
	if (do_local)
		slices(prefix, "local", local_init, &ltm, tics);
}


int main(int argc, char **argv)
{
	struct tm gmt_init, local_init;
	char ch, *progname = *argv, *nodename, *topic, *prefix = DEFAULT_PREFIX;;
	int usage = 0, interval = 10, rc;
	struct utsname uts;
	char clientid[30];
	char *host = "localhost", *ca_file;
	int port = 1883, keepalive = 60;
	int do_tls = FALSE, tls_insecure = FALSE;
	int do_psk = FALSE;
	char *psk_key = NULL, *psk_identity = NULL;

	while ((ch = getopt(argc, argv, "i:t:h:p:C:LUK:I:")) != EOF) {
		switch (ch) {
			case 'C':
				ca_file = optarg;
				do_tls = TRUE;
				break;
			case 'h':
				host = optarg;
				break;
			case 'i':
				interval = atoi(optarg);
				interval = (interval < 1) ? 1 : interval;
				break;
			case 's':
				tls_insecure = TRUE;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 't':
				prefix = optarg;
				break;
			case 'L':
				do_local = !do_local;
				break;
			case 'U':
				do_utc = !do_utc;
				break;
			case 'I':
				psk_identity = optarg;
				do_psk = TRUE;
				break;
			case 'K':
				psk_key = optarg;
				do_psk = TRUE;
				break;
			default:
				usage = 1;
				break;
		}
	}

	if (do_tls && do_psk)
		usage = 1;
	if (do_psk && (psk_key == NULL || psk_identity == NULL))
		usage = 1;

	if (usage) {
		fprintf(stderr, "Usage: %s [-h host] [-i interval] [-p port] [-t prefix] [-C CA-cert] [-L] [-U] [-K psk-key] [-I psk-identity] [-s]\n", progname);
		exit(1);
	}

	if (uname(&uts) == 0) {
		char *p;
		nodename = strdup(uts.nodename);

		if ((p = strchr(nodename, '.')) != NULL)
			*p = 0;
	} else {
		nodename = strdup("unknown");
	}

	mosquitto_lib_init();

	sprintf(clientid, "mqtt-tics-%d", getpid());
	m = mosquitto_new(clientid, TRUE, NULL);
	if (!m) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}

	if (do_psk) {
		rc = mosquitto_tls_psk_set(m, psk_key, psk_identity,NULL);
		if (rc != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "Cannot set TLS PSK: %s\n",
				mosquitto_strerror(rc));
			exit(3);
		}
	} else if (do_tls) {
		rc = mosquitto_tls_set(m, ca_file, NULL, NULL, NULL, NULL);
		if (rc != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "Cannot set TLS PSK: %s\n",
				mosquitto_strerror(rc));
			exit(3);
		}

		/* FIXME */
		// mosquitto_tls_opts_set(m, SSL_VERIFY_PEER, "tlsv1", NULL);
		
		if (tls_insecure) {
#if LIBMOSQUITTO_VERSION_NUMBER >= 1002000
			/* mosquitto_tls_insecure_set() requires libmosquitto 1.2. */
			mosquitto_tls_insecure_set(m, TRUE);
#endif
		}
	}

	if ((rc = mosquitto_connect(m, host, port, keepalive)) != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Unable to connect to %s:%d: %s\n", host, port,
			mosquitto_strerror(rc));
		perror("");
		exit(2);
	}

	signal(SIGINT, catcher);

	mosquitto_loop_start(m);

	topic = malloc(strlen(prefix) + (3 * strlen(nodename)) + 12);
	sprintf(topic, prefix, nodename, nodename, nodename);

	memset(&gmt_init, 0, sizeof(struct tm));
	memset(&local_init, 0, sizeof(struct tm));

	time(&start_tics);

	while (1) {
		pubtime(topic, &gmt_init, &local_init);
		sleep(interval);
	}

	free(nodename);

	mosquitto_disconnect(m);
	mosquitto_loop_stop(m, false);
	mosquitto_lib_cleanup();

	return 0;
}
