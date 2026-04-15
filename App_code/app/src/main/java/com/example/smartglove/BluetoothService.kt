package com.example.smartglove

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothSocket
import android.util.Log
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.*
import java.io.InputStream
import java.io.OutputStream

class BluetoothService : CoroutineScope by CoroutineScope(Dispatchers.IO + SupervisorJob()) {

    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var inputStream: InputStream? = null

    private val _messageFromDevice = MutableLiveData<String>()
    val messageFromDevice: LiveData<String> get() = _messageFromDevice

    private val _connectionStatus = MutableLiveData<String>()
    val connectionStatus: LiveData<String> get() = _connectionStatus

    fun connectToDevice(address: String): Boolean {
        return try {
            val device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address)
            bluetoothSocket = device.createRfcommSocketToServiceRecord(Constants.SPP_UUID)
            bluetoothSocket?.connect()
            outputStream = bluetoothSocket?.outputStream
            inputStream = bluetoothSocket?.inputStream
            true
        } catch (e: Exception) {
            false
        }
    }

    fun startReading() {
        launch {
            val buffer = ByteArray(1024)
            while (isActive) {
                try {
                    val bytes = inputStream?.read(buffer) ?: -1
                    if (bytes > 0) {
                        val message = String(buffer, 0, bytes).trim()
                        _messageFromDevice.postValue(message)
                    }
                } catch (e: Exception) {
                    _connectionStatus.postValue("Disconnected")
                    break
                }
            }
        }
    }

    fun sendData(data: String): Boolean {
        return try {
            outputStream?.write((data + "\n").toByteArray())
            true
        } catch (e: Exception) {
            false
        }
    }

    fun disconnect() {
        bluetoothSocket?.close()
        inputStream = null
        outputStream = null
    }
}