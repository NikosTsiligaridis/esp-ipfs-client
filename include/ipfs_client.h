/******************************************************************************
 * @author Nikos Tsiligaridis
 * @brief Simple IPFS client for ESP32 IDF
 * @version 0.1.0
 * @date 2022-04-03
 * 
 * @copyright Copyright (c) 2022
******************************************************************************/
#ifndef IPFS_H
#define IPFS_H

class IPFSClient
{
public:
    /**
     * Return codes
     */
    enum Result
    {
        IPFS_CLIENT_OK,
        IPFS_CLIENT_CANNOT_CONNECT,
        IPFS_CLIENT_INVALID_ADDRESS,
        IPFS_CLIENT_REQUEST_FAILED,
        IPFS_CLIENT_INVALID_RESPONSE,
        IPFS_CLIENT_INVALID_INPUT
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

    void set_buffer(char *buff, int size);
    Result set_addr(char *addr);
    void set_req_timeout(uint32_t ms);
    void set_basic_auth_creds_base64(const char *creds);
    void set_basic_auth_creds(char *user, char *pass);

    Result add(IPFSFile *file_out, char *filename, char *content);
private:
    Result parse_url(char *url);

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

    static const char API_PATH[];
};

#endif