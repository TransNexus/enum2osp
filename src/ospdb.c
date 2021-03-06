/*
 * ospdb.c
 *
 * Copyright (c) 2013, TransNexus, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <regex.h>
#include <sys/time.h>

#include <isc/mem.h>

#include <dns/log.h>
#include <dns/sdb.h>

#include <named/globals.h>
#include <named/client.h>

#include <osp/osp.h>
#include <osp/osputils.h>
#include <osp/ospb64.h>
#include <osp/osptrans.h>

#include "ospdb.h"

/* Buffer size */
#define OSPDB_STR_SIZE	512		/* Normal string length */
#define OSPDB_KEY_SIZE	1024	/* Key string length */

/* Constant */
#define OSPDB_MAX_SPNUM	8		/* Max number of service point URLs */
#define OSPDB_MAX_CANUM	4		/* Max number of cacert file */

/* Configuration parameter name */
#define OSPDB_NAME_SPURL		"spurl_"				/* Service point URL parameter name */
#define OSPDB_NAME_SPWEIGHT		"spweight_"				/* Service point weight parameter name */
#define OSPDB_NAME_AUDITURL		"auditurl"				/* Audit URL parameter name */
#define OSPDB_NAME_SECURITY		"security"				/* Security feature parameter name */
#define OSPDB_NAME_PRIVATEKEY	"privatekey"			/* Private key parameter name */
#define OSPDB_NAME_LOCALCERT	"localcert"				/* Local cert parameter name */
#define OSPDB_NAME_CACERT		"cacert_"				/* CA cert parameter name */
#define OSPDB_NAME_VALIDATION	"tokenvalidation"		/* Token validation parameter name */
#define OSPDB_NAME_SSLLIFETIME	"ssllifetime"			/* SSL lifetime parameter name */
#define OSPDB_NAME_CONNECTIONS	"httpmaxconnections"	/* HTTP max connections parameter name */
#define OSPDB_NAME_PERSISTENCE	"httppersistence"		/* HTTP persistence parameter name */
#define OSPDB_NAME_RETRYDELAY	"httpretrydelay"		/* HTTP retry delay parameter name */
#define OSPDB_NAME_RETRYLIMIT	"httpretrylimit"		/* HTTP retry limit parameter name */
#define OSPDB_NAME_TIMEOUT		"httptimeout"			/* HTTP timeout parameter name */
#define OSPDB_NAME_USESRCURI	"usesourceuri"			/* Support EDNS0 source URI */
#define OSPDB_NAME_DEVICEIP		"deviceip"				/* OSP client address */
#define OSPDB_NAME_MAXDEST		"maxdestinations"		/* Max number of destinations parameter name */
#define OSPDB_NAME_NIDLOCATION	"networkidlocation"		/* Destination network ID location parameter name */
#define OSPDB_NAME_NIDNAME		"networkidname"			/* Destination network ID name parameter name */
#define OSPDB_NAME_USERPHONE	"userphone"				/* Append user=phone parameter name */

/* Configuration parameter value */
#define OSPDB_VALUE_NO			"no"						/* Boolean flase */
#define OSPDB_VALUE_YES			"yes"						/* Boolean true */
#define OSPDB_DEF_SPURL			"http:/*127.0.0.1:5045/osp"	/* Default service point RUL */
#define OSPDB_DEF_SPWEIGHT		1000						/* Default service point weight */
#define OSPDB_MIN_SPWEIGHT		1							/* Min service point weight */
#define OSPDB_DEF_AUDITURL		"http://localhost:1234"		/* Default audit URL */
#define OSPDB_DEF_SECURITY		ISC_FALSE					/* Default security flag */
#define OSPDB_DEF_PRIVATEKEY	"privatekey.pem"			/* Default private key */
#define OSPDB_DEF_LOCALCERT		"localcert.pem"				/* Default local cert */
#define OSPDB_DEF_CACERT		"cacert_0.pem"				/* Default CA cert */
#define OSPDB_DEF_VALIDATION	1							/* Default token validation, locally */
#define OSPDB_DEF_SSLLIFETIME	300							/* Default SSL life time in seconds */
#define OSPDB_MIN_SSLLIFETIME	0							/* Min SSL life time */
#define OSPDB_DEF_CONNECTIONS	20							/* Default max number of connections */
#define OSPDB_MIN_CONNECTIONS	1							/* Min max number of connections */
#define OSPDB_MAX_CONNECTIONS	1000						/* Max max number of connections */
#define OSPDB_DEF_PERSISTENCE	60							/* Default HTTP persistence in seconds */
#define OSPDB_MIN_PERSISTENCE	0							/* Min HTTP persistence */
#define OSPDB_DEF_RETRYDELAY	0							/* Default retry delay */
#define OSPDB_MIN_RETRYDELAY	0							/* Min retry delay */
#define OSPDB_MAX_RETRYDELAY	10							/* Max retry delay */
#define OSPDB_DEF_RETRYLIMIT	2							/* Default retry times */
#define OSPDB_MIN_RETRYLIMIT	0							/* Min retry times */
#define OSPDB_MAX_RETRYLIMIT	100							/* Max retry times */
#define OSPDB_DEF_TIMEOUT		10000						/* Default timeout */
#define OSPDB_MIN_TIMEOUT		200							/* Min timeout in milliseconds */
#define OSPDB_MAX_TIMEOUT		60000						/* Max timeout in milliseconds */
#define OSPDB_CUSTOMER_ID		""							/* Customer ID */
#define OSPDB_DEVICE_ID			""							/* Device ID */
#define OSPDB_DEF_USESRCURI		ISC_TRUE					/* Support EDNS0 source URI */
#define OSPDB_DEF_DEVICEIP		"127.0.0.1"					/* Default OSP client address */
#define OSPDB_DEF_MAXDEST		2							/* Default max number of destinations returned from OSPrey server */
#define OSPDB_MIN_MAXDEST		1							/* Min max number of destinations returned from OSPrey server */
#define OSPDB_MAX_MAXDEST		12							/* Max max number of destinations returned from OSPrey server */
#define OSPDB_DEF_NIDLOCATION	2							/* Default destination network ID location, URI parameter */
#define OSPDB_MIN_NIDLOCATION	0							/* Min destination network ID location, URI parameter */
#define OSPDB_MAX_NIDLOCATION	2							/* Max destination network ID location, URI parameter */
#define OSPDB_DEF_NIDNAME		"networkid"					/* Default destination network ID name */
#define OSPDB_DEF_USERPHONE		ISC_FALSE					/* Default user=phone flag */
#define OSPDB_DEF_PROTOCOL		OSPC_PROTNAME_SIP			/* Default signaling protocol */

/* Protocol */
#define OSPDB_PROTOCOL_SIP		"sip"	/* SIP */
#define OSPDB_PROTOCOL_H323		"h323"	/* H.323 */

/* URI scheme header */
#define OSPDB_URIHEADER_SIP		"sip:"	/* SIP URI scheme header */
#define OSPDB_URIHEADER_SIPS	"sips:"	/* SIPS URI scheme header */
#define OSPDB_URIHEADER_TEL		"tel:"	/* Telephone URI scheme header */

/* URI scheme header length */
#define OSPDB_URIHLEN_SIP		4		/* SIP URI scheme header */
#define OSPDB_URIHLEN_SIPS		5		/* SIPS URI scheme header */
#define OSPDB_URIHLEN_TEL		4		/* Telephone URI scheme header */

