#include "Socket.h"
#include "OpenSSL_API.h"

#include <iostream>
#include <string>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ui.h>
#include <openssl/x509v3.h>

#include <unistd.h>
#include <sys/ioctl.h>

#if AP
#include <aruba/util/grouplog_cloudconnect.h>
#else
#include "config.h"
#endif


int g_tls_ex_index_mosq = -1;
void SetGlobalOpensslExIndex(int index)
{
    g_tls_ex_index_mosq = index;
}
int GetGlobalOpensslExIndex(void)
{
    return g_tls_ex_index_mosq;
}

int openssl__server_certificate_verify(int preverify_ok, X509_STORE_CTX *ctx)
{
    /* Preverify should have already checked expiry, revocation.
     * We need to verify the hostname. */
    SSL *ssl;
    void *ref;
    /* Always reject if preverify_ok has failed. */
    if (!preverify_ok)
        return 0;

    ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    ref = (void *)SSL_get_ex_data(ssl, GetGlobalOpensslExIndex());

    auto &callback = *reinterpret_cast<serverCertificateVerifyCallbackFunc *>(ref);
    return callback(preverify_ok, ctx);
}

void OpenSSL_API::PrintSslError(int err)
{
    char ebuf[256];
    unsigned long errNum;
    int num = 0;
    if (err != 0)
        GLERROR_MQTTCLIENT("OpenSSL (err=%d) Error[%d]: %s", err, num, ERR_error_string(err, ebuf));

    errNum = ERR_get_error();
    while (errNum)
    {
        GLERROR_MQTTCLIENT("OpenSSL (errNum=%ld) Error[%d]: %s", errNum, num, ERR_error_string(errNum, ebuf));
        errNum = ERR_get_error();
        num++;
    }
}


bool OpenSSL_API::HandleSslError(int ret)
{
    bool retVal = false;
    int err;

    err = SSL_get_error(m_ssl, ret);

    switch (err)
    {
    case SSL_ERROR_WANT_READ:
    {
        errno = EAGAIN;
        retVal = true;
    }
    break;

    case SSL_ERROR_WANT_WRITE:
    {
        errno = EAGAIN;
        retVal = true;
    }
    break;
    case SSL_ERROR_ZERO_RETURN:
    case SSL_ERROR_SYSCALL:
    {
        PrintSslError(err);
        errno = ECONNRESET;
    }
    break;
    default:
    {
        PrintSslError(err);
        errno = EPROTO;
    }
    break;
    }

    ERR_clear_error();

    return retVal;
}

OpenSSL_API::OpenSSL_API_Msg_E OpenSSL_API::GetFionRead(size_t *available)
{
    int iocAvail;
    int rc = ioctl(m_tls_data->socket, FIONREAD, &iocAvail);
    if (rc < 0)
    {
        *available = 0;
        GLERROR_MQTTCLIENT("LWMQTT_NETWORK_FAILED_READ");
        return OpenSSL_API::Msg_Err_Peek;
    }
    *available = iocAvail;

    return OpenSSL_API::Msg_Success;
}

OpenSSL_API::OpenSSL_API_Msg_E OpenSSL_API::SSL_Pending(size_t *available)
{
    ssize_t byteToRead;
    byteToRead = SSL_pending(m_ssl);
    if (byteToRead < 0)
    {
        *available = 0;
        return OpenSSL_API::Msg_Err_Peek;
    }
    *available = byteToRead;
    return OpenSSL_API::Msg_Success;
}

OpenSSL_API::OpenSSL_API_Msg_E OpenSSL_API::Peek(size_t *available)
{
    OpenSSL_API_Msg_E rc = Msg_Success;
    rc = SSL_Pending(available);
    if (*available > 0)
        return rc;
    rc = GetFionRead(available);
    if (rc == Msg_Success)
    {
        if (*available)
            *available = 1;
    }
    return rc;
}

