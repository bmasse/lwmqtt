#ifndef __OpenSSL_API_h__
#define __OpenSSL_API_h__

#include <functional>

/***
 * Note: AP's OpenSSL Version is # define OPENSSL_VERSION_NUMBER  0x1000213fL (2022-02-14)
 *
 *
 ***/

#include <stdint.h>

#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ui.h>
#include <openssl/x509v3.h>

extern "C"
{

    const size_t TlsData_MaxTextLen = 256;
    typedef struct TlsData_s
    {
        char host[TlsData_MaxTextLen];
        uint16_t port;
        int socket;

        char tls_cafile[TlsData_MaxTextLen];
        char tls_capath[TlsData_MaxTextLen];
        char tls_certfile[TlsData_MaxTextLen];
        char tls_keyfile[TlsData_MaxTextLen];
        char tls_version[TlsData_MaxTextLen];
        char tls_ciphers[TlsData_MaxTextLen];
        char tls_alpn[TlsData_MaxTextLen];
        int tls_cert_reqs;
        bool tls_insecure;
        bool ssl_ctx_defaults;
        bool tls_ocsp_required;
        bool tls_use_os_certs;
        bool tls_connected;

    } TlsData_S;

} // extern "C" {

typedef std::function<int(int preverify_ok, X509_STORE_CTX *ctx)> serverCertificateVerifyCallbackFunc;

class OpenSSL_API
{
public:
    enum OpenSSL_API_Msg_E
    {
        Msg_Err_Read = -5,
        Msg_Err_Write = -4,
        Msg_Err_Peek = -3,
        Msg_Err_Tls = -2,
        Msg_Error = -1,
        Msg_Success = 0
    };
    OpenSSL_API(TlsData_S *data);
    ~OpenSSL_API() { Close(); };

    int Init();

    bool IsInitialized() { return m_initialized; }
    SSL *GetSsl() { return m_ssl; }
    SSL *m_ssl;
    void Close();
    OpenSSL_API_Msg_E Read(uint8_t *buffer, size_t len, size_t *read, uint32_t timeout);
    OpenSSL_API_Msg_E Write(uint8_t *buffer, size_t len, size_t *read, uint32_t timeout);
    OpenSSL_API_Msg_E Peek(size_t *available);

private:
    int InitSslCtx();
    void ResetInitialized() { m_initialized = false; }
    void SetInitialized() { m_initialized = true; }
    void InitTlsCrypto();
    void InitTlsCryptoVersion();
    int LoadCA();
    void SetSSLCtx();
    void SetALPN();
    int Certificats();
    void PrintTlsError();
    void SetOpensslExIndex();
    void SslClose();
    int SslConnect();
    int ServerCertificateVerifyCallback(int preverify_ok, X509_STORE_CTX *ctx);
    int CertificateHostNameVeriry(X509 *cert, const char *hostname);
    bool WildcardName(char *certname, const char *hostname);
    bool HandleSslError(int ret);
    void PrintSslError(int err);

    OpenSSL_API_Msg_E SSL_Pending(size_t *available);
    OpenSSL_API_Msg_E GetFionRead(size_t *available);

    TlsData_S *m_tls_data;

    SSL_CTX *m_ssl_ctx;
    SSL_CTX *m_user_ssl_ctx;

    // OpenSSL index
    int m_openssl_ex_index;

    // Init or Not
    bool m_initialized;

    bool m_want_connect;

    serverCertificateVerifyCallbackFunc mServerCertificateVerifyCallbackFunc;
};

extern "C"
{

#include "lwmqtt.h"

} // extern "C"

#endif // #ifndef __OpenSSL_API_h__