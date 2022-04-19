/******************************************************************************
 * @author Nikos Tsiligaridis
 * @brief Simple IPFS client for ESP32 IDF
 * @version 0.1.1
 * @date 2022-04-03
 * 
 * @copyright Copyright (c) 2022
******************************************************************************/
#ifndef IPFS_H
#define IPFS_H

#include "esp_tls.h"
#include "http_parser.h"

class IPFSClient
{
public:
    /**
     * Return codes
     */
    enum Result
    {
        IPFS_CLIENT_OK,
        IPFS_CLIENT_NOT_CONNECTED,
        IPFS_CLIENT_CANNOT_CONNECT,
        IPFS_CLIENT_INVALID_ADDRESS,
        IPFS_CLIENT_REQUEST_FAILED,
        IPFS_CLIENT_INVALID_RESPONSE,
        IPFS_CLIENT_INVALID_INPUT,
        IPFS_CLIENT_INVALID_STATE
    };

    /**
     * File descriptor
     */
    struct IPFSFile
    {
        char name[255];
        char cid[50];
        uint32_t size;
    };

    // IPFSClient(char *buff, int size);
    IPFSClient(){};
    ~IPFSClient(){};

    Result connect();
    Result disconnect();
    bool is_connected();
    esp_tls_conn_state get_status();

    void set_buffer(char *buff, int size);
    Result set_addr(const char *addr);
    void set_req_timeout(uint32_t ms);
    void set_basic_auth_creds_base64(const char *creds);
    void set_basic_auth_creds(const char *user, const char *pass);

    Result add(IPFSFile *file_out, const char *filename, const char *content);
private:
    Result parse_url(const char *url);

    /** IPFS address */
    char _addr[60] = "";

    /** IPFS port */
    int _port = 0;

    /** Timeout for all requests */
    int _timeout_ms = 10000;

    /** HTTP parser struct */
    http_parser_url _parser_url;

    /** Request/response buffer */
    char *_buffer = nullptr;
    int _buffer_size = 0;

    /** Basic auth creds */
    char _basic_auth_creds_base64[100] = "";

    /** ESP TLS connection handle */
    esp_tls *_tls_conn = NULL;

    static const char API_PATH[];
};

#endif