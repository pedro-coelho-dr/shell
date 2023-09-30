#!/bin/bash

count=0
while [ $count -lt 21 ]; do
    echo "Background #${count}"
    count=$((count+1))
    sleep 1
done