/* Configuration parameters */
typedef struct ospdb_config {
	int spnum;										/* Number of service points */
	char spurl[OSPDB_MAX_SPNUM][OSPDB_STR_SIZE];	/* Service point URL */
	int spweight[OSPDB_MAX_SPNUM];					/* Service point weight */
	char auditurl[OSPDB_STR_SIZE];					/* Audit URL */
	isc_boolean_t security;							/* Security flag */
	char privatekey[OSPDB_STR_SIZE];				/* Private key */
	char localcert[OSPDB_STR_SIZE];					/* Local cert */
	int canum;										/* Number of cacerts */
	char cacert[OSPDB_MAX_CANUM][OSPDB_STR_SIZE];	/* CA cert */
	int validation;									/* Token validation flag */
	int ssllifetime;								/* SSL life time */
	int connections;								/* HTTP max number of connections */
	int persistence;								/* HTTP persistence */
	int retrydelay;									/* HTTP retry delay */
	int retrylimit;									/* HTTP retry limit */
	int timeout;									/* HTTP timeout */
} ospdb_config_t;

/* Running data */
typedef struct ospdb_data {
	isc_boolean_t usesrcuri;		/* Support EDNS0 source URI flag */
	char deviceip[OSPDB_STR_SIZE];	/* OSP client address */
	int maxdest;					/* Max number of destinations */
	int nidlocation;				/* Destination network ID location */
	char nidname[OSPDB_STR_SIZE];	/* Destination network ID name */
	isc_boolean_t userphone;		/* Append user=phone flag */
	OSPTPROVHANDLE provider;		/* OSP provider handle */
} ospdb_data_t;

/* Query info */
typedef struct ospdb_query {
	const char *name;		/* Domain name */
	const char *serverip;	/* DNS server address */
	const char *clientip;	/* DNS client address */
	const char *srcuriuser;	/* Source URI user */
	const char *srcurihost;	/* Source URI host */
} ospdb_query_t;

/* Response info */
typedef struct ospdb_response {
	int count;											/* Destination count, starting from 1 */
	unsigned int total;									/* Total number fo destinations */
	char called[OSPDB_STR_SIZE];						/* Called number */
	char dest[OSPDB_STR_SIZE];							/* Destination address */
	char dnid[OSPDB_STR_SIZE];							/* Destination network ID */
	OSPE_PROTOCOL_NAME protocol;						/* Destination signaling protocol */
	char nprn[OSPDB_STR_SIZE];							/* Routing number */
	char npcic[OSPDB_STR_SIZE];							/* Carrier Identification Code */
	int npdi;											/* Number Portability Dip */
	char opname[OSPC_OPNAME_NUMBER][OSPDB_STR_SIZE];	/* Operator names */
} ospdb_response_t;

#define OSPDB_LOG_START					isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_SDB, ISC_LOG_DEBUG(3), "%s: Start", (const char *)__func__)
#define OSPDB_LOG_END					isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_SDB, ISC_LOG_DEBUG(3), "%s: End", (const char *)__func__)
#define OSPDB_LOG(_level, _fmt, ...)	isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_SDB, _level, "%s: "_fmt"", (const char *)__func__, __VA_ARGS__)

static const char *B64PKey = "MIIBOgIBAAJBAK8t5l+PUbTC4lvwlNxV5lpl+2dwSZGW46dowTe6y133XyVEwNiiRma2YNk3xKs/TJ3Wl9Wpns2SYEAJsFfSTukCAwEAAQJAPz13vCm2GmZ8Zyp74usTxLCqSJZNyMRLHQWBM0g44Iuy4wE3vpi7Wq+xYuSOH2mu4OddnxswCP4QhaXVQavTAQIhAOBVCKXtppEw9UaOBL4vW0Ed/6EA/1D8hDW6St0h7EXJAiEAx+iRmZKhJD6VT84dtX5ZYNVk3j3dAcIOovpzUj9a0CECIEduTCapmZQ5xqAEsLXuVlxRtQgLTUD4ZxDElPn8x0MhAiBE2HlcND0+qDbvtwJQQOUzDgqg5xk3w8capboVdzAlQQIhAMC+lDL7+gDYkNAft5Mu+NObJmQs4Cr+DkDFsKqoxqrm";
static const char *B64LCert = "MIIBeTCCASMCEHqkOHVRRWr+1COq3CR/xsowDQYJKoZIhvcNAQEEBQAwOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMB4XDTA1MDYyMzAwMjkxOFoXDTA2MDYyNDAwMjkxOFowRTELMAkGA1UEBhMCQVUxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNVBAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDBcMA0GCSqGSIb3DQEBAQUAA0sAMEgCQQCvLeZfj1G0wuJb8JTcVeZaZftncEmRluOnaME3ustd918lRMDYokZmtmDZN8SrP0yd1pfVqZ7NkmBACbBX0k7pAgMBAAEwDQYJKoZIhvcNAQEEBQADQQDnV8QNFVVJx/+7IselU0wsepqMurivXZzuxOmTEmTVDzCJx1xhA8jd3vGAj7XDIYiPub1PV23eY5a2ARJuw5w9";
static const char *B64CACert = "MIIBYDCCAQoCAQEwDQYJKoZIhvcNAQEEBQAwOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMB4XDTAyMDIwNDE4MjU1MloXDTEyMDIwMzE4MjU1MlowOzElMCMGA1UEAxMcb3NwdGVzdHNlcnZlci50cmFuc25leHVzLmNvbTESMBAGA1UEChMJT1NQU2VydmVyMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPGeGwV41EIhX0jEDFLRXQhDEr50OUQPq+f55VwQd0TQNts06BP29+UiNdRW3c3IRHdZcJdC1Cg68ME9cgeq0h8CAwEAATANBgkqhkiG9w0BAQQFAANBAGkzBSj1EnnmUxbaiG1N4xjIuLAWydun7o3bFk2tV8dBIhnuh445obYyk1EnQ27kI7eACCILBZqi2MHDOIMnoN0=";

/* OSP SDB driver instance */
static dns_sdbimplementation_t *ospdb = NULL;

/* OSP client init flag */
static isc_boolean_t ospdb_init_flag = ISC_FALSE;

/*
 * Init configuration parameter structure
 * param cfg Configuration parameter structure
 * param data Running data structure
 */
static void ospdb_init_config(
	ospdb_config_t *cfg,
	ospdb_data_t *data)
{
	int i;

	OSPDB_LOG_START;

	/* Configuration parameters */
	cfg->spnum = 0;
	for (i = 0; i < OSPDB_MAX_SPNUM; i++) {
		cfg->spurl[i][0] = '\0';
		cfg->spweight[i] = OSPDB_DEF_SPWEIGHT;
	}
	cfg->auditurl[0] = '\0';
	cfg->security = OSPDB_DEF_SECURITY;
	cfg->privatekey[0] = '\0';
	cfg->localcert[0] = '\0';
	cfg->canum = 0;
	for (i = 0; i < OSPDB_MAX_CANUM; i++) {
		cfg->cacert[i][0] = '\0';
	}
	cfg->validation = OSPDB_DEF_VALIDATION;
	cfg->ssllifetime = OSPDB_DEF_SSLLIFETIME;
	cfg->connections = OSPDB_DEF_CONNECTIONS;
	cfg->persistence = OSPDB_DEF_PERSISTENCE;
	cfg->retrydelay = OSPDB_DEF_RETRYDELAY;
	cfg->retrylimit = OSPDB_DEF_RETRYLIMIT;
	cfg->timeout = OSPDB_DEF_TIMEOUT;

	/* Running data */
	data->usesrcuri = OSPDB_DEF_USESRCURI;
	data->deviceip[0] = '\0';
	data->maxdest = OSPDB_DEF_MAXDEST;
	data->nidlocation = OSPDB_DEF_NIDLOCATION;
	data->nidname[0] = '\0';
	data->userphone = OSPDB_DEF_USERPHONE;

	OSPDB_LOG_END;
}

