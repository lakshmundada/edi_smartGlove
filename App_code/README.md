# Smart Glove Android App

An Android application for controlling a Smart Glove device via Classic Bluetooth (BluetoothSocket).

## Features

- ✅ Connects to Bluetooth device named "SmartGlove_ASL"
- ✅ All Bluetooth operations run on background thread using Coroutines (prevents ANR on Android 15)
- ✅ Calibrate button sends "CALIBRATE" command
- ✅ Two input fields for Letter and Text
- ✅ Add Gesture button sends formatted command: `ADD|letter|touch|bendX|bendZ|20.0|text`
- ✅ Real-time sensor data display
- ✅ Auto-read timer checks for incoming data every 500ms
- ✅ Message processing for: SENSOR|, PLAY|, CONNECTED|, SUCCESS|
- ✅ TextToSpeech integration for PLAY| messages
- ✅ Target SDK 34, minSDK 26
- ✅ All required Bluetooth permissions for Android 12+

## Project Structure

```
app/
├── src/main/
│   ├── java/com/example/smartglove/
│   │   ├── MainActivity.kt              # Main UI and user interactions
│   │   ├── BluetoothService.kt          # Bluetooth communication service
│   │   └── Constants.kt                 # App constants
│   ├── res/
│   │   ├── layout/
│   │   │   └── activity_main.xml        # Main UI layout
│   │   ├── values/
│   │   │   ├── strings.xml              # String resources
│   │   │   ├── colors.xml               # Color resources
│   │   │   └── themes.xml               # App theme
│   │   └── drawable/
│   │       └── sensor_data_background.xml
│   └── AndroidManifest.xml              # Permissions and app config
├── build.gradle.kts                     # Module-level build config
└── proguard-rules.pro                   # ProGuard rules

build.gradle.kts                         # Project-level build config
settings.gradle.kts                      # Gradle settings
gradle.properties                        # Gradle properties
```

## Build & Run

### Prerequisites
- Android Studio Hedgehog (2023.1.1) or newer
- JDK 17
- Android SDK with API 34

### Steps
1. Open project in Android Studio
2. Sync Gradle files
3. Build the project (Build → Make Project)
4. Run on a physical Android device (Bluetooth required)

**Note:** This app requires a physical device with Bluetooth. Emulators won't work.

## How to Use

### 1. Pair Your Device
- Make sure your "SmartGlove_ASL" device is powered on and discoverable
- Go to Android Settings → Bluetooth
- Pair with "SmartGlove_ASL"

### 2. Connect to Device
- Open the app
- Grant all necessary permissions when prompted
- Tap "Connect" button
- Wait for connection confirmation

### 3. Calibrate
- Once connected, tap "Calibrate" to send calibration command

### 4. Add Gestures
- Enter a letter in the "Letter" field (e.g., "A")
- Enter text in the "Text" field (e.g., "Hello")
- Tap "Add Gesture" to send the command

### 5. View Sensor Data
- Real-time sensor data will appear in the bottom section
- Updates automatically as data is received

## Technical Details

### Bluetooth Implementation
- Uses **Classic Bluetooth** (BluetoothSocket), NOT BLE
- SPP UUID: `00001101-0000-1000-8000-00805F9B34FB`
- Insecure RFCOMM socket for simplified connection
- All I/O operations on `Dispatchers.IO` coroutine scope
- UI updates posted to `Dispatchers.Main`

### Permissions

**Android 6.0 - 11 (API 23-30):**
- `BLUETOOTH`
- `BLUETOOTH_ADMIN`
- `ACCESS_FINE_LOCATION` (required for Bluetooth discovery)

**Android 12+ (API 31+):**
- `BLUETOOTH_CONNECT`
- `BLUETOOTH_SCAN`

### Message Protocol

**Commands Sent to Device:**
- `CALIBRATE` - Initiate calibration
- `ADD|letter|touch|bendX|bendZ|20.0|text` - Add new gesture

**Messages Received from Device:**
- `SENSOR|<data>` - Sensor readings (displayed in UI)
- `PLAY|<text>` - Text to be spoken (TextToSpeech)
- `CONNECTED|<status>` - Connection status update
- `SUCCESS|<message>` - Operation success confirmation

### Threading Model
```
User Action → lifecycleScope.launch(Dispatchers.IO) 
           → Bluetooth operation 
           → withContext(Dispatchers.Main) 
           → Update UI
```

This ensures no blocking operations on the main thread, preventing ANR on Android 15.

### Timer Implementation
- Background coroutine checks for incoming data every 500ms
- Uses `inputStream.available()` to detect new data
- Automatically processes messages via `ProcessMessage()`

## Troubleshooting

### App crashes on startup
- Check that Bluetooth is enabled on your device
- Ensure all permissions are granted

### Cannot connect to device
- Verify device is named exactly "SmartGlove_ASL"
- Make sure device is paired in Android Bluetooth settings
- Try turning Bluetooth off and on again

### No sensor data received
- Check that the glove is properly connected to the device
- Verify the device is transmitting data in the correct format

### TextToSpeech not working
- Check device volume settings
- Ensure TTS engine is installed and configured

## Testing on Android 15

The app is specifically designed to prevent ANR (Application Not Responding) on Android 15:
- All Bluetooth operations use Coroutines with `Dispatchers.IO`
- No blocking calls on the main thread
- Proper lifecycle management

## Dependencies

- **Kotlinx Coroutines** (1.7.3) - Async operations
- **AndroidX Lifecycle** (2.7.0) - LiveData, ViewModelScope
- **Material Components** (1.11.0) - UI components
- **ViewBinding** - Type-safe view access

## License

This project is provided as-is for educational purposes.
