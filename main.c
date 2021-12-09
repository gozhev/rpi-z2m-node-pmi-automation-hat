#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <linux/i2c-dev.h>  
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/gpio.h>  

#include <MQTTClient.h>
#include <cjson/cJSON.h>

static volatile sig_atomic_t g_quit = false;

void sig_handler(int signum)
{
	(void) signum;
	g_quit = true;
}

int main(int argc, char **argv)
{
	static const int RELAY_GPIO_N = 13;
	static const char* GPIO_DEV_NAME = "/dev/gpiochip0";

	int retval = 0;
	int fd = 0;
	struct gpiohandle_request req = {}; 
	struct gpiohandle_data data = {};

	signal(SIGINT, sig_handler);

	fd = open(GPIO_DEV_NAME, O_RDWR);
	if (fd == -1) {
		perror("failed to open gpio device");  
		goto exit;
	}

	req.flags = GPIOHANDLE_REQUEST_OUTPUT; 
	req.lines = 1; 
	req.lineoffsets[0] = RELAY_GPIO_N; 
	req.default_values[0] = 0; 

	retval = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req); 
	if (retval == -1) {
		perror("failed to request gpio line direction");
		goto exit_close;
	}

	data.values[0] = 0;
	retval = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
	if (retval == -1) {
		perror("failed to set gpio value");
		goto exit_gpio_close;
	}

	MQTTClient client = 0;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_create(&client, "127.0.0.1:1883", "switchd",
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	retval = MQTTClient_connect(client, &conn_opts);
	if (retval != MQTTCLIENT_SUCCESS) {
		fprintf(stderr, "mqtt connect failed\n");
		goto exit_gpio_close;
	}

	MQTTClient_subscribe(client, "zigbee2mqtt/switch0", 0);

	char* topic_name = NULL;
	int topic_length = 0;
	MQTTClient_message* message = NULL;

	int switch_value = 0x0;

	while (!g_quit) {
		MQTTClient_receive(client, &topic_name, &topic_length, &message, 200);
		if (message != NULL) {
			cJSON *json = cJSON_ParseWithLength(
					message->payload, message->payloadlen);
			cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
			if (cJSON_IsString(action) && (action->valuestring != NULL)
					&& !strcmp(action->valuestring, "single")) {

				switch_value ^= 0x1;
				data.values[0] = switch_value;
				retval = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
				if (retval == -1) {
					perror("failed to set gpio value");
					goto exit_gpio_close;
				}
			}

			MQTTClient_free(topic_name);
			MQTTClient_freeMessage(&message);
		}
	}

	data.values[0] = 0;
	retval = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
	if (retval == -1) {
		perror("failed to set gpio value");
		goto exit_gpio_close;
	}

	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);

exit_gpio_close:
	retval = close(req.fd);
	if (retval == -1) {
		perror("failed to close gpio line device");  
	}

exit_close:
	retval = close(fd);
	if (retval == -1) {
		perror("failed to close gpio device");  
	}

exit:
	return retval;
}