OpenSSL_API::OpenSSL_API_Msg_E OpenSSL_API::Read(uint8_t *buffer, size_t len, size_t *read, uint32_t timeout)
{
    OpenSSL_API_Msg_E err = Msg_Err_Read;
    ssize_t ret;
    ERR_clear_error();
    if (m_ssl)
    {
        ret = SSL_read(m_ssl, buffer, len);
        if (ret <= 0)
        {
            *read = 0;
            if (HandleSslError(ret))
            {
                err = Msg_Success;
                GLTRACE_MQTTCLIENT("HandleSslError:SSL_read() len = %lu, read = %lu, timeout = %u", len, *read, timeout);
            }
            else
            {
                err = Msg_Err_Read;
                GLERROR_MQTTCLIENT("Msg_Err_Read:SSL_read() len = %lu, read = %lu, timeout = %u", len, *read, timeout);
            }
        }
        else
        {
            err = Msg_Success;
            *read = (size_t)ret;
            GLTRACE_MQTTCLIENT("SSL_read() len = %lu, read = %lu, timeout = %u", len, *read, timeout);
        }
    }
    return err;
}

OpenSSL_API::OpenSSL_API_Msg_E OpenSSL_API::Write(uint8_t *buffer, size_t len, size_t *sent, uint32_t timeout)
{
    OpenSSL_API_Msg_E err = Msg_Err_Write;
    ssize_t ret;
    ERR_clear_error();
    if (m_ssl)
    {
        ret = SSL_write(m_ssl, buffer, len);
        if (ret <= 0)
        {
            *sent = 0;
            if (HandleSslError(ret))
            {
                err = Msg_Success;
                GLTRACE_MQTTCLIENT("HandleSslError:SSL_write() len = %lu, sent = %lu, timeout = %u", len, *sent, timeout);
            }
            else
            {
                err = Msg_Err_Write;
                GLERROR_MQTTCLIENT("Msg_Err_Write:SSL_write() len = %lu, sent = %lu, timeout = %u", len, *sent, timeout);
            }
        }
        else
        {
             err = Msg_Success;
            *sent = (size_t)ret;
            GLTRACE_MQTTCLIENT("SSL_write() len = %lu, sent = %lu, timeout = %u", len, *sent, timeout);
        }
    }
    else
    {
        GLERROR_MQTTCLIENT("SSL_write() m_ssl is NULL");
    }

    return err;
}

bool OpenSSL_API::WildcardName(char *certname, const char *hostname)
{
    if (!certname || !hostname)
    {
        return 1;
    }
    GLDEBUG_MQTTCLIENT("certname: %s and hosname %s", certname, hostname);
    std::string cert(certname);
    std::string host(hostname);
    if (certname[0] == '*')
    {
        cert.erase(cert.begin());
        size_t pos = host.find(cert);
        host.erase(0, pos);
    }
    return cert == host;
}

int OpenSSL_API::CertificateHostNameVeriry(X509 *cert, const char *hostname)
{
    int i;
    char name[256];
    X509_NAME *subj;
    bool have_san_dns = false;
    STACK_OF(GENERAL_NAME) * san;
    const GENERAL_NAME *nval;
    const unsigned char *data;
    unsigned char ipv6_addr[16];
    unsigned char ipv4_addr[4];
    int ipv6_ok;
    int ipv4_ok;

    ipv6_ok = inet_pton(AF_INET6, hostname, &ipv6_addr);
    ipv4_ok = inet_pton(AF_INET, hostname, &ipv4_addr);
    san = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    if (san)
    {
        for (i = 0; i < sk_GENERAL_NAME_num(san); i++)
        {
            nval = sk_GENERAL_NAME_value(san, i);
            if (nval->type == GEN_DNS)
            {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
                data = ASN1_STRING_data(nval->d.dNSName);
#else
                data = ASN1_STRING_get0_data(nval->d.dNSName);
#endif
                if (data && WildcardName((char *)data, hostname))
                {
                    sk_GENERAL_NAME_pop_free(san, GENERAL_NAME_free);
                    return 1;
                }
                have_san_dns = true;
            }
            else if (nval->type == GEN_IPADD)
            {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
                data = ASN1_STRING_data(nval->d.iPAddress);
#else
                data = ASN1_STRING_get0_data(nval->d.iPAddress);
#endif
                if (nval->d.iPAddress->length == 4 && ipv4_ok)
                {
                    if (!memcmp(ipv4_addr, data, 4))
                    {
                        sk_GENERAL_NAME_pop_free(san, GENERAL_NAME_free);
                        return 1;
                    }
                }
                else if (nval->d.iPAddress->length == 16 && ipv6_ok)
                {
                    if (!memcmp(ipv6_addr, data, 16))
                    {
                        sk_GENERAL_NAME_pop_free(san, GENERAL_NAME_free);
                        return 1;
                    }
                }
            }
        }
        sk_GENERAL_NAME_pop_free(san, GENERAL_NAME_free);
        if (have_san_dns)
        {
            /* Only check CN if subjectAltName DNS entry does not exist. */
            return 0;
        }
    }

    subj = X509_get_subject_name(cert);
    if (X509_NAME_get_text_by_NID(subj, NID_commonName, name, sizeof(name)) > 0)
    {
        name[sizeof(name) - 1] = '\0';
        if (WildcardName(name, hostname))
            return 1;
    }
    return 0;
}

