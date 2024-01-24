#!/bin/bash
make all
echo 'Start Running HTTP Cache Proxy Server...'
./bin/http_cache_proxy &
while true ; do continue ; done