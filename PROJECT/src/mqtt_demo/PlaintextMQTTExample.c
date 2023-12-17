/*
 * FreeRTOS V202111.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * Demo for showing use of the managed MQTT API.
 *
 * The Example shown below uses this API to create MQTT messages and
 * send them over the connection established using FreeRTOS sockets.
 * The example is single threaded and uses statically allocated memory;
 * it uses QOS0 and therefore does not implement any retransmission
 * mechanism for Publish messages.
 *
 * !!! NOTE !!!
 * This MQTT demo does not authenticate the server nor the client.
 * Hence, this demo should not be used as production ready code.
 *
 * Also see https://www.freertos.org/mqtt/mqtt-agent-demo.html? for an
 * alternative run time model whereby coreMQTT runs in an autonomous
 * background agent task.  Executing the MQTT protocol in an agent task
 * removes the need for the application writer to explicitly manage any MQTT
 * state or call the MQTT_ProcessLoop() API function. Using an agent task
 * also enables multiple application tasks to more easily share a single
 * MQTT connection.
 */

#include "PlaintextMQTTExample.h"

/**
 * @brief Packet Identifier generated when Unsubscribe request was sent to the broker;
 * it is used to match received Unsubscribe response to the transmitted unsubscribe
 * request.
 */
static uint16_t usUnsubscribePacketIdentifier;

/**
 * @brief A pair containing a topic filter and its SUBACK status.
 */
typedef struct topicFilterContext
{
    const char * pcTopicFilter;
    MQTTSubAckStatus_t xSubAckStatus;
} topicFilterContext_t;

/**
 * @brief An array containing the context of a SUBACK; the SUBACK status
 * of a filter is updated when the event callback processes a SUBACK.
 */
static topicFilterContext_t xTopicFilterContext[ mqttexampleTOPIC_COUNT ] =
{
    { mqttexampleTOPIC, MQTTSubAckFailure }
};

/**
 * @brief Static buffer used to hold MQTT messages being sent and received.
 */
static MQTTFixedBuffer_t xBuffer =
{
    .pBuffer = ucSharedBuffer,
    .size    = mqttexampleSHARED_BUFFER_SIZE
};

/*-----------------------------------------------------------*/

/**
 * @brief Create the task that demonstrates the MQTT API over a plaintext TCP
 * connection.
 */
