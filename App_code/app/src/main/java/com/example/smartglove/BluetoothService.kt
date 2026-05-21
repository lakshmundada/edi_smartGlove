package com.example.smartglove

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.*
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStream
import java.nio.charset.StandardCharsets

class BluetoothService : CoroutineScope {
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var reader: BufferedReader? = null

    private val job = SupervisorJob()
    override val coroutineContext = Dispatchers.IO + job

    private val _messageFromDevice = MutableLiveData<String>()
    val messageFromDevice: LiveData<String> = _messageFromDevice

    private val _connectionStatus = MutableLiveData<String>()
    val connectionStatus: LiveData<String> = _connectionStatus

    /**
     * Connects to the device on a background I/O thread safely.
     */
    fun connectToDevice(address: String): Boolean {
        return try {
            val adapter = BluetoothAdapter.getDefaultAdapter() ?: return false
            val device: BluetoothDevice = adapter.getRemoteDevice(address)

            // Connect socket cleanly using SPP UUID
            val socket = device.createRfcommSocketToServiceRecord(Constants.SPP_UUID)
            bluetoothSocket = socket
            socket.connect()

            outputStream = socket.outputStream
            // Use buffered reader to read stream exactly line-by-line up to '\n'
            reader = BufferedReader(InputStreamReader(socket.inputStream, StandardCharsets.UTF_8))

            _connectionStatus.postValue("Connected")
            true
        } catch (e: Exception) {
            e.printStackTrace()
            disconnect()
            false
        }
    }

    /**
     * Reads incoming lines safely, handling EOF terminations instantly.
     */
    fun startReading() {
        launch {
            try {
                while (isActive) {
                    val currentReader = reader ?: break
                    val line = currentReader.readLine()

                    if (line != null) {
                        val trimmedLine = line.trim()
                        if (trimmedLine.isNotEmpty()) {
                            _messageFromDevice.postValue(trimmedLine)
                        }
                    } else {
                        // Stream returned null -> Remote side closed connection cleanly
                        _connectionStatus.postValue("Disconnected")
                        break
                    }
                }
            } catch (e: Exception) {
                // Any hardware connection rupture drops into this catch block safely
                _connectionStatus.postValue("Disconnected")
            }
        }
    }

    /**
     * Sends commands over Bluetooth with a trailing newline character.
     */
    fun sendData(data: String): Boolean {
        return try {
            val out = outputStream ?: return false
            val bytes = (data + "\n").toByteArray(StandardCharsets.UTF_8)
            out.write(bytes)
            out.flush()
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }

    /**
     * Closes resources completely without leaving leaked socket streams.
     */
    fun disconnect() {
        try { reader?.close() } catch (e: Exception) { e.printStackTrace() }
        try { outputStream?.close() } catch (e: Exception) { e.printStackTrace() }
        try { bluetoothSocket?.close() } catch (e: Exception) { e.printStackTrace() }

        bluetoothSocket = null
        outputStream = null
        reader = null
    }
}