/*
 * Get configuration parameters
 * param argc Number of configuration parameters
 * param argv Configuration parameters
 * param cfg Configuration parameter structure
 * param data Running data structure
 */
static void ospdb_get_config(
	int argc,
	char **argv,
	ospdb_config_t *cfg,
	ospdb_data_t *data)
{
	int i, j, tmp;
	char buffer1[OSPDB_STR_SIZE];
	char buffer2[OSPDB_STR_SIZE];
	char *saveptr = NULL;
	char *name, *value;

	OSPDB_LOG_START;

	/* Parse configuration parameters */
	for (i = 0; i < argc; i++) {
		/* Get parameter */
		snprintf(buffer1, sizeof(buffer1), "%s", argv[i]);
		name = strtok_r(buffer1, "=", &saveptr);
		value = strtok_r(NULL, "=", &saveptr);
		if (value != NULL) {
			if (strncmp(name, OSPDB_NAME_SPURL, strlen(OSPDB_NAME_SPURL)) == 0) {
				for (j = 0; j < OSPDB_MAX_SPNUM; j++) {
					snprintf(buffer2, sizeof(buffer2), "%s%d", OSPDB_NAME_SPURL, j + 1);
					if (strcmp(name, buffer2) == 0) {
						cfg->spnum = (cfg->spnum > (j + 1)) ? cfg->spnum : j + 1;
						snprintf(cfg->spurl[j], sizeof(cfg->spurl[j]), value);
						OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, cfg->spurl[j]);
						break;
					}
				}
				if (j == OSPDB_MAX_SPNUM) {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong parameter name '%s'", name);
				}
			} else if (strncmp(name, OSPDB_NAME_SPWEIGHT, strlen(OSPDB_NAME_SPWEIGHT)) == 0) {
				for (j = 0; j < OSPDB_MAX_SPNUM; j++) {
					snprintf(buffer2, sizeof(buffer2), "%s%d", OSPDB_NAME_SPWEIGHT, j + 1);
					if (strcmp(name, buffer2) == 0) {
						tmp = atoi(value);
						if (tmp >= OSPDB_MIN_SPWEIGHT) {
							cfg->spweight[j] = tmp;
							OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->spweight[j]);
						} else {
							OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
						}
						break;
					}
				}
				if (j == OSPDB_MAX_SPNUM) {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong parameter name '%s'", name);
				}
			} else if (strcmp(name, OSPDB_NAME_AUDITURL) == 0) {
				snprintf(cfg->auditurl, sizeof(cfg->auditurl), value);
				OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, cfg->auditurl);
			} else if (strcmp(name, OSPDB_NAME_SECURITY) == 0) {
				if (strcmp(value, OSPDB_VALUE_YES) == 0) {
					cfg->security = ISC_TRUE;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->security);
				} else if (strcmp(value, OSPDB_VALUE_NO) == 0) {
					cfg->security = ISC_FALSE;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->security);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_PRIVATEKEY) == 0) {
				snprintf(cfg->privatekey, sizeof(cfg->privatekey), value);
				OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, cfg->privatekey);
			} else if (strcmp(name, OSPDB_NAME_LOCALCERT) == 0) {
				snprintf(cfg->localcert, sizeof(cfg->localcert), value);
				OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, cfg->localcert);
			} else if (strncmp(name, OSPDB_NAME_CACERT, strlen(OSPDB_NAME_CACERT)) == 0) {
				for (j = 0; j < OSPDB_MAX_CANUM; j++) {
					snprintf(buffer2, sizeof(buffer2), "%s%d", OSPDB_NAME_CACERT, j + 1);
					if (strcmp(name, buffer2) == 0) {
						cfg->canum = (cfg->canum > (j + 1)) ? cfg->canum : j + 1;
						snprintf(cfg->cacert[j], sizeof(cfg->cacert[j]), value);
						OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, cfg->cacert[j]);
						break;
					}
				}
				if (j == OSPDB_MAX_CANUM) {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong parameter name '%s'", name);
				}
			} else if (strcmp(name, OSPDB_NAME_VALIDATION) == 0) {
				cfg->validation = atoi(value);
				OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->validation);
			} else if (strcmp(name, OSPDB_NAME_SSLLIFETIME) == 0) {
				tmp = atoi(value);
				if (tmp >= OSPDB_MIN_SSLLIFETIME) {
					cfg->ssllifetime = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->ssllifetime);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_CONNECTIONS) == 0) {
				tmp = atoi(value);
				if ((tmp >= OSPDB_MIN_CONNECTIONS) && (tmp <= OSPDB_MAX_CONNECTIONS)) {
					cfg->connections = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->connections);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_PERSISTENCE) == 0) {
				tmp = atoi(value);
				if (tmp >= OSPDB_MIN_PERSISTENCE) {
					cfg->persistence = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->persistence);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_RETRYDELAY) == 0) {
				tmp = atoi(value);
				if ((tmp >= OSPDB_MIN_RETRYDELAY) && (tmp <= OSPDB_MAX_RETRYDELAY)) {
					cfg->retrydelay = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->retrydelay);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_RETRYLIMIT) == 0) {
				tmp = atoi(value);
				if ((tmp >= OSPDB_MIN_RETRYLIMIT) && (tmp <= OSPDB_MAX_RETRYLIMIT)) {
					cfg->retrylimit = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->retrylimit);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_TIMEOUT) == 0) {
				tmp = atoi(value);
				if ((tmp >= OSPDB_MIN_TIMEOUT) && (tmp <= OSPDB_MAX_TIMEOUT)) {
					cfg->timeout = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, cfg->timeout);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_USESRCURI) == 0) {
				if (strcmp(value, OSPDB_VALUE_YES) == 0) {
					data->usesrcuri = ISC_TRUE;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, data->usesrcuri);
				} else if (strcmp(value, OSPDB_VALUE_NO) == 0) {
					data->usesrcuri = ISC_FALSE;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, data->usesrcuri);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_DEVICEIP) == 0) {
				snprintf(data->deviceip, sizeof(data->deviceip), value);
				OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, data->deviceip);
			} else if (strcmp(name, OSPDB_NAME_MAXDEST) == 0) {
				tmp = atoi(value);
				if ((tmp >= OSPDB_MIN_MAXDEST) && (tmp <= OSPDB_MAX_MAXDEST)) {
					data->maxdest = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, data->maxdest);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_NIDLOCATION) == 0) {
				tmp = atoi(value);
				if ((tmp >= OSPDB_MIN_NIDLOCATION) && (tmp <= OSPDB_MAX_NIDLOCATION)) {
					data->nidlocation = tmp;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, data->nidlocation);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else if (strcmp(name, OSPDB_NAME_NIDNAME) == 0) {
				snprintf(data->nidname, sizeof(data->nidname), value);
				OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%s'", name, data->nidname);
			} else if (strcmp(name, OSPDB_NAME_USERPHONE) == 0) {
				if (strcmp(value, OSPDB_VALUE_YES) == 0) {
					data->userphone = ISC_TRUE;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, data->userphone);
				} else if (strcmp(value, OSPDB_VALUE_NO) == 0) {
					data->userphone = ISC_FALSE;
					OSPDB_LOG(ISC_LOG_DEBUG(2), "%s = '%d'", name, data->userphone);
				} else {
					OSPDB_LOG(ISC_LOG_WARNING, "Wrong %s value '%s'", name, value);
				}
			} else {
				OSPDB_LOG(ISC_LOG_WARNING, "Wrong parameter name '%s'", name);
			}
		} else {
			OSPDB_LOG(ISC_LOG_WARNING, "Empty %s value", name);
		}
	}

	OSPDB_LOG_END;
}