void vStartSimpleMQTTDemo( void )
{
    /* This example uses a single application task, which in turn is used to
     * connect, subscribe, publish, unsubscribe and disconnect from the MQTT
     * broker.
     *
     * Also see https://www.freertos.org/mqtt/mqtt-agent-demo.html? for an
     * alternative run time model whereby coreMQTT runs in an autonomous
     * background agent task.  Executing the MQTT protocol in an agent task
     * removes the need for the application writer to explicitly manage any MQTT
     * state or call the MQTT_ProcessLoop() API function. Using an agent task
     * also enables multiple application tasks to more easily share a single
     * MQTT connection.*/
    xTaskCreate( prvMQTTDemoTask,          /* Function that implements the task. */
                 "DemoTask",               /* Text name for the task - only used for debugging. */
                 democonfigDEMO_STACKSIZE, /* Size of stack (in words, not bytes) to allocate for the task. */
                 NULL,                     /* Task parameter - not used in this case. */
                 tskIDLE_PRIORITY,         /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                 NULL );                   /* Used to pass out a handle to the created task - not used in this case. */
}
/*-----------------------------------------------------------*/
#if 0
static void prvMQTTDemoTask( void * pvParameters )
{
    uint32_t ulPublishCount = 0U, ulTopicCount = 0U;
    const uint32_t ulMaxPublishCount = 5UL;
    NetworkContext_t xNetworkContext = { 0 };
    PlaintextTransportParams_t xPlaintextTransportParams = { 0 };
    MQTTContext_t xMQTTContext;
    MQTTStatus_t xMQTTStatus;
    PlaintextTransportStatus_t xNetworkStatus;

    /* Remove compiler warnings about unused parameters. */
    ( void ) pvParameters;

    /* Set the pParams member of the network context with desired transport. */
    xNetworkContext.pParams = &xPlaintextTransportParams;

    ulGlobalEntryTimeMs = prvGetTimeMs();

    for( ; ; )
    {
        /****************************** Connect. ******************************/

        /* Attempt to connect to the MQTT broker. If connection fails, retry after
         * a timeout. The timeout value will exponentially increase until the
         * maximum number of attempts are reached or the maximum timeout value is
         * reached. The function below returns a failure status if the TCP connection
         * cannot be established to the broker after the configured number of attempts. */
        xNetworkStatus = prvConnectToServerWithBackoffRetries( &xNetworkContext );
        configASSERT( xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS );

        /* Sends an MQTT Connect packet over the already connected TCP socket,
         * and waits for a connection acknowledgment (CONNACK) packet. */
        LogInfo( ( "Creating an MQTT connection to %s.", democonfigMQTT_BROKER_ENDPOINT ) );
        prvCreateMQTTConnectionWithBroker( &xMQTTContext, &xNetworkContext );

        /**************************** Subscribe. ******************************/

        /* If server rejected the subscription request, attempt to resubscribe to
         * the topic. Attempts are made according to the exponential backoff retry
         * strategy declared in backoff_algorithm.h. */
        prvMQTTSubscribeWithBackoffRetries( &xMQTTContext );

        /******************* Publish and Keep Alive Loop. *********************/
        /* Publish messages with QoS0, then send and process Keep Alive messages. */
        for( ulPublishCount = 0; ulPublishCount < ulMaxPublishCount; ulPublishCount++ )
        {
            LogInfo( ( "Publish to the MQTT topic %s.", mqttexampleTOPIC ) );
            prvMQTTPublishToTopic( &xMQTTContext );
vTaskDelay(mqttexamplePROCESS_LOOP_TIMEOUT_MS);
            /* Process the incoming publish echo. Since the application subscribed
             * to the same topic, the broker will send the same publish message
             * back to the application. */
            LogInfo( ( "Attempt to receive publish message from broker." ) );
            xMQTTStatus = MQTT_ProcessLoop( &xMQTTContext,
                                            mqttexamplePROCESS_LOOP_TIMEOUT_MS );
            configASSERT( xMQTTStatus == MQTTSuccess );

            /* Leave the connection idle for some time. */
            LogInfo( ( "Keeping Connection Idle...\r\n" ) );
            vTaskDelay( mqttexampleDELAY_BETWEEN_PUBLISHES );
        }

        /******************** Unsubscribe from the topic. *********************/
        LogInfo( ( "Unsubscribe from the MQTT topic %s.", mqttexampleTOPIC ) );
        prvMQTTUnsubscribeFromTopic( &xMQTTContext );

        /* Process the incoming packet from the broker. */
        xMQTTStatus = MQTT_ProcessLoop( &xMQTTContext,
                                        mqttexamplePROCESS_LOOP_TIMEOUT_MS );
        configASSERT( xMQTTStatus == MQTTSuccess );

        /**************************** Disconnect. *****************************/

        /* Send an MQTT Disconnect packet over the connected TCP socket.
         * There is no corresponding response for a disconnect packet. After
         * sending the disconnect, the client must close the network connection. */
        LogInfo( ( "Disconnecting the MQTT connection with %s.",
                   democonfigMQTT_BROKER_ENDPOINT ) );
        xMQTTStatus = MQTT_Disconnect( &xMQTTContext );
        configASSERT( xMQTTStatus == MQTTSuccess );

        /* Close the network connection. */
        xNetworkStatus = Plaintext_FreeRTOS_Disconnect( &xNetworkContext );
        configASSERT( xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS );

        /* Reset SUBACK status for each topic filter after completion of
         * subscription request cycle. */
        for( ulTopicCount = 0; ulTopicCount < mqttexampleTOPIC_COUNT; ulTopicCount++ )
        {
            xTopicFilterContext[ ulTopicCount ].xSubAckStatus = MQTTSubAckFailure;
        }

        /* Wait for some time between two iterations to ensure that we do not
         * bombard the MQTT broker. */
        LogInfo( ( "prvMQTTDemoTask() completed an iteration successfully. " ) );
                 //  "Total free heap is %u.", xPortGetFreeHeapSize() ) );
        LogInfo( ( "Demo completed successfully." ) );
        LogInfo( ( "Short delay before starting the next iteration.... \r\n" ) );
        vTaskDelay( mqttexampleDELAY_BETWEEN_DEMO_ITERATIONS );
    }
}
#endif
/*-----------------------------------------------------------*/

