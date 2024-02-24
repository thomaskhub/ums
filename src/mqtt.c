#include "mqtt.h"
#include <libavutil/log.h>

Mqtt *internalContext;
void parseMessage(const char *msg) {
  json_error_t error;
  json_t *root = json_loads(msg, 0, &error);

  if (root) {
    if (root->type == JSON_OBJECT) {
      const char *key;
      json_t *value;

      json_object_foreach(root, key, value) {
        if (strcmp("cmd", key) == 0) { // when stop command receive exit the app
          av_log(NULL, AV_LOG_DEBUG, "Stop command received\n");
          char payload[512];
          sprintf(payload, "{\"cmd\":\"stopping\",\"payload\":{\"sessionId\":\"%s\"}}", getenv("sessionId"));

          MQTTClient_message message = MQTTClient_message_initializer;
          MQTTClient_token token;

          message.payload = payload;
          message.payloadlen = strlen(payload);
          message.qos = 2;
          message.retained = 0;
          MQTTClient_publishMessage(internalContext->client, getenv("mqttPublish"), &message, &token);
          exit(0);
        }
      }
    }
  }
}

void connlost(void *context, char *cause) {
  Mqtt *mqttContext = (Mqtt *)context;

  sleep(5);
  av_log(NULL, AV_LOG_ERROR, "reconnect....\n");
  mqttStart(mqttContext, mqttContext->mqttUrl);
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
  av_log(NULL, AV_LOG_DEBUG, "message arrived on topic %s. message is %s\n", topicName, (char *)message->payload);

  parseMessage((const char *)message->payload);
  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
  return 1;
}

void onSubscribeFailure(void *context, MQTTAsync_failureData *response) {
  av_log(NULL, AV_LOG_ERROR, "Subscribe failed, rc %d\n", response->code);
}

void onConnect(void *context, MQTTAsync_successData *response) {
  Mqtt *mqttContext = (Mqtt *)context;
  int rc;

  char *topic = getenv("mqttSubscribe");
  if (topic == NULL) {
    av_log(NULL, AV_LOG_ERROR, "no topic defined in mqttSubscribe input. abort.\n");
    exit(1);
  }

  if ((rc = MQTTClient_subscribe(mqttContext->client, topic, 0)) != MQTTCLIENT_SUCCESS) {
    av_log(NULL, AV_LOG_ERROR, "Failed to start subscribe, return code %d\n", rc);
  }
}

void mqttStart(Mqtt *context, char *mqttUrl) {
  int clientCreateCode, connectionStartCode, callbackCode;
  int retryCnt = 0;

  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

  context->mqttUrl = mqttUrl;
  context->mqttId = getenv("mqttId");
  context->mqttToken = getenv("mqttToken");

  if (context->mqttId == NULL || context->mqttUrl == NULL || context->mqttToken == NULL) {
    av_log(NULL, AV_LOG_ERROR, "Mqtt params not correct\n");
    av_log(NULL, AV_LOG_ERROR, "mqttId %s\n", context->mqttId);
    av_log(NULL, AV_LOG_ERROR, "mqttUrl %s\n", context->mqttUrl);
    av_log(NULL, AV_LOG_ERROR, "mqttToken %s\n", context->mqttToken);
    exit(1);
  }

  av_log(NULL, AV_LOG_DEBUG, "creating new mqtt client");

  clientCreateCode = MQTTClient_create(&context->client, mqttUrl, context->mqttId, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if (clientCreateCode != MQTTASYNC_SUCCESS) {
    av_log(NULL, AV_LOG_ERROR, "Failed to create client, return code %d\n", clientCreateCode);
    exit(1);
  }

  if ((callbackCode = MQTTClient_setCallbacks(context->client, context, connlost, msgarrvd, NULL)) != MQTTASYNC_SUCCESS) {
    av_log(NULL, AV_LOG_ERROR, "Failed to set callbacks, return code %d\n", callbackCode);
    callbackCode = EXIT_FAILURE;
  }

  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;
  conn_opts.retryInterval = 10;
  conn_opts.username = context->mqttId;
  conn_opts.password = context->mqttToken;
  context->options = conn_opts;
  
  internalContext = context;

  while (1) {
    connectionStartCode = MQTTClient_connect(context->client, &conn_opts);

    if (connectionStartCode != MQTTASYNC_SUCCESS) {
      av_log(NULL, AV_LOG_ERROR, "Failed to connect, return code %d. retry\n", connectionStartCode);
      sleep(3);
      retryCnt++;

    } else {
      break;
    }

    if (retryCnt >= 10) {
      av_log(NULL, AV_LOG_ERROR, "Failed to connect, return code %d\n", connectionStartCode);
      exit(1);
    }
  }

  onConnect(context, NULL);
}