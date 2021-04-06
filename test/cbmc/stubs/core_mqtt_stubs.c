/*
 * coreMQTT-Agent V1.0.0
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
 */

/**
 * @file core_mqtt_stubs.h
 * @brief Stub functions to interact with queues.
 */

#include "core_mqtt.h"
#include <string.h>


static bool isValidIncomingMqttPacket( uint8_t packetType )
{
    bool isValid = false;

    switch( packetType )
    {
        case MQTT_PACKET_TYPE_PUBACK:
        case MQTT_PACKET_TYPE_PUBCOMP:
        case MQTT_PACKET_TYPE_SUBACK:
        case MQTT_PACKET_TYPE_UNSUBACK:
        case MQTT_PACKET_TYPE_CONNACK:
        case MQTT_PACKET_TYPE_PUBLISH:
        case MQTT_PACKET_TYPE_PUBREC:
        case MQTT_PACKET_TYPE_PUBREL:
        case MQTT_PACKET_TYPE_PINGRESP:
            isValid = true;
            break;

        default:
            break;
    }

    return isValid;
}

MQTTStatus_t MQTT_Init( MQTTContext_t * pContext,
                        const TransportInterface_t * pTransportInterface,
                        MQTTGetCurrentTimeFunc_t getTimeFunction,
                        MQTTEventCallback_t userCallback,
                        const MQTTFixedBuffer_t * pNetworkBuffer )
{
    MQTTStatus_t status;

    __CPROVER_assert( pContext != NULL,
                      "MQTT Context is not NULL." );

    ( void ) memset( pContext, 0x00, sizeof( MQTTContext_t ) );

    pContext->connectStatus = MQTTNotConnected;
    pContext->transportInterface = *pTransportInterface;
    pContext->getTime = getTimeFunction;
    pContext->appCallback = userCallback;
    pContext->networkBuffer = *pNetworkBuffer;

    /* Zero is not a valid packet ID per MQTT spec. Start from 1. */
    pContext->nextPacketId = 1;

    return status;
}

MQTTStatus_t MQTT_ProcessLoop( MQTTContext_t * pContext,
                               uint32_t timeoutMs )
{
    MQTTStatus_t status;
    MQTTPacketInfo_t * pPacketInfo;
    MQTTDeserializedInfo_t * pDeserializedInfo;
    size_t remainingLength;
    MQTTPublishInfo_t * pPublishInfo;
    uint16_t topicNameLength;
    size_t payloadLength;
    static bool terminate = false;

    __CPROVER_assert( pContext != NULL,
                      "MQTT Context is not NULL." );

    /* Only one packet per MQTT_ProcessLoop is received for the proof and it will be enough
     * to prove the memory safety. The second invocation of MQTT_ProcessLoopreturns without
     * invoking the appCallback. */
    if( terminate == false )
    {
        pPacketInfo = malloc( sizeof( MQTTPacketInfo_t ) );
        __CPROVER_assume( pPacketInfo != NULL );
        __CPROVER_assume( isValidIncomingMqttPacket( pPacketInfo->type ) );
        __CPROVER_assume( remainingLength < 64U );

        /* SUBACK codes will be after 2 bytes. */
        if( pPacketInfo->type == MQTT_PACKET_TYPE_SUBACK )
        {
            __CPROVER_assume( remainingLength > 2U );
        }

        pPacketInfo->pRemainingData = malloc( remainingLength );
        __CPROVER_assume( pPacketInfo->pRemainingData != NULL );
        pPacketInfo->remainingLength = remainingLength;

        pDeserializedInfo = malloc( sizeof( MQTTDeserializedInfo_t ) );
        __CPROVER_assume( pDeserializedInfo != NULL );

        if( pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH )
        {
            pPublishInfo = malloc( sizeof( MQTTPublishInfo_t ) );
            __CPROVER_assume( pPublishInfo != NULL );

            __CPROVER_assume( topicNameLength < 32U );
            pPublishInfo->pTopicName = malloc( topicNameLength );
            __CPROVER_assume( pPublishInfo->pTopicName != NULL );
            pPublishInfo->topicNameLength = topicNameLength;

            __CPROVER_assume( payloadLength < 64U );
            pPublishInfo->pPayload = malloc( payloadLength );
            __CPROVER_assume( pPublishInfo->pPayload != NULL );
            pPublishInfo->payloadLength = payloadLength;

            pDeserializedInfo->pPublishInfo = pPublishInfo;
        }

        __CPROVER_assume( pDeserializedInfo->packetIdentifier > 0U );

        /* Invoke event callback. */
        pContext->appCallback( pContext,
                               pPacketInfo,
                               pDeserializedInfo );

        terminate = true;
    }

    return status;
}