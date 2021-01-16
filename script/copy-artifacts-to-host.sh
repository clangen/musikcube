#!/bin/bash

if [[ -z "${MUSIKCUBE_BUILD_HOST_IP}" ]]; then
    echo "no build host ip specified"
    exit
fi

if [[ -z "${MUSIKCUBE_BUILD_HOST_PW}" ]]; then
    echo "no build host pw specified"
    exit
fi

GLOB=${1}

if [[ -z "${GLOB}" ]]; then
    echo "no file glob specified."
    exit
fi

echo "copying build artifacts to host..."
echo "  from: ${GLOB}"
ls -al "${GLOB}"
sshpass -p ${MUSIKCUBE_BUILD_HOST_PW} scp -o StrictHostKeyChecking=no "${GLOB}" build@${MUSIKCUBE_BUILD_HOST_IP}:/home/build/ 2> /dev/null
echo "finished copying build artifacts."
