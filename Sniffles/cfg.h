#pragma once

#include "str.h"

enum ConfigOptions
{
	INVALID_OPT = -1,
	DEVICE,
	PORT,
	GAY,
	OPTCOUNT
};

static char g_default_config[] = "\x02\x00\x00\x00|\x87\x69\x00\x00|gaypuss|";
static char* g_config;
static size_t g_config_size;

static char* get_config_value(int opt)
{
	char* v = g_config;
	char* delimiter = g_config;
	size_t length = g_config_size;
	char* endOfConfig = g_config + length;

	int index = 0;
	
	for (int i = 0; i < opt; i++)
	{
		v = (char*)memchr(v, '|', length) + 1;
		length = (int)((unsigned long)endOfConfig - (unsigned long)v);
	}

	delimiter = (char*)memchr(v, '|', length);
	length = (int)((unsigned long)delimiter - (unsigned long)v);

	//v[length] = '\0';

	return v;
}

static void set_config_value(int opt, void* value, size_t size)
{
	char* cfg = g_config;
	char* v = g_config;
	char* delimiter = g_config;
	size_t origLength = g_config_size;
	size_t length = origLength;
	char* endOfConfig = g_config + origLength;

	for (int i = 0; i < opt; i++)
	{
		v = (char*)memchr(v, '|', length) + 1;
		length = (int)((unsigned long)endOfConfig - (unsigned long)v);
	}

	int index = (int)((unsigned long)v - (unsigned long)g_config);

	delimiter = (char*)memchr(v, '|', length);
	length = (int)((unsigned long)endOfConfig - (unsigned long)delimiter);
	int delimiterIndex = (int)((unsigned long)delimiter - (unsigned long)g_config);

	// calculate new size
	int oldValueSize = delimiterIndex - index;
	int newSize = g_config_size - oldValueSize + size;

	// reallocate the config pointer
	g_config = (char*)realloc(cfg, newSize);

	// set new value then copy other values to the end
	memcpy(v, value, size);
	memcpy(v + size, delimiter, length);

	g_config_size = newSize;

}

template<class T>
static void set_config_option(int opt, memory_t<T> value)
{
	if (opt > OPTCOUNT)
	{
		shout_error("[set_config_option] Invalid config option");
		return;
	}
	
	set_config_value(opt, (void*)value.ptr(), value.size());
}

template<class T>
static memory_t<T> get_config_option(int opt)
{
	if (opt > OPTCOUNT)
		return memory_t<T>();

	char* value = get_config_value(opt);

	return memory_t<T>(value, sizeof(T));
}

template<class T>
static memory_t<T> get_config_option(int opt, size_t count)
{
	if (opt > OPTCOUNT)
		return memory_t<T>();

	void* value = get_config_value(opt);

	memory_t<T> mem(value, sizeof(T));

	return mem;
}

static bool file_exists(const char* name)
{
	FILE *file;
	errno_t ret = fopen_s(&file, name, "r");
	if (file)
	{
		fclose(file);
		return true;
	}

	return false;
}

static bool check_config_file()
{
	FILE* fp;
	bool bFileExists;
	errno_t res;

	bFileExists = file_exists("cfg");

	res = fopen_s(&fp, "cfg", "ab+");
	if (!fp)
		shout_error_and_die(res);

	if (!bFileExists)
	{
		// Make new default config
		g_config_size = sizeof(g_default_config);
		g_config = g_default_config;
		fwrite(g_config, 1, g_config_size, fp);
	}
	else
	{
		// obtain file size
		long configSize;
		fseek(fp, 0, SEEK_END);
		configSize = ftell(fp);
		rewind(fp);

		g_config_size = configSize;
		g_config = (char*)malloc(g_config_size);
		size_t result = fread(g_config, 1, g_config_size, fp);
		if (result != g_config_size)
			shout_error_and_die("Reading error!");
	}

	fclose(fp);

	return bFileExists;
}

static void save_config_file(FILE* fp)
{
	fwrite(g_config, 1, g_config_size, fp);
}

static bool save_config(memory_t<int> cfg_device, memory_t<int> cfg_port)
{
	FILE* fp;
	errno_t res;

	res = fopen_s(&fp, "cfg", "rb+");
	if (!fp)
		shout_error_and_die(res);

	set_config_option<int>(PORT, cfg_port);
	set_config_option<int>(DEVICE, cfg_device);

	save_config_file(fp);

	fclose(fp);

	return true;
}