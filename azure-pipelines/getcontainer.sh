#!/bin/bash

set -e

DOCKERFILE_PATH=$1

if [ "${DOCKERFILE_PATH}" = "" ]; then
	echo "usage: $0 dockerfile"
	exit 1
fi

if [ "${DOCKER_REGISTRY}" = "" ]; then
	echo "DOCKER_REGISTRY environment variable is unset."
	echo "Not running inside GitHub Actions or misconfigured?"
	exit 1
fi

DOCKER_CONTAINER="${GITHUB_REPOSITORY}/$(basename ${DOCKERFILE_PATH})"
DOCKER_REGISTRY_CONTAINER="${DOCKER_REGISTRY}/${DOCKER_CONTAINER}"

echo "docker-container=${DOCKER_CONTAINER}" >> $GITHUB_ENV
echo "docker-registry-container=${DOCKER_REGISTRY_CONTAINER}" >> $GITHUB_ENV

# Identify the last git commit that touched the Dockerfiles
# Use this as a hash to identify the resulting docker containers
DOCKER_SHA=$(git log -1 --pretty=format:"%h" -- "${DOCKERFILE_PATH}")
echo "docker-sha=${DOCKER_SHA}" >> $GITHUB_ENV

DOCKER_REGISTRY_CONTAINER_SHA="${DOCKER_REGISTRY_CONTAINER}:${DOCKER_SHA}"

echo "docker-registry-container-sha=${DOCKER_REGISTRY_CONTAINER_SHA}" >> $GITHUB_ENV
echo "docker-registry-container-latest=${DOCKER_REGISTRY_CONTAINER}:latest" >> $GITHUB_ENV

exists="true"
docker login https://${DOCKER_REGISTRY} -u ${GITHUB_ACTOR} -p ${GITHUB_TOKEN} || exists="false"

if [ "${exists}" != "false" ]; then
	docker pull ${DOCKER_REGISTRY_CONTAINER_SHA} || exists="false"
fi

if [ "${exists}" = "true" ]; then
	echo "docker-container-exists=true" >> $GITHUB_ENV
else
	echo "docker-container-exists=false" >> $GITHUB_ENV
fi
