/*
 * OpenThread Custom Configuration
 * CoAP API, Ping Sender API, Leader Weight, SRP Server enabled by default.
 * (SRP server + CLI: requires CONFIG_OPENTHREAD_HEADER_CUSTOM=y and path "include" in sdkconfig.)
 */

#pragma once

#define OPENTHREAD_CONFIG_COAP_API_ENABLE 1
#define OPENTHREAD_CONFIG_PING_SENDER_ENABLE 1
#define OPENTHREAD_CONFIG_MLE_DEVICE_PROPERTY_LEADER_WEIGHT_ENABLE 1

/* SRP Server: lets Thread nodes (SRP client) register services for DNS-based service discovery */
#define OPENTHREAD_CONFIG_SRP_SERVER_ENABLE 1
