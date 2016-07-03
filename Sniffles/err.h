#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void shout_error(int err)
{
	char errbuf[512];
	strerror_s(errbuf, err);
	printf("Error: %s (%i)", errbuf, err);
}

static void shout_error(const char* err)
{
	printf("Error: %s\n", err);
}

static void shout_error_and_die(int err)
{
	shout_error(err);
	exit(err);
}

static void shout_error_and_die(const char* err)
{
	shout_error(err);
	exit(1);
}