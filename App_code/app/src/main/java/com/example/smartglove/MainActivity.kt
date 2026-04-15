package com.example.smartglove

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.util.Log
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.example.smartglove.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Locale

class MainActivity : AppCompatActivity(), TextToSpeech.OnInitListener {

    private lateinit var binding: ActivityMainBinding
    private lateinit var bluetoothService: BluetoothService
    private lateinit var textToSpeech: TextToSpeech

    private var isConnected = false
    private var pairedDeviceAddress: String? = null
    private var currentTouch = "0"
    private var currentBendX = "0.0"
    private var currentBendZ = "0.0"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        bluetoothService = BluetoothService()
        textToSpeech = TextToSpeech(this, this)

        setupUI()
        checkPermissionsAndBluetooth()
        observeBluetoothData()
    }

    private fun setupUI() {
        binding.btnConnect.setOnClickListener {
            if (isConnected) disconnectFromDevice() else connectToDevice()
        }

        binding.btnCalibrate.setOnClickListener {
            if (isConnected) sendCommand(Constants.CMD_CALIBRATE)
        }

        binding.btnAddGesture.setOnClickListener {
            if (isConnected) sendAddGestureCommand()
        }

        binding.btnClear.setOnClickListener {
            if (isConnected) sendCommand(Constants.CMD_CLEAR)
        }
    }

    private fun sendAddGestureCommand() {
        val letter = binding.etLetter.text.toString().trim()
        val text = binding.etText.text.toString().trim()
        if (letter.isEmpty() || text.isEmpty()) {
            Toast.makeText(this, "Fill all fields", Toast.LENGTH_SHORT).show()
            return
        }

        val touchVal = if (currentTouch.contains("1")) "1" else "0"
        val command = "ADD|$letter|$touchVal|$currentBendX|$currentBendZ|20.0|$text"
        sendCommand(command)

        binding.etLetter.text?.clear()
        binding.etText.text?.clear()
    }

    private fun sendCommand(cmd: String) {
        lifecycleScope.launch(Dispatchers.IO) {
            val success = bluetoothService.sendData(cmd)
            withContext(Dispatchers.Main) {
                if (!success) Toast.makeText(this@MainActivity, "Send Failed", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun observeBluetoothData() {
        bluetoothService.messageFromDevice.observe(this) { message ->
            handleIncomingMessage(message)
        }
        bluetoothService.connectionStatus.observe(this) { status ->
            if (status.contains("Disconnected")) disconnectFromDevice()
        }
    }

    private fun handleIncomingMessage(message: String) {
        when {
            message.startsWith(Constants.MSG_PLAY) -> {
                speakText(message.removePrefix(Constants.MSG_PLAY))
            }
            message.startsWith(Constants.MSG_SENSOR) || message.startsWith(Constants.MSG_CALIBRATE_RESULT) -> {
                val parts = message.split("|")
                if (parts.size >= 4) {
                    currentTouch = parts[1]
                    currentBendX = parts[2]
                    currentBendZ = parts[3]
                    binding.tvSensorData.text = "Touch: $currentTouch | X: $currentBendX Z: $currentBendZ"
                }
            }
            message.startsWith(Constants.MSG_SUCCESS) -> {
                Toast.makeText(this, message.removePrefix(Constants.MSG_SUCCESS), Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun connectToDevice() {
        val adapter = BluetoothAdapter.getDefaultAdapter()
        val device = adapter.bondedDevices.find { it.name == Constants.DEVICE_NAME }

        if (device != null) {
            lifecycleScope.launch {
                val success = bluetoothService.connectToDevice(device.address)
                if (success) {
                    isConnected = true
                    binding.btnConnect.text = "Disconnect"
                    binding.tvConnectionStatus.text = "Connected"
                    toggleUI(true)
                    bluetoothService.startReading()
                }
            }
        } else {
            Toast.makeText(this, "Pair with SmartGlove_ASL first", Toast.LENGTH_LONG).show()
        }
    }

    private fun disconnectFromDevice() {
        bluetoothService.disconnect()
        isConnected = false
        binding.btnConnect.text = "Connect"
        binding.tvConnectionStatus.text = "Disconnected"
        toggleUI(false)
    }

    private fun toggleUI(enabled: Boolean) {
        binding.btnCalibrate.isEnabled = enabled
        binding.btnAddGesture.isEnabled = enabled
        binding.btnClear.isEnabled = enabled
        binding.etLetter.isEnabled = enabled
        binding.etText.isEnabled = enabled
    }

    private fun speakText(text: String) {
        textToSpeech.speak(text, TextToSpeech.QUEUE_FLUSH, null, null)
    }

    override fun onInit(status: Int) {
        if (status == TextToSpeech.SUCCESS) textToSpeech.language = Locale.US
    }

    private fun checkPermissionsAndBluetooth() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
        } else {
            arrayOf(Manifest.permission.BLUETOOTH, Manifest.permission.ACCESS_FINE_LOCATION)
        }
        ActivityCompat.requestPermissions(this, permissions, 1)
    }

    override fun onDestroy() {
        super.onDestroy()
        bluetoothService.disconnect()
        textToSpeech.shutdown()
    }
}