PlaintextTransportStatus_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pxNetworkContext )
{
    PlaintextTransportStatus_t xNetworkStatus;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xReconnectParams;
    uint16_t usNextRetryBackOff = 0U;

    /* Initialize reconnect attempts and interval.*/
    BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                       mqttexampleRETRY_BACKOFF_BASE_MS,
                                       mqttexampleRETRY_MAX_BACKOFF_DELAY_MS,
                                       mqttexampleRETRY_MAX_ATTEMPTS );

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase till maximum
     * attempts are reached.
     */
    do
    {
        /* Establish a TCP connection with the MQTT broker. This example connects to
         * the MQTT broker as specified in democonfigMQTT_BROKER_ENDPOINT and
         * democonfigMQTT_BROKER_PORT at the top of this file. */
        LogInfo( ( "Create a TCP connection to %s:%d.",
                   democonfigMQTT_BROKER_ENDPOINT,
                   democonfigMQTT_BROKER_PORT ) );
        xNetworkStatus = Plaintext_FreeRTOS_Connect( pxNetworkContext,
                                                     democonfigMQTT_BROKER_ENDPOINT,
                                                     democonfigMQTT_BROKER_PORT,
                                                     mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                                     mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS )
        {
            /* Generate a random number and calculate backoff value (in milliseconds) for
             * the next connection retry.
             * Note: It is recommended to seed the random number generator with a device-specific
             * entropy source so that possibility of multiple devices retrying failed network operations
             * at similar intervals can be avoided. */
            xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xReconnectParams, uxRand(), &usNextRetryBackOff );

            if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                LogError( ( "Connection to the broker failed, all attempts exhausted." ) );
            }
            else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogWarn( ( "Connection to the broker failed. "
                           "Retrying connection with backoff and jitter." ) );
                vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );
            }
        }
    } while( ( xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );

    return xNetworkStatus;
}
/*-----------------------------------------------------------*/

void prvCreateMQTTConnectionWithBroker( MQTTContext_t * pxMQTTContext,
                                               NetworkContext_t * pxNetworkContext )
{
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent;
    TransportInterface_t xTransport;

    /***
     * For readability, error handling in this function is restricted to the use of
     * asserts().
     ***/

    /* Fill in Transport Interface send and receive function pointers. */
    xTransport.pNetworkContext = pxNetworkContext;
    xTransport.send = Plaintext_FreeRTOS_send;
    xTransport.recv = Plaintext_FreeRTOS_recv;

    /* Initialize MQTT library. */
    xResult = MQTT_Init( pxMQTTContext, &xTransport, prvGetTimeMs, prvEventCallback, &xBuffer );
    configASSERT( xResult == MQTTSuccess );

    /* Many fields not used in this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = true;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    //original: xConnectInfo.pClientIdentifier = democonfigCLIENT_IDENTIFIER;
    //original: xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( democonfigCLIENT_IDENTIFIER );

    /*Edited by Parth*/
    xConnectInfo.pClientIdentifier = SECRET_MQTT_CLIENT_ID;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( SECRET_MQTT_CLIENT_ID );

    xConnectInfo.pUserName = SECRET_MQTT_USERNAME;
    xConnectInfo.userNameLength = ( uint16_t ) strlen(xConnectInfo.pUserName);

    xConnectInfo.pPassword = SECRET_MQTT_PASSWORD;
    xConnectInfo.passwordLength = ( uint16_t ) strlen(xConnectInfo.pPassword);

    /* Set MQTT keep-alive period. It is the responsibility of the application to ensure
     * that the interval between Control Packets being sent does not exceed the Keep Alive value.
     * In the absence of sending any other Control Packets, the Client MUST send a PINGREQ Packet. */
    xConnectInfo.keepAliveSeconds = mqttexampleKEEP_ALIVE_TIMEOUT_SECONDS;

    /* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it
     * is passed as NULL. */
    xResult = MQTT_Connect( pxMQTTContext,
                            &xConnectInfo,
                            NULL,
                            mqttexampleCONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );
    configASSERT( xResult == MQTTSuccess );
}
/*-----------------------------------------------------------*/

