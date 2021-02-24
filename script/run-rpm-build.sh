#!/bin/bash

JOB_COUNT=${MUSIKCUBE_BUILD_JOB_COUNT}
if [[ -z "${JOB_COUNT}" ]]; then
  JOB_COUNT=2
fi

echo "configuring rpm with ${JOB_COUNT} jobs"
sed -i "s/-j2/-j${JOB_COUNT}/g" musikcube.spec 
rpmbuild -ba -vv musikcube.spec

