# HTTP Proxy

This is an HTTP proxy server that can handle GET, POST, and CONNECT requests, as well as cache GET responses. It can be used to browse typical web pages, and is designed to be configurable for use with a browser.

## Features

- Handles GET, POST, and CONNECT requests
- Caches responses (when they are 200-OK) to GET requests
- Follows rules of expiration time and/or re-validation in determining whether to serve a request from the local cache or re-fetch it from the origin server
- Allows for multiple concurrent requests and uses multiple threads
- Logs each request with a unique identifier, time received, IP address received from, and HTTP request line
- Responds with appropriate messages based on whether the request is in cache, expired, requires validation, or is valid
- Can handle tunnels resulting from 200-OK responses
- Produces a log in /var/log/erss/proxy.log with information about each request
- Prints times in UTC with a format given by asctime

## Installation

### Docker

```bash
cd docker-deploy
sudo docker-compuse up

or
cd docker-deploy
sudo docker-compuse down
sudo docker-compuse build
sudo docker-compuse up
```

## Logging

Upon receiving a new request, the proxy assigns it a unique id (UUID), prints the ID, time received (TIME), IP address the request was received from (IPFROM), and the HTTP request line (REQUEST) of the request. If the request is a GET request, the proxy checks its cache and prints one of the following messages: "not in cache," "in cache, but expired at EXPIREDTIME," "in cache, requires validation," or "in cache, valid." If the proxy needs to contact the origin server about the request, it prints the request it makes to the origin server, and later, when it receives the response from the origin server, it prints: "Received RESPONSE from SERVER." If the proxy receives a 200-OK in response to a GET request, it prints one of the following messages: "not cacheable because REASON," "cached, expires at EXPIRES," or "cached, but requires re-validation." Whenever the proxy responds to the client, it logs: "Responding RESPONSE." If the proxy is handling a tunnel resulting from a 200-OK response, it logs: "Tunnel closed." 

## Running the Proxy

To run the proxy, use sudo docker-compose up, which will connect the host computer's port 12345 to the proxy and mount a directory called logs to /var/log/erss in the container running the proxy. This will allow you to find the proxy.log file in the logs directory on the host.

## Test Cases

The file containing the test cases' records can be found in "TestRecords.txt".