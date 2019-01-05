#ifndef SETTINGS_H
#define SETTINGS_H

static struct {
	char* queue_path;
} settings;

void settings_init ();
void settings_free ();

#endif