void prvUpdateSubAckStatus( MQTTPacketInfo_t * pxPacketInfo )
{
    MQTTStatus_t xResult = MQTTSuccess;
    uint8_t * pucPayload = NULL;
    size_t ulSize = 0;
    uint32_t ulTopicCount = 0U;

    xResult = MQTT_GetSubAckStatusCodes( pxPacketInfo, &pucPayload, &ulSize );

    /* MQTT_GetSubAckStatusCodes always returns success if called with packet info
     * from the event callback and non-NULL parameters. */
    configASSERT( xResult == MQTTSuccess );

    for( ulTopicCount = 0; ulTopicCount < ulSize; ulTopicCount++ )
    {
        //xTopicFilterContext[ ulTopicCount ].xSubAckStatus = pucPayload[ ulTopicCount ];  //this line was commented out by Parth
    }
}
/*-----------------------------------------------------------*/

void prvMQTTSubscribeWithBackoffRetries( MQTTContext_t * pxMQTTContext )
{
    MQTTStatus_t xResult = MQTTSuccess;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xRetryParams;
    uint16_t usNextRetryBackOff = 0U;
    MQTTSubscribeInfo_t xMQTTSubscription[ mqttexampleTOPIC_COUNT ];
    bool xFailedSubscribeToTopic = false;
    uint32_t ulTopicCount = 0U;

    /* Some fields not used by this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xMQTTSubscription, 0x00, sizeof( xMQTTSubscription ) );

    /* Get a unique packet id. */
    usSubscribePacketIdentifier = MQTT_GetPacketId( pxMQTTContext );

    /* Subscribe to the mqttexampleTOPIC topic filter. This example subscribes to
     * only one topic and uses QoS0. */
    xMQTTSubscription[ 0 ].qos = MQTTQoS0;
    xMQTTSubscription[ 0 ].pTopicFilter = mqttexampleTOPIC;
    xMQTTSubscription[ 0 ].topicFilterLength = ( uint16_t ) strlen( mqttexampleTOPIC );

    /* Initialize context for backoff retry attempts if SUBSCRIBE request fails. */
    BackoffAlgorithm_InitializeParams( &xRetryParams,
                                       mqttexampleRETRY_BACKOFF_BASE_MS,
                                       mqttexampleRETRY_MAX_BACKOFF_DELAY_MS,
                                       mqttexampleRETRY_MAX_ATTEMPTS );

    do
    {
        /* The client is now connected to the broker. Subscribe to the topic
         * as specified in mqttexampleTOPIC at the top of this file by sending a
         * subscribe packet then waiting for a subscribe acknowledgment (SUBACK).
         * This client will then publish to the same topic it subscribed to, so it
         * will expect all the messages it sends to the broker to be sent back to it
         * from the broker. This demo uses QOS0 in Subscribe, therefore, the Publish
         * messages received from the broker will have QOS0. */
        LogInfo( ( "Attempt to subscribe to the MQTT topic %s.", mqttexampleTOPIC ) );
        xResult = MQTT_Subscribe( pxMQTTContext,
                                  xMQTTSubscription,
                                  sizeof( xMQTTSubscription ) / sizeof( MQTTSubscribeInfo_t ),
                                  usSubscribePacketIdentifier );
        configASSERT( xResult == MQTTSuccess );

        LogInfo( ( "SUBSCRIBE sent for topic %s to broker.\n\n", mqttexampleTOPIC ) );

        /* Process incoming packet from the broker. After sending the subscribe, the
         * client may receive a publish before it receives a subscribe ack. Therefore,
         * call generic incoming packet processing function. Since this demo is
         * subscribing to the topic to which no one is publishing, probability of
         * receiving Publish message before subscribe ack is zero; but application
         * must be ready to receive any packet.  This demo uses the generic packet
         * processing function everywhere to highlight this fact. */
        xResult = MQTT_ProcessLoop( pxMQTTContext, mqttexamplePROCESS_LOOP_TIMEOUT_MS );
        configASSERT( xResult == MQTTSuccess );

        /* Reset flag before checking suback responses. */
        xFailedSubscribeToTopic = false;

        /* Check if recent subscription request has been rejected. #xTopicFilterContext is updated
         * in the event callback to reflect the status of the SUBACK sent by the broker. It represents
         * either the QoS level granted by the server upon subscription, or acknowledgement of
         * server rejection of the subscription request. */
        for( ulTopicCount = 0; ulTopicCount < mqttexampleTOPIC_COUNT; ulTopicCount++ )
        {
            if( xTopicFilterContext[ ulTopicCount ].xSubAckStatus == MQTTSubAckFailure )
            {
                xFailedSubscribeToTopic = true;

                /* Generate a random number and calculate backoff value (in milliseconds) for
                 * the next connection retry.
                 * Note: It is recommended to seed the random number generator with a device-specific
                 * entropy source so that possibility of multiple devices retrying failed network operations
                 * at similar intervals can be avoided. */
                xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xRetryParams, uxRand(), &usNextRetryBackOff );

                if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
                {
                    LogError( ( "Server rejected subscription request. All retry attempts have exhausted. Topic=%s",
                                xTopicFilterContext[ ulTopicCount ].pcTopicFilter ) );
                }
                else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
                {
                    LogWarn( ( "Server rejected subscription request. Attempting to re-subscribe to topic %s.",
                               xTopicFilterContext[ ulTopicCount ].pcTopicFilter ) );
                    /* Backoff before the next re-subscribe attempt. */
                    vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );
                }

                break;
            }
        }

        configASSERT( xBackoffAlgStatus != BackoffAlgorithmRetriesExhausted );
    } while( ( xFailedSubscribeToTopic == true ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );
}
/*-----------------------------------------------------------*/

