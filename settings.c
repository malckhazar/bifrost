#include "settings.h"

void settings_init ()
{
	bifrost_settings.queue_path = "/tmp/mq";
	bifrost_settings.message_batch_size = 5;
	bifrost_settings.channel_prefix = "/tmp/bifrost/";
}

void settings_free ()
{
}

