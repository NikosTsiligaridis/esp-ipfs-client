# ESP IPFS Client
IPFS REST client as an ESP32 IDF component.
Supports HTTPS with basic authentication.

## How to use
In components folder:
```
git submodule add git@github.com:NikosTsiligaridis/esp-ipfs-client.git
```

In main CMakeLists add to dependencies
```
set(COMPONENT_REQUIRES "esp-ipfs-client")
```

## Example
### Add file
```
IPFSClient ipfs_client;

ipfs_client.set_addr("ipfs.examp.le");
ipfs_client.set_basic_auth_creds("timon", "pumba");
ipfs_client.set_buffer(buff, sizeof(buff));

esp_tls_cfg_t tls_cfg = {
    .cacert_buf = pem_start,
    .cacert_bytes = pem_end - pem_start,
    .timeout_ms = 10000
};

ipfs_client.set_tls_cfg(&tls_cfg);

// Will receive parsed response
IPFSClient::IPFSFile ipfs_file = {};

// Buffer to use for requests/response
char buff[500] = "";

if(ipfs_client.add(&ipfs_file, "filename.txt", "file content") != IPFSClient::IPFS_CLIENT_OK)
{
    ESP_LOGE(TAG, "Could not submit file to IPFS.");
}
else
{
    ESP_LOGI(TAG, "Success! File: %s CID: %s (%d bytes)", ipfs_file.name, ipfs_file.cid, ipfs_file.size);;
}
```

Output:
```
Success! File: filename.txt CID: QmZev1d1Ji8yhafeMJiht6yFGvXtUcChQ2NNokZHojciNv (32 bytes)
```