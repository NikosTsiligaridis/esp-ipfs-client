/******************************************************************************
 * @author Nikos Tsiligaridis
 * @brief Simple IPFS client for ESP32 IDF with TLS support
 * @version 0.1.1
 * @date 2022-04-03
 * 
 * @copyright Copyright (c) 2022
******************************************************************************/
#include "string.h"
#include <inttypes.h>
#include "esp_log.h"
#include "ArduinoJson.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "http_parser.h"
#include "mbedtls/base64.h"
#include "ipfs_client.h"

const char TAG[] = "IPFSClient";

// API root
#define API_PATH ""

// Write to socket, on failure return error
#define ASSERT_WRITE(val) do { \
	if(esp_tls_conn_write(_tls_conn, val, strlen(val)) < 0) { \
        ESP_LOGE(TAG, "Req. write failed"); \
        return IPFS_CLIENT_REQUEST_FAILED; \
    } \
}while(0);\

//
// Constants
//
// Used both in user agent headers and as multipart boundary
const char USER_AGENT[] = "ESP32_IPFS_Client";

/******************************************************************************
* Connect to node
* @param tls_cfg    ESP-TLS config struct. NULL for non-TLS connection. Struct
*                   will be copied
* @return 
*	- IPFS_CLIENT_INVALID_STATE Already connected
******************************************************************************/
IPFSClient::Result IPFSClient::connect()
{
	if(is_connected())
	{
		ESP_LOGI(TAG, "Already connected.");
		return IPFSClient::IPFS_CLIENT_INVALID_STATE;
	}

	auto status = get_status();
	if(status != esp_tls_conn_state::ESP_TLS_FAIL &&
		status != esp_tls_conn_state::ESP_TLS_INIT)
	{
		ESP_LOGI(TAG, "Already connected.");
		return IPFSClient::IPFS_CLIENT_INVALID_STATE;
	}

	ESP_LOGW(TAG, "Connecting to %s", _addr);

	_tls_conn = esp_tls_conn_http_new(_addr, &_tls_cfg);
	if(_tls_conn == NULL)
	{
		ESP_LOGE(TAG, "Could not open TLS connection.");
		return IPFSClient::IPFS_CLIENT_CANNOT_CONNECT;
	}

	return IPFS_CLIENT_OK;
}

/******************************************************************************
* Disconnect from node and cleanup
******************************************************************************/
IPFSClient::Result IPFSClient::disconnect()
{
	if(_tls_conn == NULL)
		return Result::IPFS_CLIENT_NOT_CONNECTED;

	esp_tls_conn_destroy(_tls_conn);

	_tls_conn = NULL;

	return IPFS_CLIENT_OK;
}

/******************************************************************************
* Is client connected
******************************************************************************/
bool IPFSClient::is_connected()
{
	if(_tls_conn == NULL)
		return false;

	if(get_status() == esp_tls_conn_state::ESP_TLS_DONE)
		return true;

	return true;
}

/******************************************************************************
* Get connection status
******************************************************************************/
esp_tls_conn_state IPFSClient::get_status()
{
	if(_tls_conn == NULL)
	{
		return esp_tls_conn_state::ESP_TLS_INIT;
	}

	return _tls_conn->conn_state;
}

/******************************************************************************
* Set node address
* @param addr IPFS node address
******************************************************************************/
IPFSClient::Result IPFSClient::set_addr(const char *addr)
{
    http_parser_url_init(&_parser_url);
    if(parse_url(addr) != IPFS_CLIENT_OK)
    {
        return IPFS_CLIENT_INVALID_ADDRESS;
    }

    strncpy(_addr, addr, sizeof(_addr));

    // Get port
    if(_parser_url.field_data[UF_PORT].len)
    {
        _port = strtol(&addr[_parser_url.field_data[UF_PORT].off], NULL, 10);
    }
    else
    {
        if (strncasecmp(&addr[_parser_url.field_data[UF_SCHEMA].off], "http", _parser_url.field_data[UF_SCHEMA].len) == 0)
        {
            _port = 80;
        }
        else if (strncasecmp(&addr[_parser_url.field_data[UF_SCHEMA].off], "https", _parser_url.field_data[UF_SCHEMA].len) == 0)
        {
            _port = 443;
        }
    }

    return IPFS_CLIENT_OK;
}

/******************************************************************************
* Parse URL into pars
* @param url URL to parse
* @return
*   - IPFS_CLIENT_OK Sucess
*   - IPFS_CLIENT_INVALID_ADDRESS Invalid URL provided
******************************************************************************/
IPFSClient::Result IPFSClient::parse_url(const char *url)
{
    http_parser_url_init(&_parser_url);
    
    if(http_parser_parse_url(url, strlen(url), 0, &_parser_url) != 0)
    {
        ESP_LOGE(TAG, "Could not parse URL: %s", url);
        return IPFSClient::IPFS_CLIENT_INVALID_ADDRESS;
    }

    return IPFSClient::IPFS_CLIENT_OK;
}

