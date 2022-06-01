#ifndef __DAEMON_CONFIG_CFG_H__
#define __DAEMON_CONFIG_CFG_H__

#include <string>

using std::string;

#define DEFAULT_CA_CERT_PATH "/aruba/conf/AmazonRootCA.pem"
#define DEFAULT_ONBOARDING_CA_CERT_PATH "/aruba/conf/smb_ca_certificate.pem"

/**
 * @brief Daemon configuration.
 */
struct DaemonConfig {
    int wsServerPort; /**< WebSocket server port to listen on. */
    int syslogServerPort; /**< Syslog server port to listen on. */
    string mqttHost; /**< Host of the MQTT broker to connect to. */
    int mqttHostPort; /**< Port of the MQTT broker to connect to. */
    bool mqttHostCertValidation; /**< Indicates if server certificate should be validated. */
    string deviceCert; /**< Path to the location of the device certificate file. */
    string deviceKey; /**< Path to the location of the device private key file. */
    string caCert; /**< Path to the CA certificate file to use to connect to the MQTT broker. */
    string onboardingCaCert; /**< Path to the CA certificate file used to connect to the onboarding service. */
    bool debug; /**< whether or not debug logging is enabled */
    bool toStdout; /**< whether or not debug logging to stdout is enabled */
    bool forceMqttConnStart; /**< whether or not MQTT connection should be started immediately */
};

#endif // __DAEMON_CONFIG_CFG_H__
