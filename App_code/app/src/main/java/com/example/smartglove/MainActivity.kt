package com.example.smartglove

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.os.Build
import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.lifecycle.lifecycleScope
import com.example.smartglove.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Locale

class MainActivity : AppCompatActivity(), TextToSpeech.OnInitListener {
    private lateinit var binding: ActivityMainBinding
    private lateinit var bluetoothService: BluetoothService
    private var isConnected = false
    private var textToSpeech: TextToSpeech? = null
    private var isTtsReady = false

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
            if (isConnected) {
                disconnectFromDevice()
            } else {
                connectToDevice()
            }
        }

        binding.btnCalibrate.setOnClickListener {
            if (isConnected) {
                sendCommand(Constants.CMD_CALIBRATE)
            }
        }

        binding.btnAddGesture.setOnClickListener {
            if (isConnected) {
                sendAddGestureCommand()
            }
        }

        binding.btnClear.setOnClickListener {
            if (isConnected) {
                sendCommand(Constants.CMD_CLEAR)
            }
        }
    }

    private fun sendAddGestureCommand() {
        val letter = binding.etLetter.text.toString().trim()
        val text = binding.etText.text.toString().trim()

        if (letter.isNotEmpty() && text.isNotEmpty()) {
            val touchVal = if (currentTouch.contains("1")) "1" else "0"
            // Layout protocol: ADD|letter|touch|bendX|bendZ|threshold|text
            val command = "ADD|$letter|$touchVal|$currentBendX|$currentBendZ|20.0|$text"
            sendCommand(command)
            binding.etLetter.text?.clear()
            binding.etText.text?.clear()
        } else {
            Toast.makeText(this, "Fill all fields", Toast.LENGTH_SHORT).show()
        }
    }

    private fun sendCommand(cmd: String) {
        lifecycleScope.launch(Dispatchers.IO) {
            val success = bluetoothService.sendData(cmd)
            if (!success) {
                withContext(Dispatchers.Main) {
                    Toast.makeText(this@MainActivity, "Send Failed", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun observeBluetoothData() {
        bluetoothService.messageFromDevice.observe(this) { message ->
            message?.let { handleIncomingMessage(it) }
        }

        bluetoothService.connectionStatus.observe(this) { status ->
            status?.let {
                if (it.contains("Disconnected", ignoreCase = true)) {
                    handleDisconnectionUI()
                }
            }
        }
    }

    private fun handleIncomingMessage(message: String) {
        if (message.startsWith(Constants.MSG_PLAY)) {
            val textToSpeak = message.removePrefix(Constants.MSG_PLAY)
            speakText(textToSpeak)
            return
        }

        if (message.startsWith(Constants.MSG_SENSOR) || message.startsWith(Constants.MSG_CALIBRATE_RESULT)) {
            val parts = message.split("|")
            if (parts.size >= 4) {
                currentTouch = parts[1]
                currentBendX = parts[2]
                currentBendZ = parts[3]
                binding.tvSensorData.text = "Touch: $currentTouch | X: $currentBendX Z: $currentBendZ"
            }
            return
        }

        if (message.startsWith(Constants.MSG_SUCCESS)) {
            val successMsg = message.removePrefix(Constants.MSG_SUCCESS)
            Toast.makeText(this, successMsg, Toast.LENGTH_SHORT).show()
        }
    }

    private fun connectToDevice() {
        val adapter = BluetoothAdapter.getDefaultAdapter()
        if (adapter == null) {
            Toast.makeText(this, "Bluetooth not supported", Toast.LENGTH_SHORT).show()
            return
        }

        val bondedDevices: Set<BluetoothDevice> = adapter.bondedDevices
        val device = bondedDevices.find { it.name == Constants.DEVICE_NAME }

        if (device != null) {
            binding.tvConnectionStatus.text = "Connecting..."
            binding.btnConnect.isEnabled = false

            // Run connection routines away from the Main UI Thread
            lifecycleScope.launch(Dispatchers.IO) {
                val success = bluetoothService.connectToDevice(device.address)
                withContext(Dispatchers.Main) {
                    binding.btnConnect.isEnabled = true
                    if (success) {
                        isConnected = true
                        binding.btnConnect.text = "Disconnect"
                        binding.tvConnectionStatus.text = "Connected"
                        toggleUI(true)
                        bluetoothService.startReading()
                    } else {
                        binding.tvConnectionStatus.text = "Connection Failed"
                        Toast.makeText(this@MainActivity, "Failed to connect", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        } else {
            Toast.makeText(this, "Pair with ${Constants.DEVICE_NAME} first", Toast.LENGTH_LONG).show()
        }
    }

    private fun disconnectFromDevice() {
        bluetoothService.disconnect()
        handleDisconnectionUI()
    }

    private fun handleDisconnectionUI() {
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
        if (isTtsReady) {
            textToSpeech?.speak(text, TextToSpeech.QUEUE_FLUSH, null, null)
        }
    }

    override fun onInit(status: Int) {
        if (status == TextToSpeech.SUCCESS) {
            val result = textToSpeech?.setLanguage(Locale.US)
            if (result != TextToSpeech.LANG_MISSING_DATA && result != TextToSpeech.LANG_NOT_SUPPORTED) {
                isTtsReady = true
            }
        }
    }

    private fun checkPermissionsAndBluetooth() {
        val permissions = if (Build.VERSION.SDK_INT >= 31) {
            arrayOf("android.permission.BLUETOOTH_CONNECT", "android.permission.BLUETOOTH_SCAN")
        } else {
            arrayOf("android.permission.BLUETOOTH", "android.permission.ACCESS_FINE_LOCATION")
        }
        ActivityCompat.requestPermissions(this, permissions, 1)
    }

    override fun onDestroy() {
        super.onDestroy()
        bluetoothService.disconnect()
        textToSpeech?.shutdown()
    }
}