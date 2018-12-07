/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <events/mbed_events.h>

#include <mbed.h>
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ButtonService.h"


static InterruptIn button(PA_0); //BLE_BUTTON_PIN_NAME


////////////// binbeat
DigitalOut enableDebugPortRX(PB_15, 0); //enable RX on debug port
DigitalOut debugPortForceOff(PD_10, 1); // uses inverse logic!
DigitalOut debugPortForceOn(PD_11, 1);
DigitalOut disableVc(PC_6, 0);  //enable vcontrolled
DigitalOut enable3v3(PG_1, 1);
DigitalOut enable5v(PA_1, 1);

void powerup(void) {
    wait_ms(100);
    printf("Board ready\r\n");
}
///

static EventQueue eventQueue(/* event count */ 10 * EVENTS_EVENT_SIZE);

const static char     DEVICE_NAME[] = "Button";
static const uint16_t uuid16_list[] = {ButtonService::BUTTON_SERVICE_UUID};

ButtonService *buttonServicePtr;

void buttonPressedCallback(void)
{
    eventQueue.call(Callback<void(bool)>(buttonServicePtr, &ButtonService::updateButtonState), true);
}

void buttonReleasedCallback(void)
{
    eventQueue.call(Callback<void(bool)>(buttonServicePtr, &ButtonService::updateButtonState), false);
}

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    printf("disconnected, restarting advertising...\r\n");
    BLE::Instance().gap().startAdvertising(); // restart advertising
}

void connectionCallback(const Gap::ConnectionCallbackParams_t *params)
{
    printf("connected...\r\n");
}

void onBleInitError(BLE &ble, ble_error_t error)
{
    /* Initialization error handling should go here */
    printf("onBleInitError %d: %s\r\n", (uint8_t)error, BLE::errorToString(error));
}

void printMacAddress()
{
    /* Print out device MAC address to the console*/
    Gap::AddressType_t addr_type;
    Gap::Address_t address;
    BLE::Instance().gap().getAddress(&addr_type, address);
    printf("DEVICE MAC ADDRESS: ");
    for (int i = 5; i >= 1; i--){
        printf("%02x:", address[i]);
    }
    printf("%02x\r\n", address[0]);
}

void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE&        ble   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        /* In case of error, forward the error handling to onBleInitError */
        onBleInitError(ble, error);
        return;
    }

    /* Ensure that it is the default instance of BLE */
    if(ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        printf("BLE instance not default instance!\r\n");
        return;
    }

    ble.gap().onDisconnection(disconnectionCallback);
    ble.gap().onConnection(connectionCallback);

    button.fall(buttonReleasedCallback);
    button.rise(buttonPressedCallback);

    /* Setup primary service. */
    //buttonServicePtr = new ButtonTestService();
    buttonServicePtr = new ButtonService(ble, false /* initial value for button pressed */);

    /* setup advertising */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* 1000ms. */
    ble.gap().startAdvertising();

    printMacAddress();
}

void scheduleBleEventsProcessing(BLE::OnEventsToProcessCallbackContext* context) {
    BLE &ble = BLE::Instance();
    eventQueue.call(Callback<void()>(&ble, &BLE::processEvents));
}

int main()
{
    powerup();

    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(scheduleBleEventsProcessing);
    ble.init(bleInitComplete);

    eventQueue.dispatch_forever();

    return 0;
}
