package com.example.smartglove

import java.util.UUID

object Constants {
    const val DEVICE_NAME = "SmartGlove_ASL"
    val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    // Message prefixes from device
    const val MSG_SENSOR = "SENSOR|"
    const val MSG_PLAY = "PLAY|"
    const val MSG_CONNECTED = "CONNECTED|"
    const val MSG_SUCCESS = "SUCCESS|"
    const val MSG_CALIBRATE_RESULT = "CALIBRATE_RESULT|"

    // Commands to send to device
    const val CMD_CALIBRATE = "CALIBRATE"
    const val CMD_CLEAR = "CLEAR"

    const val READ_INTERVAL_MS = 500L
}