/******************************************************************************
* Add a plain text file to IPFS
* @param	file_out	Parsed IPFS response (output)
* @param	filename	Filename to submit to IFPS
* @param	data		Data
* @return	Result struct
******************************************************************************/
IPFSClient::Result IPFSClient::add(IPFSFile *file_out, const char *filename, const char *content)
{
	if(!is_connected())
    {
		return IPFS_CLIENT_NOT_CONNECTED;
    }

	if(_buffer == nullptr)
    {
		return IPFS_CLIENT_INVALID_INPUT;
    }

    //
    // Write request
    //
    _buffer[0] = '\0';
    ASSERT_WRITE("POST " API_PATH "/add HTTP/1.0\r\n");

    snprintf(_buffer, _buffer_size, "Host: %.*s:%d\r\n", 
        _parser_url.field_data[UF_HOST].len, (char*)(&_addr[_parser_url.field_data[UF_HOST].off]),
        _port);
    ASSERT_WRITE(_buffer);

    snprintf(_buffer, _buffer_size, "User-Agent: %s\r\n", USER_AGENT);
    ASSERT_WRITE(_buffer);

    snprintf(_buffer, _buffer_size, "Content-Type: multipart/form-data; boundary=%s\r\n", USER_AGENT);
    ASSERT_WRITE(_buffer);

    // Ouput basic auth creds if provided
    if(strlen(_basic_auth_creds_base64))
    {
        snprintf(_buffer, _buffer_size, "Authorization: Basic %s\r\n", _basic_auth_creds_base64);
        ASSERT_WRITE(_buffer);
    }

    // Prepare boundary headers to calc content length 
    snprintf(_buffer, _buffer_size,
            "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
            "Content-Type: text/plain\r\n\r\n", filename);

    // Calc content length
    int content_length = strlen(_buffer);
    // Boundary start/end * 2, plus '--' only for closing boundary
    content_length += (strlen(USER_AGENT) + 2) * 2 + 2;
    // Content body
    content_length += strlen(content);

    ASSERT_WRITE("Content-Length: ");
    char len[12] = "";
    snprintf(len, sizeof(len), "%d", content_length);
    ASSERT_WRITE(len);

    ASSERT_WRITE("\r\n\r\n--");
    ASSERT_WRITE(USER_AGENT);
    ASSERT_WRITE("\r\n");

    // Boundary headers prepared earlier
    ASSERT_WRITE(_buffer);

    ASSERT_WRITE(content);

    ASSERT_WRITE("\r\n--");
    ASSERT_WRITE(USER_AGENT);
    ASSERT_WRITE("--\r\n\r\n");

    //
    // Get response
    //
    char *pos = _buffer;
    int bytes = 0;
    int left = _buffer_size - 1;
    memset(_buffer, 0, _buffer_size);
    do
    {
		bytes = esp_tls_conn_read(_tls_conn, pos, left);

        left -= bytes;
        pos += bytes;       
    }while(bytes > 0 && left > 0 && (pos < _buffer + _buffer_size - 1));

    //
    // Parse data
    //
    int resp_code = 0;
    int found = sscanf(_buffer, "HTTP/%*f %d\r\n", &resp_code);
    
    if(found != 1)
    {
        ESP_LOGE(TAG, "Response code not found");

        return IPFS_CLIENT_INVALID_RESPONSE;
    }
    else if(resp_code != 200)
    {
        ESP_LOGE(TAG, "HTTP not OK, status: %d", resp_code);


        return IPFS_CLIENT_INVALID_RESPONSE;
    }

    // Find body
    char *resp_body = strstr(_buffer, "\r\n\r\n");
    if(resp_body == NULL)
    {
        ESP_LOGE(TAG, "Could not parse body");

        return IPFS_CLIENT_INVALID_RESPONSE;
    }
    // Skip 2x \r\n
	resp_body += 4;

    //
    // Parse JSON resp
    //
    StaticJsonDocument<255> json_doc;
    if (deserializeJson(json_doc, resp_body) == DeserializationError::Ok)
    {
        JsonObject obj = json_doc.as<JsonObject>();

        if (obj["name"].isNull() || obj["cid"]["/"].isNull() || obj["size"].isNull())
        {
            ESP_LOGE(TAG, "Invalid JSON object in response.");


            return IPFS_CLIENT_INVALID_RESPONSE;
        }
        else
        {
            if (file_out != NULL)
            {
                strncpy(file_out->name, obj["name"].as<char *>(), sizeof(file_out->name));
                strncpy(file_out->cid, obj["cid"]["/"].as<char *>(), sizeof(file_out->cid));
                file_out->size = obj["size"].as<uint32_t>();
            }


            return IPFS_CLIENT_OK;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Could not parse response JSON.");
        
        return IPFS_CLIENT_INVALID_RESPONSE;
    }

    return IPFS_CLIENT_OK;
}

/******************************************************************************
* Set ESP-TLS configuration
* @param tls_cfg Config struct
******************************************************************************/
void IPFSClient::set_tls_cfg(esp_tls_cfg_t *tls_cfg)
{
    memcpy(&_tls_cfg, tls_cfg, sizeof(esp_tls_cfg_t));
}

/******************************************************************************
* Set buffer to use for requests/responses.
* @param buff Pointer to buffer
* @param size Buff size
******************************************************************************/
void IPFSClient::set_buffer(char *buff, int size)
{
    _buffer = buff;
    _buffer_size = size;
}

/******************************************************************************
* Set basic auth credentials to be used in requests.
* @param creds base64 encoded credentials
******************************************************************************/
void IPFSClient::set_basic_auth_creds_base64(const char *creds)
{
    strncpy(_basic_auth_creds_base64, creds, sizeof(_basic_auth_creds_base64));
}

/******************************************************************************
* Set basic auth credentials to be used in requests.
* @param user Username
* @param pass Password 
******************************************************************************/
void IPFSClient::set_basic_auth_creds(const char *user, const char *pass)
{
    char buff[100];
    size_t output_len = 0;
    snprintf(buff, sizeof(buff), "%s:%s", user, pass);

    mbedtls_base64_encode((unsigned char*)_basic_auth_creds_base64, sizeof(_basic_auth_creds_base64), &output_len, (unsigned char*)buff, strlen(buff));
}