int OpenSSL_API::ServerCertificateVerifyCallback(int preverify_ok, X509_STORE_CTX *ctx)
{
    X509 *cert;

    if (X509_STORE_CTX_get_error_depth(ctx) == 0)
    {
        cert = X509_STORE_CTX_get_current_cert(ctx);
        preverify_ok = CertificateHostNameVeriry(cert, m_tls_data->host);
        if (preverify_ok != 1)
        {
            GLERROR_MQTTCLIENT("Error: host name verification failed.");
        }
    }
    GLINFO_MQTTCLIENT("Server Certificate Verified, preveriry_ok = %d, 1 successfull, 0 failed", preverify_ok);
    return preverify_ok;
}

void OpenSSL_API::InitTlsCryptoVersion(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
#else
    OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS | OPENSSL_INIT_LOAD_CONFIG, NULL);
#endif
}

void OpenSSL_API::SetOpensslExIndex()
{
    if (m_openssl_ex_index == -1)
    {
        m_openssl_ex_index = SSL_get_ex_new_index(0, (void *)"client context", NULL, NULL, NULL);
        SetGlobalOpensslExIndex(m_openssl_ex_index);
    }
}

void OpenSSL_API::InitTlsCrypto(void)
{
    if (IsInitialized())
        return;

    InitTlsCryptoVersion();
    SetOpensslExIndex();
    SetInitialized();
}

void OpenSSL_API::PrintTlsError(void)
{
    char ebuf[256];
    unsigned long e;
    int num = 0;

    e = ERR_get_error();
    while (e)
    {
        GLERROR_MQTTCLIENT("OpenSSL Error[%d]: %s", num, ERR_error_string(e, ebuf));
        e = ERR_get_error();
        num++;
    }
}

int OpenSSL_API::LoadCA()
{
    int ret;
    if (m_tls_data->tls_use_os_certs)
    {
        SSL_CTX_set_default_verify_paths(m_ssl_ctx);
    }
    if (m_tls_data->tls_cafile || m_tls_data->tls_capath)
    {
        ret = SSL_CTX_load_verify_locations(m_ssl_ctx, m_tls_data->tls_cafile, nullptr);
        if (ret == 0)
        {
            if (m_tls_data->tls_cafile && m_tls_data->tls_capath)
            {
                GLERROR_MQTTCLIENT("Error: Unable to load CA certificates, check cafile \"%s\" and capath \"%s\".", m_tls_data->tls_cafile, m_tls_data->tls_capath);
            }
            else if (m_tls_data->tls_cafile)
            {
                GLERROR_MQTTCLIENT("Error: Unable to load CA certificates, check cafile \"%s\".", m_tls_data->tls_cafile);
            }
            else
            {
                GLERROR_MQTTCLIENT("Error: Unable to load CA certificates, check capath \"%s\".", m_tls_data->tls_capath);
            }
            return Msg_Err_Tls;
        }
    }
    return Msg_Success;
}

