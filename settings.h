#ifndef SETTINGS_H
#define SETTINGS_H

static struct {
	char* queue_path;
	unsigned int message_batch_size;
	char* channel_prefix;
} bifrost_settings;

void settings_init ();
void settings_free ();

#endif