void prvMQTTPublishToTopic(char *msg, MQTTContext_t * pxMQTTContext )
{
    MQTTStatus_t xResult;
    MQTTPublishInfo_t xMQTTPublishInfo;
    //char msg[40] = mqttexampleMESSAGE;
    int len = strlen(msg);
    sprintf(&msg[len], " %d", (int)xTaskGetTickCount());
    len = strlen(msg);

    /***
     * For readability, error handling in this function is restricted to the use of
     * asserts().
     ***/

    /* Some fields are not used by this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xMQTTPublishInfo, 0x00, sizeof( xMQTTPublishInfo ) );

    /* This demo uses QoS0. */
    xMQTTPublishInfo.qos = MQTTQoS0;
    xMQTTPublishInfo.retain = false;
    xMQTTPublishInfo.pTopicName = mqttexampleTOPIC;
    xMQTTPublishInfo.topicNameLength = ( uint16_t ) strlen( mqttexampleTOPIC );
#if 0
    xMQTTPublishInfo.pPayload = mqttexampleMESSAGE;
    xMQTTPublishInfo.payloadLength = strlen( mqttexampleMESSAGE );
#else
    xMQTTPublishInfo.pPayload = msg;
    xMQTTPublishInfo.payloadLength = len;
#endif

    /* Send PUBLISH packet. Packet ID is not used for a QoS0 publish. */
    xResult = MQTT_Publish( pxMQTTContext, &xMQTTPublishInfo, 0U );

    configASSERT( xResult == MQTTSuccess );
}
/*-----------------------------------------------------------*/

void prvMQTTUnsubscribeFromTopic( MQTTContext_t * pxMQTTContext )
{
    MQTTStatus_t xResult;
    MQTTSubscribeInfo_t xMQTTSubscription[ mqttexampleTOPIC_COUNT ];

    /* Some fields not used by this demo so start with everything at 0. */
    ( void ) memset( ( void * ) &xMQTTSubscription, 0x00, sizeof( xMQTTSubscription ) );

    /* Get a unique packet id. */
    usSubscribePacketIdentifier = MQTT_GetPacketId( pxMQTTContext );

    /* Subscribe to the mqttexampleTOPIC topic filter. This example subscribes to
     * only one topic and uses QoS0. */
    xMQTTSubscription[ 0 ].qos = MQTTQoS0;
    xMQTTSubscription[ 0 ].pTopicFilter = mqttexampleTOPIC;
    xMQTTSubscription[ 0 ].topicFilterLength = ( uint16_t ) strlen( mqttexampleTOPIC );

    /* Get next unique packet identifier. */
    usUnsubscribePacketIdentifier = MQTT_GetPacketId( pxMQTTContext );

    /* Send UNSUBSCRIBE packet. */
    xResult = MQTT_Unsubscribe( pxMQTTContext,
                                xMQTTSubscription,
                                sizeof( xMQTTSubscription ) / sizeof( MQTTSubscribeInfo_t ),
                                usUnsubscribePacketIdentifier );

    configASSERT( xResult == MQTTSuccess );
}
/*-----------------------------------------------------------*/