int OpenSSL_API::Certificats()
{
    int ret;
    if (m_tls_data->tls_cafile || m_tls_data->tls_capath || m_tls_data->tls_use_os_certs)
    {
        ret = LoadCA();
        if (ret != Msg_Success)
        {
            PrintTlsError();
            return Msg_Err_Tls;
        }
        if (m_tls_data->tls_cert_reqs == 0)
        {
            SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_NONE, 0);
        }
        else
        {
            SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_PEER, openssl__server_certificate_verify);
        }
        if (m_tls_data->tls_certfile)
        {
            ret = SSL_CTX_use_certificate_chain_file(m_ssl_ctx, m_tls_data->tls_certfile);
            if (ret != 1)
            {
                GLERROR_MQTTCLIENT("Error: Unable to load client certificate \"%s\".", m_tls_data->tls_certfile);
                PrintTlsError();
                return Msg_Err_Tls;
            }
        }
        if (m_tls_data->tls_keyfile)
        {
            ret = SSL_CTX_use_PrivateKey_file(m_ssl_ctx, m_tls_data->tls_keyfile, SSL_FILETYPE_PEM);
            if (ret != 1)
            {
                GLERROR_MQTTCLIENT("Error: Unable to load client key file \"%s\".", m_tls_data->tls_keyfile);
                PrintTlsError();
                return Msg_Err_Tls;
            }
            ret = SSL_CTX_check_private_key(m_ssl_ctx);
            if (ret != 1)
            {
                GLERROR_MQTTCLIENT("Error: Client certificate/key are inconsistent.");
                PrintTlsError();
                return Msg_Err_Tls;
            }
        }
    }
    return Msg_Success;
}

void OpenSSL_API::SetSSLCtx()
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    m_ssl_ctx = SSL_CTX_new(SSLv23_client_method());
#else
    m_ssl_ctx = SSL_CTX_new(TLS_client_method());
#endif
}

void OpenSSL_API::SetALPN()
{
    uint8_t tls_alpn_wire[256];
    uint8_t tls_alpn_len;

    /* Set ALPN */
    if (m_tls_data->tls_alpn)
    {
        tls_alpn_len = (uint8_t)strnlen(m_tls_data->tls_alpn, 254);
        tls_alpn_wire[0] = tls_alpn_len; /* first byte is length of string */
        memcpy(tls_alpn_wire + 1, m_tls_data->tls_alpn, tls_alpn_len);
        SSL_CTX_set_alpn_protos(m_ssl_ctx, tls_alpn_wire, tls_alpn_len + 1U);
    }
}

/**
 * @brief Initialize TLSConnection
 *
 *
 * @return int
 */
