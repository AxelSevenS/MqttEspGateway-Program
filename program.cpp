#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <modbus/modbus.h>
#include <mosquitto.h>
#include <unistd.h>
#include <errno.h>
#include <chrono>
#include <thread>


#define MODBUS_DEVICE "/dev/ttyS0"
#define BAUD_RATE 9600
#define SLAVE_ID 1
#define REGISTER_START 0
#define REGISTER_COUNT 1

#define MQTT_HOST "192.168.238.201"
#define MQTT_PORT 1883
#define STEAM_RAW_TOPIC "sensors/steam/raw"
#define STEAM_THRESHOLD_TOPIC "sensors/steam/threshold"
#define STEAM_BOOL_TOPIC "sensors/steam/bool"


uint16_t steamThreshold = 1000;
int callback_id;

void on_steam_threshold_update(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
    printf("Nouvelle valeur de seuil de valeur reçue\n");
}

int main() {
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Erreur : Impossible de se connecter au broker MQTT (%s)\n", MQTT_HOST);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }



    mosquitto_subscribe(mosq, NULL, STEAM_THRESHOLD_TOPIC, 1);
    mosquitto_subscribe_callback_set(mosq, on_steam_threshold_update);



    modbus_t* ctx = modbus_new_rtu(MODBUS_DEVICE, BAUD_RATE, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Erreur : Impossible d'initialiser Modbus\n");
        return 1;
    }
    modbus_set_slave(ctx, SLAVE_ID);

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Erreur : Impossible de se connecter à Modbus\n");
        modbus_free(ctx);
        return 1;
    }


    uint16_t data[64];
    while (1) {
        int rc = modbus_read_registers(ctx, REGISTER_START, REGISTER_COUNT, data);
        if (rc == -1) {
            fprintf(stderr, "Erreur : %s\n", modbus_strerror(errno));
        } else {
            uint16_t steam = data[0];

            char payload[256];
            snprintf(payload, sizeof(payload), "%d", data[0]);
            mosquitto_publish(mosq, NULL, STEAM_RAW_TOPIC, strlen(payload), payload, 0, false);

            bool steam_bool = steam > steamThreshold;
            mosquitto_publish(mosq, NULL, STEAM_BOOL_TOPIC, 1, steam_bool ? "1" : "0", 0, false);

            printf("MQTT envoyé : %s\n", payload);
        }
        sleep(1);
    }

    modbus_close(ctx);
    modbus_free(ctx);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}