/*
 * Check configuration parameters
 * param cfg Configuration parameter structure
 * param data Running data structure
 * return ISC_R_SUCCESS
 */
static isc_boolean_t ospdb_check_config(
	ospdb_config_t *cfg,
	ospdb_data_t *data)
{
	int i;
	isc_boolean_t result = ISC_R_SUCCESS;

	OSPDB_LOG_START;

	/* Configuration parameters */
	for (i = 0; i < cfg->spnum; i++) {
		if (cfg->spurl[i][0] == '\0') {
			OSPDB_LOG(ISC_LOG_WARNING, "Without spuri_%d", i + 1);
			cfg->spnum = i;
			break;
		}
	}
	if (cfg->spnum == 0) {
		OSPDB_LOG(ISC_LOG_WARNING, "%s", "Without service point defined");

		snprintf(cfg->spurl[0], sizeof(cfg->spurl[0]), "%s", OSPDB_DEF_SPURL);
		cfg->spnum = 1;
	}

	if (cfg->auditurl[0] == '\0') {
		snprintf(cfg->auditurl, sizeof(cfg->auditurl), "%s", OSPDB_DEF_AUDITURL);
	}

	if (cfg->security == ISC_TRUE) {
		if (cfg->privatekey[0] == '\0') {
			snprintf(cfg->privatekey, sizeof(cfg->privatekey), "%s", OSPDB_DEF_PRIVATEKEY);
		}

		if (cfg->localcert[0] == '\0') {
			snprintf(cfg->localcert, sizeof(cfg->localcert), "%s", OSPDB_DEF_LOCALCERT);
		}

		for (i = 0; i < cfg->canum; i++) {
			if (cfg->cacert[i][0] == '\0') {
				OSPDB_LOG(ISC_LOG_WARNING, "Without cacert_%d", i + 1);
				cfg->canum = i;
				break;
			}
		}
		if (cfg->canum == 0) {
			snprintf(cfg->cacert[0], sizeof(cfg->cacert[0]), "%s", OSPDB_DEF_CACERT);
			cfg->canum = 1;
		}
	}

	/* Running data */
	if (data->deviceip[0] == '\0') {
		snprintf(data->deviceip, sizeof(data->deviceip), "%s", OSPDB_DEF_DEVICEIP);
	}

	if (data->nidname[0] == '\0') {
		snprintf(data->nidname, sizeof(data->nidname), "%s", OSPDB_DEF_NIDNAME);
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Dump configuration parameters
 * param cfg Configuration parameter structure
 * param data Running data structure
 */
static void ospdb_dump_config(
	ospdb_config_t *cfg,
	ospdb_data_t *data)
{
	int i;

	OSPDB_LOG_START;

	/* Configuration parameters */
	for (i = 0; i < cfg->spnum; i++) {
		OSPDB_LOG(ISC_LOG_DEBUG(1), "%s%d = '%s'", OSPDB_NAME_SPURL, i + 1, cfg->spurl[i]);
		OSPDB_LOG(ISC_LOG_DEBUG(1), "%s%d = '%d'", OSPDB_NAME_SPWEIGHT, i + 1, cfg->spweight[i]);
	}
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%s'", OSPDB_NAME_AUDITURL, cfg->auditurl);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_SECURITY, cfg->security);
	if (cfg->security == ISC_TRUE) {
		OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%s'", OSPDB_NAME_PRIVATEKEY, cfg->privatekey);
		OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%s'", OSPDB_NAME_LOCALCERT, cfg->localcert);
		for (i = 0; i < cfg->canum; i++) {
			OSPDB_LOG(ISC_LOG_DEBUG(1), "%s%d = '%s'", OSPDB_NAME_CACERT, i + 1, cfg->cacert[i]);
		}
	}
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_VALIDATION, cfg->validation);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_SSLLIFETIME, cfg->ssllifetime);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_CONNECTIONS, cfg->connections);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_PERSISTENCE, cfg->persistence);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_RETRYDELAY, cfg->retrydelay);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_RETRYLIMIT, cfg->retrylimit);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_TIMEOUT, cfg->timeout);

	/* Running data */
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_USESRCURI, data->usesrcuri);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%s'", OSPDB_NAME_DEVICEIP, data->deviceip);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_MAXDEST, data->maxdest);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_NIDLOCATION, data->nidlocation);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%s'", OSPDB_NAME_NIDNAME, data->nidname);
	OSPDB_LOG(ISC_LOG_DEBUG(1), "%s = '%d'", OSPDB_NAME_USERPHONE, data->userphone);

	OSPDB_LOG_END;
}

/*
 * Create OSP provider
 * param cfg Configuration parameter structure
 * param provider OSP provider handle
 * return ISC_R_SUCCESS successful, ISC_R_FAILURE failed
 */