int OpenSSL_API::InitSslCtx()
{
    int ret = Msg_Success;

    if (m_tls_data->tls_cafile || m_tls_data->tls_capath || m_tls_data->tls_use_os_certs)
    {
        if (!m_ssl_ctx)
        {
            InitTlsCrypto();

            SetSSLCtx();

            if (!m_ssl_ctx)
            {
                GLERROR_MQTTCLIENT("Msg_Err_Tls");
                PrintTlsError();
                ret = Msg_Err_Tls;
            }
        }
        if (ret == Msg_Success)
        {
            SSL_CTX_set_options(m_ssl_ctx, SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);

            /* Disable compression */
            SSL_CTX_set_options(m_ssl_ctx, SSL_OP_NO_COMPRESSION);

            SetALPN();

            /* Use even less memory per SSL connection. */
            SSL_CTX_set_mode(m_ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

            ret = Certificats();
        }
    }
    return ret;
}

void OpenSSL_API::SslClose()
{
    GLTRACE_MQTTCLIENT("SslClose.");
    if (m_ssl)
    {
        if (!SSL_in_init(m_ssl))
        {
            GLERROR_MQTTCLIENT("Error: SSL_shutdown.");
            SSL_shutdown(m_ssl);
        }
        GLTRACE_MQTTCLIENT("Error: SSL_free.");
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
}

void OpenSSL_API::Close()
{
    m_tls_data->tls_connected = false;
    GLTRACE_MQTTCLIENT("OpenSSL_API Close.");
    SslClose();
    if (m_ssl_ctx)
    {
        SSL_CTX_free(m_ssl_ctx);
        GLERROR_MQTTCLIENT("Error: ssl_ctx free.");
    }
    m_initialized = false;
    m_openssl_ex_index = -1;
    m_ssl_ctx = nullptr;
    m_ssl = nullptr;
}

int OpenSSL_API::SslConnect()
{
    int ret, err;

    ERR_clear_error();

    ret = SSL_connect(m_ssl);
    if (ret != 1)
    {
        err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ)
        {
            GLTRACE_MQTTCLIENT("Warning: SSL_ERROR_WANT_READ.");
            m_want_connect = true;
        }
        else if (err == SSL_ERROR_WANT_WRITE)
        {
            GLTRACE_MQTTCLIENT("Warning: SSL_ERROR_WANT_WRITE.");
            m_want_connect = true;
        }
        else
        {
            PrintTlsError();
            GLERROR_MQTTCLIENT("Error: ssl connect failed.");
            return Msg_Err_Tls;
        }
    }
    else
    {
        GLDEBUG_MQTTCLIENT("SSL Connected: m_want_connect = false.");
        m_want_connect = false;
    }
    return Msg_Success;
}

int OpenSSL_API::Init()
{
    BIO *bio;

    // Create a function object encapsulating the server certificate verify callback.  Pointer
    // to this object is passed to the c-callback wrapper.
    mServerCertificateVerifyCallbackFunc = std::bind(&OpenSSL_API::ServerCertificateVerifyCallback,
                                                     this,
                                                     std::placeholders::_1,
                                                     std::placeholders::_2);

    int rc = InitSslCtx();
    if (rc)
    {
        Close();
        return Msg_Err_Tls;
    }

    if (m_ssl_ctx)
    {
        if (m_ssl)
        {
            SSL_free(m_ssl);
        }
        m_ssl = SSL_new(m_ssl_ctx);
        if (!m_ssl)
        {
            Close();
            return Msg_Err_Tls;
        }

        SSL_set_ex_data(m_ssl, m_openssl_ex_index, &mServerCertificateVerifyCallbackFunc);
        bio = BIO_new_socket(m_tls_data->socket, BIO_NOCLOSE);
        if (!bio)
        {
            Close();
            PrintTlsError();
            GLERROR_MQTTCLIENT("Msg_Err_Tls");
            return Msg_Err_Tls;
        }
        SSL_set_bio(m_ssl, bio, bio);

        // Required for the SNI resolving
        if (SSL_set_tlsext_host_name(m_ssl, m_tls_data->host) != 1)
        {
            Close();
            GLERROR_MQTTCLIENT("Msg_Err_Tls");
            return Msg_Err_Tls;
        }
        const __useconds_t timeoutLim = 500 * 1000; // 500 ms
        const __useconds_t timeout = 100 * 1000; // 100 ms
        __useconds_t timeCount = timeoutLim/timeout;
        timeCount = timeCount ? timeCount : 1; // timeCount must be at least 1.
        do
        {
            if (SslConnect())
            {
                Close();
                GLERROR_MQTTCLIENT("Msg_Err_Tls");
                return Msg_Err_Tls;
            }
            GLTRACE_MQTTCLIENT("timeCount = %d ", timeCount);
            usleep(timeout);
        } while (m_want_connect && timeCount--);
        if ( m_want_connect )
        {
            Close();
            GLERROR_MQTTCLIENT("Msg_Err_Tls, timeout");
            return Msg_Err_Tls;
        }
    }
    m_tls_data->tls_connected = true;
    return Msg_Success;
}

OpenSSL_API::OpenSSL_API(TlsData_S *data)
{
    m_initialized = false;
    m_openssl_ex_index = -1;
    m_ssl_ctx = nullptr;
    m_ssl = nullptr;
    m_tls_data = data;
}