void prvMQTTProcessResponse( MQTTPacketInfo_t * pxIncomingPacket,
                                    uint16_t usPacketId )
{
    uint32_t ulTopicCount = 0U;

    switch( pxIncomingPacket->type )
    {
        case MQTT_PACKET_TYPE_SUBACK:

            /* A SUBACK from the broker, containing the server response to our subscription request, has been received.
             * It contains the status code indicating server approval/rejection for the subscription to the single topic
             * requested. The SUBACK will be parsed to obtain the status code, and this status code will be stored in global
             * variable #xTopicFilterContext. */
            prvUpdateSubAckStatus( pxIncomingPacket );

            for( ulTopicCount = 0; ulTopicCount < mqttexampleTOPIC_COUNT; ulTopicCount++ )
            {
                if( xTopicFilterContext[ ulTopicCount ].xSubAckStatus != MQTTSubAckFailure )
                {
                    LogInfo( ( "Subscribed to the topic %s with maximum QoS %u.",
                               xTopicFilterContext[ ulTopicCount ].pcTopicFilter,
                               xTopicFilterContext[ ulTopicCount ].xSubAckStatus ) );
                }
            }

            /* Make sure ACK packet identifier matches with Request packet identifier. */
            configASSERT( usSubscribePacketIdentifier == usPacketId );
            break;

        case MQTT_PACKET_TYPE_UNSUBACK:
            LogInfo( ( "Unsubscribed from the topic %s.", mqttexampleTOPIC ) );
            /* Make sure ACK packet identifier matches with Request packet identifier. */
            configASSERT( usUnsubscribePacketIdentifier == usPacketId );
            break;

        case MQTT_PACKET_TYPE_PINGRESP:

            /* Nothing to be done from application as library handles
             * PINGRESP with the use of MQTT_ProcessLoop API function. */
            LogWarn( ( "PINGRESP should not be handled by the application "
                       "callback when using MQTT_ProcessLoop.\n" ) );
            break;

        /* Any other packet type is invalid. */
        default:
            LogWarn( ( "prvMQTTProcessResponse() called with unknown packet type:(%02X).",
                       pxIncomingPacket->type ) );
    }
}

/*-----------------------------------------------------------*/

void prvMQTTProcessIncomingPublish( MQTTPublishInfo_t * pxPublishInfo )
{
    configASSERT( pxPublishInfo != NULL );

    /* Process incoming Publish. */
    LogInfo( ( "Incoming QoS : %d\n", pxPublishInfo->qos ) );

    /* Verify the received publish is for the we have subscribed to. */
    if( ( pxPublishInfo->topicNameLength == strlen( mqttexampleTOPIC ) ) &&
        ( 0 == strncmp( mqttexampleTOPIC, pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength ) ) )
    {
        LogInfo( ( "Incoming Publish Topic Name: %.*s matches subscribed topic.\r\n"
                   "Incoming Publish Message : %.*s",
                   pxPublishInfo->topicNameLength,
                   (const char *) pxPublishInfo->pTopicName,
                   pxPublishInfo->payloadLength,
				   (const char *) pxPublishInfo->pPayload ) );
    }
    else
    {
        LogInfo( ( "Incoming Publish Topic Name: %.*s does not match subscribed topic.",
                   pxPublishInfo->topicNameLength,
                   pxPublishInfo->pTopicName ) );
    }
}

/*-----------------------------------------------------------*/

void prvEventCallback( MQTTContext_t * pxMQTTContext,
                              MQTTPacketInfo_t * pxPacketInfo,
                              MQTTDeserializedInfo_t * pxDeserializedInfo )
{
    /* The MQTT context is not used for this demo. */
    ( void ) pxMQTTContext;

    if( ( pxPacketInfo->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
    {
        prvMQTTProcessIncomingPublish( pxDeserializedInfo->pPublishInfo );
    }
    else
    {
        prvMQTTProcessResponse( pxPacketInfo, pxDeserializedInfo->packetIdentifier );
    }
}

/*-----------------------------------------------------------*/

uint32_t prvGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

/*-----------------------------------------------------------*/
