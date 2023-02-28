#ifndef __MQTT___
#define __MQTT___

#include <MQTTAsync.h>
#include <MQTTClient.h>
#include <jansson.h>
#include <unistd.h>

typedef struct Mqtt {
  MQTTClient client;
  MQTTClient_connectOptions options;
  char *mqttUrl;
  char *mqttId;
  char *mqttToken;
  int isConnected;

  const int qos;
} Mqtt;

void mqttStart(Mqtt *context, char *mqttUrl);

#endif