static isc_result_t ospdb_create_provider(
	ospdb_config_t *cfg,
	OSPTPROVHANDLE *provider)
{
	int i, error = OSPC_ERR_NO_ERROR;
	const char *spurls[OSPDB_MAX_SPNUM];
	unsigned long spweights[OSPDB_MAX_SPNUM];
	OSPTPRIVATEKEY privatekey;
	OSPT_CERT localcert;
	const OSPT_CERT *cacerts[OSPDB_MAX_CANUM];
	OSPT_CERT certs[OSPDB_MAX_CANUM];
	unsigned char keydata[OSPDB_KEY_SIZE];
	unsigned char certdata[OSPDB_KEY_SIZE];
	unsigned char cadata[OSPDB_KEY_SIZE];
	isc_result_t result = ISC_R_FAILURE;

	OSPDB_LOG_START;

	/* Service point URL and weight */
	for (i = 0; i < cfg->spnum; i++) {
		spurls[i] = cfg->spurl[i];
		spweights[i] = cfg->spweight[i];
	}

	/* Certificate */
	if (cfg->security == ISC_TRUE) {
		/* Private key */
		privatekey.PrivateKeyData = NULL;
		privatekey.PrivateKeyLength = 0;
		/* Local cert */
		localcert.CertData = NULL;
		localcert.CertDataLength = 0;
		/* CA certs */
		for (i = 0; i < OSPDB_MAX_CANUM; i++) {
			certs[i].CertData = NULL;
			certs[i].CertDataLength = 0;
		}
		/* Load certificates */
		if ((error = OSPPUtilLoadPEMPrivateKey((unsigned char *)cfg->privatekey, &privatekey)) != OSPC_ERR_NO_ERROR) {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to load privatekey, error %d", error);
		} else if ((error = OSPPUtilLoadPEMCert((unsigned char *)cfg->localcert, &localcert)) != OSPC_ERR_NO_ERROR) {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to load localcert, error %d", error);
		} else {
			for (i = 0; i < cfg->canum; i++) {
				if ((error = OSPPUtilLoadPEMCert((unsigned char *)cfg->cacert[i], &certs[i])) != OSPC_ERR_NO_ERROR) {
					OSPDB_LOG(ISC_LOG_ERROR, "Failed to load cacert %d, error %d", i, error);
					break;
				} else {
					cacerts[i] = &certs[i];
				}
			}
		}
	} else {
		/* Private key */
		privatekey.PrivateKeyData = keydata;
		privatekey.PrivateKeyLength = sizeof(keydata);
		/* Local cert */
		localcert.CertData = certdata;
		localcert.CertDataLength = sizeof(certdata);
		/* CA certs */
		cfg->canum = 1;
		certs[0].CertData = cadata;
		certs[0].CertDataLength = sizeof(cadata);
		cacerts[0] = &certs[0];
		/* Load default certificates */
		if ((error = OSPPBase64Decode(B64PKey, strlen(B64PKey), privatekey.PrivateKeyData, &privatekey.PrivateKeyLength)) != OSPC_ERR_NO_ERROR) {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to decode privatekey, error %d", error);
		} else if ((error = OSPPBase64Decode(B64LCert, strlen(B64LCert), localcert.CertData, &localcert.CertDataLength)) != OSPC_ERR_NO_ERROR) {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to decode localcert, error %d", error);
		} else if ((error = OSPPBase64Decode(B64CACert, strlen(B64CACert), certs[0].CertData, &certs[0].CertDataLength)) != OSPC_ERR_NO_ERROR) {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to decode cacert, error %d", error);
		}
	}

	if (error == OSPC_ERR_NO_ERROR) {
		/* Create OSP provider */
		error = OSPPProviderNew(
			cfg->spnum,
			spurls,
			spweights,
			cfg->auditurl,
			&privatekey,
			&localcert,
			cfg->canum,
			cacerts,
			cfg->validation,
			cfg->ssllifetime,
			cfg->connections,
			cfg->persistence,
			cfg->retrydelay,
			cfg->retrylimit,
			cfg->timeout,
			OSPDB_CUSTOMER_ID,
			OSPDB_DEVICE_ID,
			provider);
		if (error != OSPC_ERR_NO_ERROR) {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to create provider, error %d", error);
		} else {
			result = ISC_R_SUCCESS;
		}
	}

	/* Free memory */
	if (cfg->security == ISC_TRUE) {
		if (privatekey.PrivateKeyData) {
			free(privatekey.PrivateKeyData);
		}
		if (localcert.CertData) {
			free(localcert.CertData);
		}
		for (i = 0; i < cfg->canum; i++) {
			if (certs[i].CertData) {
				free(certs[i].CertData);
			}
		}
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Delete OSP provider
 * param provider OSP provider handle
 */
static void ospdb_delete_provider(
	OSPTPROVHANDLE provider)
{
	OSPDB_LOG_START;

	OSPPProviderDelete(provider, 0);

	OSPDB_LOG_END;
}

/*
 * Parse SIP/SIPS URI
 * param secure SIP or SIPS
 * param uri SIP/SIPS URI
 * param user URI user buffer
 * param usersize URI user buffer size
 * param host URI host buffer
 * param hostsize URI host buffer size
 */
static void ospdb_parse_sipuri(
	isc_boolean_t secure,
	char* uri,
	char* user,
	int usersize,
	char* host,
	int hostsize)
{
	char* head = uri + ((secure == ISC_TRUE) ? OSPDB_URIHLEN_SIPS : OSPDB_URIHLEN_SIP);
	char* end;
	char* ptr;

	if ((ptr = strchr(head, '@')) == NULL) {
		user[0] = '\0';
	} else {
		/* With userinfo */
		ptr[0] = '\0';
		/* Parse user */
		if ((end = strchr(head, ':')) != NULL) {
			/* With password */
			end[0] = '\0';
		}
		if ((end = strchr(head, ';')) != NULL) {
			/* With parameter */
			end[0] = '\0';
		}
		strncpy(user, head, usersize);
		user[usersize - 1] = '\0';
		head = ptr + 1;
	}
	/* Prase hostport */
	if ((end = strchr(head, ';')) != NULL) {
		end[0] = '\0';
	}
	strncpy(host, head, hostsize);
	host[hostsize - 1] = '\0';
}

/*
 * Parse telephone URI
 * param uri Telephone URI
 * param user URI user buffer
 * param usersize URI user buffer size
 */
static void ospdb_parse_teluri(
	char* uri,
	char* user,
	int usersize)
{
	char* head = uri + OSPDB_URIHLEN_TEL;
	char* end;

	if ((end = strchr(head, ';')) != NULL) {
		end[0] = '\0';
	}
	strncpy(user, head, usersize);
	user[usersize - 1] = '\0';
}

/*
 * Parse source URI
 * param uri Source URI
 * param user URI user buffer
 * param usersize URI user buffer size
 * param host URI host buffer
 * param hostsize URI host buffer size
 */
static void ospdb_parse_uri(
	char* uri,
	char* user,
	int usersize,
	char* host,
	int hostsize)
{
	char* head = uri;

	user[0] = '\0';
	host[0] = '\0';

	if (memcmp(head, OSPDB_URIHEADER_SIP, OSPDB_URIHLEN_SIP) == 0) {
		ospdb_parse_sipuri(ISC_FALSE, uri, user, usersize, host, hostsize);
	} else if (memcmp(head, OSPDB_URIHEADER_SIPS, OSPDB_URIHLEN_SIPS) == 0) {
		ospdb_parse_sipuri(ISC_TRUE, uri, user, usersize, host, hostsize);
	} else if (memcmp(head, OSPDB_URIHEADER_TEL, OSPDB_URIHLEN_TEL) == 0) {
		ospdb_parse_teluri(uri, user, usersize);
	}
}

/*
 * Convert "address:port" to "[x.x.x.x]:port" or "hostname:port" format
 * param addr Address string
 * param buffer Destination buffer
 * param bufsize Size of buffer
 */
static void ospdb_convert_toout(
	const char* addr,
	char* buf,
	int bufsize)
{
	struct in_addr inp;
	char buffer[OSPDB_STR_SIZE];
	char* port;

	OSPDB_LOG_START;

	if ((addr != NULL) && (*addr != '\0')) {
		snprintf(buffer, sizeof(buffer), "%s", addr);

		if((port = strchr(buffer, ':')) != NULL) {
			*port = '\0';
			port++;
		}

		if (inet_pton(AF_INET, buffer, &inp) == 1) {
			if (port != NULL) {
				snprintf(buf, bufsize, "[%s]:%s", buffer, port);
			} else {
				snprintf(buf, bufsize, "[%s]", buffer);
			}
		} else {
			snprintf(buf, bufsize, "%s", addr);
		}
	} else {
		*buf = '\0';
	}

	OSPDB_LOG_END;
}

/*
 * Convert "[x.x.x.x]:port" or "hostname:prot" to "address:port" format
 * param addr Address string
 * param buf Destination buffer
 * param bufsize Size of buffer
 */
static void ospdb_convert_toin(
	const char* addr,
	char* buf,
	int bufsize)
{
	char buffer[OSPDB_STR_SIZE];
	char* end;
	char* port;

	OSPDB_LOG_START;

	if ((addr != NULL) && (*addr != '\0')) {
		snprintf(buffer, sizeof(buffer), "%s", addr);

		if (buffer[0] == '[') {
			if((port = strchr(buffer + 1, ':')) != NULL) {
				*port = '\0';
				port++;
			}

			if ((end = strchr(buffer + 1, ']')) != NULL) {
				*end = '\0';
			}

			if (port != NULL) {
				snprintf(buf, bufsize, "%s:%s", buffer + 1, port);
			} else {
				snprintf(buf, bufsize, "%s", buffer + 1);
			}
		} else {
			snprintf(buf, bufsize, "%s", buffer);
		}
	} else {
		*buf = '\0';
	}

	OSPDB_LOG_END;
}

/*
 * Check domain of ENUM query
 * param domain Domain name
 * return ISC_R_SUCCESS successful, ISC_R_FAILURE failed
 */
static isc_result_t ospdb_check_domain(
	const char *domain)
{
	int i, size = strlen(domain);
	isc_result_t result = ISC_R_SUCCESS;

	OSPDB_LOG_START;

	for (i = 0; i < size; i++) {
		if (i % 2) {
			if (domain[i] != '.') {
				result = ISC_R_FAILURE;
			}
		} else {
			if ((domain[i] < '0') || (domain[i] > '9')) {
				result = ISC_R_FAILURE;
			}
		}
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Convert ENUM domain name to called number
 * param domain ENUM domain
 * param buf Destination buffer
 * param bufsize Size of buffer
 */
static void ospdb_convert_domain(
	const char *domain,
	char *buf,
	int bufsize)
{
	char *h = buf;
	const char *p = domain + strlen(domain) - 1;
	int size = bufsize;

	OSPDB_LOG_START;

	while ((size > 1) && (p >= domain)) {
		if (*p != '.') {
			*h = *p;
			h++;
			size--;
		}
		p--;
	}
	*h = '\0';

	OSPDB_LOG(ISC_LOG_DEBUG(2), "Number = '%s'", buf);

	OSPDB_LOG_END;
}

/*
 * Request auth and routing
 * param transaction OSP Transaction handle
 * param query Query info
 * param destnum Number of destinations
 * return ISC_R_SUCCESS successful, ISC_R_NOPERM unauth or blocked, ISC_R_NOTFOUND not found, ISC_R_FAILURE failed, ISC_R_NOMORE without route
 */
static isc_result_t ospdb_request_auth(
	OSPTTRANHANDLE transaction,
	ospdb_query_t *query,
	unsigned int *destnum)
{
	char source[OSPDB_STR_SIZE];
	char srcdev[OSPDB_STR_SIZE];
	char called[OSPDB_STR_SIZE];
	struct timeval ts, te, td;
	unsigned int logsize = 0;
	int error = OSPC_ERR_NO_ERROR;
	isc_result_t result = ISC_R_SUCCESS;

	OSPDB_LOG_START;

	/* Set service type */
	OSPPTransactionSetServiceType(transaction, OSPC_SERVICE_VOICE);

	ospdb_convert_toout(query->serverip, source, sizeof(source));
	if (query->srcurihost[0] != '\0') {
		ospdb_convert_toout(query->srcurihost, srcdev, sizeof(srcdev));
	} else {
		ospdb_convert_toout(query->clientip, srcdev, sizeof(srcdev));
	}
	ospdb_convert_domain(query->name, called, sizeof(called));

	/* Log AuthReq info */
	OSPDB_LOG(ISC_LOG_DEBUG(1), "AuthReq source '%s' srcdev '%s' called '%s' calling '%s' destnum '%d'", source, srcdev, called, query->srcuriuser, *destnum);

	/* Get AuthReq start time */
	gettimeofday(&ts, NULL);

	/* Request OSP authorization */
	error = OSPPTransactionRequestAuthorisation(
		transaction,		/* Transaction handle */
		source,				/* BIND server address */
		srcdev,				/* Source device or ENUM client address */
		query->srcuriuser,	/* Calling number */
		OSPC_NFORMAT_E164,	/* Calling number format */
		called,				/* Called number */
		OSPC_NFORMAT_E164,	/* Called number format */
		NULL,				/* Username string, used if no number */
		0,					/* Number of call IDs */
		NULL,				/* Call IDs */
		NULL,				/* Preferred destinations */
		destnum,			/* Max destinations */
		&logsize,			/* Size of detail log buffer */
		NULL);				/* Detail log buffer */

	/* Get AuthReq end time */
	gettimeofday(&te, NULL);

	/* Get AuthReq cost */
	timersub(&te, &ts, &td);

	/* Log AuthReq cost info */
	OSPDB_LOG(ISC_LOG_DEBUG(2), "AuthReq for %s cost = '%lu.%06lu'", called, td.tv_sec, td.tv_usec);

	if (error == OSPC_ERR_NO_ERROR) {
		if (destnum == 0) {
			OSPDB_LOG(ISC_LOG_DEBUG(1), "Without any route for %s", called);
			result = ISC_R_NOMORE;
		}
	} else {
		OSPDB_LOG(ISC_LOG_DEBUG(1), "Unable to request auth for %s, error %d", called, error);
		switch (error) {
		case OSPC_ERR_TRAN_UNAUTHORIZED:
		case OSPC_ERR_TRAN_ROUTE_BLOCKED:
			result = ISC_R_NOPERM;
			break;
		case OSPC_ERR_TRAN_ROUTE_NOT_FOUND:
			result = ISC_R_NOTFOUND;
			break;
		default:
			result = ISC_R_FAILURE;
			break;
		}
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Check route
 * param transaction OSP Transaction handle
 * param response Response info
 * return ISC_R_SUCCESS successful, ISC_R_FAILURE failed
 */
static isc_result_t ospdb_check_route(
	OSPTTRANHANDLE transaction,
	ospdb_response_t *response)
{
	int error = OSPC_ERR_NO_ERROR;
	OSPE_DEST_OSPENABLED enabled;
	OSPTTRANS *context;
	unsigned long long transid;
	OSPE_PROTOCOL_NAME protocol;
	OSPE_OPERATOR_NAME type;
	isc_result_t result = ISC_R_FAILURE;

	OSPDB_LOG_START;

	if ((error = OSPPTransactionIsDestOSPEnabled(transaction, &enabled)) != OSPC_ERR_NO_ERROR) {
		OSPDB_LOG(ISC_LOG_ERROR, "Failed to get route OSP version, error %d", error);
	} else if ((error = OSPPTransactionGetDestProtocol(transaction, &protocol)) != OSPC_ERR_NO_ERROR) {
		OSPDB_LOG(ISC_LOG_ERROR, "Failed to get signaling protocol, error %d", error);
	} else {
		context = OSPPTransactionGetContext(transaction, &error);
		if (error == OSPC_ERR_NO_ERROR) {
			transid = context->TransactionID;
		} else {
			transid = 0;
		}

		switch(protocol) {
		case OSPC_PROTNAME_Q931:
			break;
		case OSPC_PROTNAME_UNKNOWN:
		case OSPC_PROTNAME_UNDEFINED:
		case OSPC_PROTNAME_SIP:
		case OSPC_PROTNAME_LRQ:
		case OSPC_PROTNAME_IAX:
		case OSPC_PROTNAME_T37:
		case OSPC_PROTNAME_T38:
		case OSPC_PROTNAME_SKYPE:
		case OSPC_PROTNAME_SMPP:
		case OSPC_PROTNAME_XMPP:
		case OSPC_PROTNAME_SMS:
		default:
			protocol = OSPC_PROTNAME_SIP;
			break;
		}
		response->protocol = protocol;

		/* Get destination network ID */
		if ((error = OSPPTransactionGetDestinationNetworkId(transaction, sizeof(response->dnid), response->dnid)) != OSPC_ERR_NO_ERROR) {
			response->dnid[0] = '\0';
		}

		/* Get LNP parameters */
		error = OSPPTransactionGetNumberPortabilityParameters(
			transaction,
			sizeof(response->nprn),
			response->nprn,
			sizeof(response->npcic),
			response->npcic,
			&response->npdi);
		if (error != OSPC_ERR_NO_ERROR) {
			response->nprn[0] = '\0';
			response->npcic[0] = '\0';
			response->npdi = 0;
		}

		/* Get operator names */
		for (type = OSPC_OPNAME_START; type < OSPC_OPNAME_NUMBER; type++) {
			if ((error = OSPPTransactionGetOperatorName(transaction, type, sizeof(response->opname[type]), response->opname[type])) != OSPC_ERR_NO_ERROR) {
				response->opname[type][0] = '\0';
			}
		}

		/* Log AuthRsp info */
		OSPDB_LOG(ISC_LOG_DEBUG(1),
			"AuthRsp "
			"%d/%d "
			"transid '%llu' "
			"called '%s' "
			"dest '%s' "
			"dnid '%s' "
			"protocol '%d' "
			"lnp %s/%s/%d",
			response->count, response->total,
			transid,
			response->called,
			response->dest,
			response->dnid,
			response->protocol,
			response->nprn, response->npcic, response->npdi);

		result = ISC_R_SUCCESS;
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Get first route
 * param transaction OSP Transaction handle
 * param called Called number
 * param response Response info
 * return ISC_R_SUCCESS successful, ISC_R_FAILURE failed
 */
static isc_result_t ospdb_get_first(
	OSPTTRANHANDLE transaction,
	ospdb_response_t *response)
{
	int error = OSPC_ERR_NO_ERROR;
	unsigned int timelimit;
	unsigned int callidlen = 0;
	char dest[OSPDB_STR_SIZE];
	unsigned int tokenlen = 0;
	isc_result_t result = ISC_R_SUCCESS;

	OSPDB_LOG_START;

	error = OSPPTransactionGetFirstDestination(
		transaction,				/* Transaction handle */
		0,							/* Size of timestamp buffer */
		NULL,						/* Valid after timestamp buffer */
		NULL,						/* Valid until timestamp buffer */
		&timelimit,					/* Call duration limit */
		&callidlen,					/* Size of call ID buffer */
		NULL,						/* Call ID buffer */
		sizeof(response->called),	/* Size of called number buffer */
		response->called,			/* Called number buffer */
		0,							/* Size of calling number buffer */
		NULL,						/* Calling number buffer */
		sizeof(dest),				/* Size of destination buffer */
		dest,						/* Destination buffer */
		0,							/* Size of destination device buffer */
		NULL,						/* Destination device buffer */
		&tokenlen,					/* Size of token buffer */
		NULL);						/* Token buffer */
	if (error == OSPC_ERR_NO_ERROR) {
		response->count = 1;
		ospdb_convert_toin(dest, response->dest, sizeof(response->dest));
		result = ospdb_check_route(transaction, response);
	} else {
		OSPDB_LOG(ISC_LOG_ERROR, "Failed to get first destination, error %d", error);
		result = ISC_R_FAILURE;
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Get next route
 * param transaction OSP Transaction handle
 * param called Called number
 * param response Response info
 * return ISC_R_SUCCESS successful, ISC_R_FAILURE failed
 */
static isc_result_t ospdb_get_next(
	OSPTTRANHANDLE transaction,
	ospdb_response_t *response)
{
	int error = OSPC_ERR_NO_ERROR;
	unsigned int timelimit;
	unsigned int callidlen = 0;
	char called[OSPDB_STR_SIZE];
	char dest[OSPDB_STR_SIZE];
	unsigned int tokenlen = 0;
	isc_result_t result = ISC_R_SUCCESS;

	OSPDB_LOG_START;

	error = OSPPTransactionGetNextDestination(
		transaction,	/* Transaction handle */
		0,				/* Reason */
		0,				/* Size of timestamp buffer */
		NULL,			/* Valid after timestamp buffer */
		NULL,			/* Valid until timestamp buffer */
		&timelimit,		/* Call duration limit */
		&callidlen,		/* Size of call ID buffer */
		NULL,			/* Call ID buffer */
		sizeof(called),	/* Size of called number buffer */
		called,			/* Called number buffer */
		0,				/* Size of calling number buffer */
		NULL,			/* Calling number buffer */
		sizeof(dest),	/* Size of destination buffer */
		dest,			/* Destination buffer */
		0,				/* Size of destination device buffer */
		NULL,			/* Destination device buffer */
		&tokenlen,		/* Size of token buffer */
		NULL);			/* Token buffer */
	if (error == OSPC_ERR_NO_ERROR) {
		ospdb_convert_toin(dest, response->dest, sizeof(response->dest));
		result = ospdb_check_route(transaction, response);
	} else {
		OSPDB_LOG(ISC_LOG_ERROR, "Failed to get %d destination, error %d", response->count, error);
		result = ISC_R_FAILURE;
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Build response record
 * param data Running data structure
 * param response Response data structure
 * param record Record buffer
 * param recsize Record buffer size
 */
static void ospdb_build_record(
	ospdb_data_t *data,
	ospdb_response_t *response,
	char *record,
	int recsize)
{
	const char *protocol;
	char userinfo[OSPDB_STR_SIZE];
	char parameters[OSPDB_STR_SIZE];
	int length, size;
	char *head;

	OSPDB_LOG_START;

	switch (response->protocol) {
	case OSPC_PROTNAME_Q931:
		protocol = OSPDB_PROTOCOL_H323;
		break;
	case OSPC_PROTNAME_SIP:
	default:
		protocol = OSPDB_PROTOCOL_SIP;
		break;
	}

	head = userinfo;
	size = sizeof(userinfo);
	length = snprintf(head, size, "%s", response->called);
	head += length;
	size -= length;
	if (response->nprn[0] != '\0') {
		length = snprintf(head, size, ";rn=%s", response->nprn);
		head += length;
		size -= length;
	}
	if (response->npcic[0]) {
		length = snprintf(head, size, ";cic=%s", response->npcic);
		head += length;
		size -= length;
	}
	if (response->npdi) {
		length = snprintf(head, size, ";npdi");
		head += length;
		size -= length;
	}
	if ((data->nidlocation == 1) && (response->dnid[0] != '\0')) {
		length = snprintf(head, size, ";%s=%s", data->nidname, response->dnid);
	}

	head = parameters;
	size = sizeof(parameters);
	head[0] = '\0';
	if ((data->nidlocation == 2) && (response->dnid[0] != '\0')) {
		length = snprintf(head, size, ";%s=%s", data->nidname, response->dnid);
		head += length;
		size -= length;
	}
	if (data->userphone == ISC_TRUE) {
		snprintf(head, size, ";user=phone");
	}

	snprintf(record, recsize, "%d %d \"U\" \"E2U+%s\" \"!^.*$!%s:%s@%s%s!\" .", response->count * 10, 0, protocol, protocol, userinfo, response->dest, parameters);

	OSPDB_LOG(ISC_LOG_DEBUG(2), "Record = '%s'", record);

	OSPDB_LOG_END;
}

/*
 * Lookup call back function
 */
#ifdef DNS_CLIENTINFO_VERSION
static isc_result_t ospdb_lookup(
	const char *zone,
	const char *name,
	void *dbdata,
	dns_sdblookup_t *lookup,
	dns_clientinfomethods_t *methods,
	dns_clientinfo_t *clientinfo)
#else
static isc_result_t ospdb_lookup(
	const char *zone,
	const char *name,
	void *dbdata,
	dns_sdblookup_t *lookup)
#endif /* DNS_CLIENTINFO_VERSION */
{
#ifdef DNS_CLIENTINFO_VERSION
	ns_client_t *client;
	isc_sockaddr_t *address;
	int length;
	char srcuribuf[OSPDB_STR_SIZE];
#endif /* DNS_CLIENTINFO_VERSION */
	ospdb_data_t *data = dbdata;
	int error = OSPC_ERR_NO_ERROR;
	OSPTTRANHANDLE transaction;
	ospdb_query_t query;
	char clientip[OSPDB_STR_SIZE];
	char srcuriuser[OSPDB_STR_SIZE];
	char srcurihost[OSPDB_STR_SIZE];
	unsigned int index;
	ospdb_response_t response;
	char record[OSPDB_STR_SIZE];
	isc_result_t result = ISC_R_SUCCESS;

	UNUSED(zone);
	UNUSED(lookup);

	OSPDB_LOG_START;

	if (strcmp(name, "@") == 0) {
		/* If authority() is not defined, issue RR for SOA and for NS here. */
		OSPDB_LOG(ISC_LOG_DEBUG(3), "%s", "lookup for '@'");
	} else if (strcmp(name, "ns") == 0) {
		/* For ns record */
		OSPDB_LOG(ISC_LOG_DEBUG(3), "%s", "lookup for 'ns'");
		result = dns_sdb_putrr(lookup, "A", 0, data->deviceip);
	} else if (ospdb_check_domain(name) != ISC_R_SUCCESS) {
		OSPDB_LOG(ISC_LOG_DEBUG(1), "Unsupported domain name '%s'", name);
		result = ISC_R_NOTFOUND;
	} else {
		if ((error = OSPPTransactionNew(data->provider, &transaction)) == OSPC_ERR_NO_ERROR) {
			/* Get domain name */
			query.name = name;

			/* Get DNS server address */
			query.serverip = data->deviceip;

			/* Get DNS client address and source URI info */
			clientip[0] = '\0';
			srcuriuser[0] = '\0';
			srcurihost[0] = '\0';
#ifdef DNS_CLIENTINFO_VERSION
			if ((methods != NULL) && ((methods->version - methods->age) >= DNS_CLIENTINFOMETHODS_VERSION)) {
				methods->sourceip(clientinfo, &address);
				if (getnameinfo(&address->type.sa, address->length, clientip, sizeof(clientip), NULL, 0, NI_NUMERICHOST) != 0) {
					clientip[0] = '\0';
				}
			}

			if ((data->usesrcuri == ISC_TRUE) && (clientinfo != NULL)) {
				client = (ns_client_t *)clientinfo->data;
				if ((client != NULL) && (client->urilen != 0)) {
					length = client->urilen < sizeof(srcuribuf) ? client->urilen : sizeof(srcuribuf) - 1;
					memmove(srcuribuf, client->uribuf, length);
					srcuribuf[length] = '\0';
					ospdb_parse_uri(srcuribuf, srcuriuser, sizeof(srcuriuser), srcurihost, sizeof(srcurihost));
				}
			}
#endif /* DNS_CLIENTINFO_VERSION */
			query.clientip = clientip;
			query.srcuriuser = srcuriuser;
			query.srcurihost = srcurihost;

			response.total = data->maxdest;
			if ((result = ospdb_request_auth(transaction, &query, &response.total)) == ISC_R_SUCCESS) {
				if ((result = ospdb_get_first(transaction, &response)) == ISC_R_SUCCESS) {
					ospdb_build_record(data, &response, record, sizeof(record));
					dns_sdb_putrr(lookup, "NAPTR", 0, record);

					for (index = 0; index < (response.total - 1); index++) {
						response.count = index + 2;
						if ((result = ospdb_get_next(transaction, &response)) == ISC_R_SUCCESS) {
							ospdb_build_record(data, &response, record, sizeof(record));
							dns_sdb_putrr(lookup, "NAPTR", 0, record);
						} else {
							break;
						}
					}
				}
			}

			if (transaction != OSPC_TRAN_HANDLE_INVALID) {
				OSPPTransactionDelete(transaction);
			}
		} else {
			OSPDB_LOG(ISC_LOG_ERROR, "Failed to create transaction, error %d", error);
			result = ISC_R_FAILURE;
		}
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Authority call back function
 */
static isc_result_t ospdb_authority(
	const char *zone,
	void *dbdata,
	dns_sdblookup_t *lookup)
{
	isc_result_t result;

	UNUSED(zone);
	UNUSED(dbdata);

	OSPDB_LOG_START;

	/* SOA record */
	result = dns_sdb_putsoa(lookup, "ns", "ospadmin", 0);
	INSIST(result == ISC_R_SUCCESS);

	/* NS record */
	result = dns_sdb_putrr(lookup, "NS", 86400, "ns");
	INSIST(result == ISC_R_SUCCESS);

	OSPDB_LOG_END;

	return (ISC_R_SUCCESS);
}

/*
 * Create call back function
 */
static isc_result_t ospdb_create(
	const char *zone,
	int argc,
	char **argv,
	void *driverdata,
	void **dbdata)
{
	ospdb_config_t cfg;
	ospdb_data_t *data;
	isc_result_t result = ISC_R_SUCCESS;

	UNUSED(zone);
	UNUSED(driverdata);

	OSPDB_LOG_START;

	/* Get running data structure */
	data = isc_mem_get(ns_g_mctx, sizeof(*data));
	if (data != NULL) {
		/* Get configuration parameters */
		ospdb_init_config(&cfg, data);
		ospdb_get_config(argc, argv, &cfg, data);
		ospdb_check_config(&cfg, data);
		ospdb_dump_config(&cfg, data);

		/* Create OSP provider */
		result = ospdb_create_provider(&cfg, &data->provider);
		if (result == ISC_R_SUCCESS) {
			*dbdata = data;
		} else {
			isc_mem_put(ns_g_mctx, data, sizeof(*data));
		}
	} else {
		OSPDB_LOG(ISC_LOG_ERROR, "%s", "Failed to get memory");
		result = ISC_R_NOMEMORY;
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * Destroy call back function
 */
static void ospdb_destroy(
	const char *zone,
	void *driverdata,
	void **dbdata)
{
	ospdb_data_t *data = *dbdata;

	UNUSED(zone);
	UNUSED(driverdata);

	OSPDB_LOG_START;

	/* Delete OSP provider */
	ospdb_delete_provider(data->provider);

	/* Free running data structure */
	isc_mem_put(ns_g_mctx, data, sizeof(*data));

	OSPDB_LOG_END;
}

/*
 * Call back function list structure
 */
static dns_sdbmethods_t ospdb_methods = {
	ospdb_lookup,
	ospdb_authority,
	NULL,				/* allnodes */
	ospdb_create,
	ospdb_destroy,
	NULL				/* lookup2 */
};

/*
 * OSP SDB driver init function
 */
isc_result_t ospdb_init(void)
{
	int error = OSPC_ERR_NO_ERROR;
	unsigned int flags = DNS_SDBFLAG_RELATIVEOWNER | DNS_SDBFLAG_RELATIVERDATA | DNS_SDBFLAG_THREADSAFE;
	isc_result_t result = ISC_R_FAILURE;

	OSPDB_LOG_START;

	/* Initialize OSP client with crypto hardware disabled */
	if ((error = OSPPInit(OSPC_FALSE)) == OSPC_ERR_NO_ERROR) {
		ospdb_init_flag = ISC_TRUE;

		/* Register OSP SDB driver */
		result = dns_sdb_register("osp", &ospdb_methods, NULL, flags, ns_g_mctx, &ospdb);
	} else {
		OSPDB_LOG(ISC_LOG_ERROR, "Failed to initialize OSP client, error '%d'", error);
	}

	OSPDB_LOG_END;

	return result;
}

/*
 * OSP SDB clear function
 */
void ospdb_clear(void)
{
	OSPDB_LOG_START;

	if (ospdb_init_flag == ISC_TRUE) {
		if (ospdb != NULL) {
			/* Unregister OSP SDB driver */
			dns_sdb_unregister(&ospdb);
		}

		/* Cleanup OSP client */
		OSPPCleanup();
	}

	OSPDB_LOG_END;
}

