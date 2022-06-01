#include <bits/getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "getopt.h"

static const char* const short_options = "o:m:nc:k:a:b:p:l:dsfh";
static struct option long_options[] = {
    { "mqtt-host", required_argument, NULL, 'o' },
    { "mqtt-port", required_argument, NULL, 'm' },
    { "no-mqtt-cert-validation", no_argument, NULL, 'n' },
    { "device-cert", required_argument, NULL, 'c' },
    { "device-key", required_argument, NULL, 'k' },
    { "ca-cert", required_argument, NULL, 'a' },
    { "onboarding-ca-cert", required_argument, NULL, 'b' },
    { "ws-port", required_argument, NULL, 'p' },
    { "ss-port", required_argument, NULL, 'l' },
    { "debug", no_argument, NULL, 'd' },
    { "to-stdout", no_argument, NULL, 's' },
    { "force-mqtt-conn-start", no_argument, NULL, 'f' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

static void usage(void)
{
    fprintf(stderr, "Usage: cloud_connect [OPTIONS...]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o, --mqtt-host HOST           Hostname of the MQTT endpoint to connect to.\n");
    fprintf(stderr, "  -m, --mqtt-port PORT           Port of the MQTT host to connect to (default: 443).\n");
    fprintf(stderr, "  -n, --no-mqtt-cert-validation  Disable certificate validation of the MQTT endpoint.\n");
    fprintf(stderr, "  -c, --device-cert PATH         Path to the location of the device certitifcate.\n");
    fprintf(stderr, "  -k, --device-key PATH          Path to the location of the device private key.\n");
    fprintf(stderr, "  -a, --ca-cert PATH             Path to the location of the CA certificate used to connect to the MQTT endpoint (default: %s).\n", DEFAULT_CA_CERT_PATH);
    fprintf(stderr, "  -b, --onboarding-ca-cert PATH  Path to the location of the CA certificate used to connect to onboarding service (default: %s).\n", DEFAULT_ONBOARDING_CA_CERT_PATH);
    fprintf(stderr, "  -p, --ws-port PORT             Port to listen for WebSocket connections (default: 8080).\n");
    fprintf(stderr, "  -l, --ss-port PORT             Port to listen for Syslog messages (default: none).\n");
    fprintf(stderr, "  -d, --debug                    Enable debug logging.\n");
    fprintf(stderr, "  -s, --to-stdout                Enable logging on the standard output.\n");
    fprintf(stderr, "  -f, --force-mqtt-conn-start    Force the start of the MQTT connection.\n");
    fprintf(stderr, "  -h, --help                     Display this help and exit.\n");
}

int getopt_init(int argc, char **argv, DaemonConfig *cfg)
{
    int n = 0;
    if (argc < 1)
    {
        usage();
        return -1;
    }

    while(n >= 0)
    {
        n = getopt_long(argc, argv, short_options, long_options, NULL);
        if (n < 0)
        {
             continue;
        }
        switch(n)
        {
            case 'o':
                cfg->mqttHost = string(optarg);
                break;
            case 'm':
                cfg->mqttHostPort = atoi(optarg);
                break;
            case 'n':
                cfg->mqttHostCertValidation = false;
                break;
            case 'c':
                cfg->deviceCert = string(optarg);
                break;
            case 'k':
                cfg->deviceKey = string(optarg);
                break;
            case 'a':
                cfg->caCert = string(optarg);
                break;
            case 'b':
                cfg->onboardingCaCert = string(optarg);
                break;
            case 'p':
                cfg->wsServerPort = atoi(optarg);
                break;
            case 'l':
                cfg->syslogServerPort = atoi(optarg);
                break;
            case 'd':
                cfg->debug = true;
                break;
            case 's':
                cfg->toStdout = true;
                break;
            case 'f':
                cfg->forceMqttConnStart = true;
                break;
            case 'h':
            case '?':
                usage();
                return -1;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        usage();
        return -1;
    }

    return 0;
}

