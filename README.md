# ESP32-S3 Boje Control Panel (Panel Do Bojki)

A user-interface application built for the Waveshare ESP32-S3 RGB LCD Touch Display module using **LVGL v8**. This application acts as a control panel for an underwater buoy system, providing step-by-step state machine navigation, interactive mode selection, manual control sending, test execution, and serial communication protocols via USB.

---

## 🛠️ Requirements & Libraries

To compile and upload this project using **Arduino IDE** (or PlatformIO), ensure you have the following setup:

### Core Board Support
* **Board Package:** ESP32 Board Package by Espressif Systems (v3.x or later)
* **Target Board:** `ESP32S3 Dev Module` (or equivalent Waveshare ESP32-S3 LCD target)

### Required Libraries
1. **LVGL (Light and Versatile Graphics Library)**
   * **Required Version:** `v8.3.x` – `v8.4.0` (*Note: Do **NOT** use LVGL v9.x as driver architectures differ*).
   * **Configuration:** Ensure your `lv_conf.h` enables required widgets (`lv_msgbox`, `lv_textarea`, `lv_keyboard`, `lv_btn`, `lv_label`, `lv_timer`).
2. **ESP32 LCD & Touch Drivers (Included in Project repository)**
   * `esp_lcd` (ESP-IDF / Arduino integrated component for RGB LCD panels)
   * `touch_gt911` (Driver for GT911 capacitive touch panel)
   * `waveshare_esp32_s3_rgb_lcd` / `lvgl_port` (Board support package for display initialization and VSYNC notifications)

---

## 🚀 Application Structure & Functionality

The interface functions as a state machine with a dedicated boot screen, main menu, and mode-specific sub-interfaces.

### 1. Boot Screen (Loading)
* Displays a dark ocean theme background on boot.
* Shows a loading message / splash logo.
* Automatically transitions to the Main Menu after 2 seconds via a non-blocking `lv_timer`.

### 2. Main Menu
Presents 5 distinct control modes:
1. **MANUAL MODE**
2. **TEST MODE**
3. **MISSION MODE**
4. **GET DATA MODE**
5. **LIVE DATA MODE**

When a mode is entered, the panel sends a 1-byte entry code over `Serial` (USB) to notify the connected buoy station.

#### USB Entry/Exit Commands Table:
| Mode | Entry Command (HEX) | Exit Command (HEX) |
| :--- | :---: | :---: |
| **MANUAL MODE** | `0x02` | `0xFF` |
| **TEST MODE** | `0x04` | `0xFF` |
| **MISSION MODE** | `0x06` | `0xFF` |
| **GET DATA MODE** | `0x08` | `0xFF` |
| **LIVE DATA MODE** | `0x0A` | `0xFF` |

---

## 🕹️ Mode Specification & USB Protocols

### 🔹 MANUAL MODE
* **Purpose:** Direct manual motor/thruster control.
* **Controls:** 
  * Text Area for RPM input (automatically clamped between `0` and `240`).
  * On-screen numeric keyboard (`LV_KEYBOARD_MODE_NUMBER`).
  * **UP** and **DOWN** hold-to-run action buttons (`dir = 1` for UP, `dir = 2` for DOWN, `dir = 0` when released).
* **Serial Transmission:** Sends a 9-byte packed structure (`manual_control_t`) continuously every **10 ms**:

```cpp
typedef struct __attribute__((packed)) {
    uint8_t ID;      // 0x02 (Manual Mode Identifier)
    uint8_t size;    // 9 (Total struct size in bytes)
    uint8_t dir;     // 1 = UP, 2 = DOWN, 0 = STOP
    uint16_t rpm;    // Target speed (0-240 RPM)
    float rounds;    // Number of rotations (Default: 1000.0)
} manual_control_t;
```

---

### 🔹 TEST MODE
* **Purpose:** Configurable test submersions with safety lockouts.
* **Parameters:**
  * **Time [s]:** Submersion time in seconds (automatically converted to milliseconds when transmitted).
  * **RPM:** Target rotation speed (0–240 RPM).
  * **Rounds:** Total motor rotations to complete.
* **Safety & Locking:**
  1. Clicking **START TEST** opens a confirmation modal dialog (`lv_msgbox`).
  2. Confirming sends a single 13-byte packed structure (`test_mode_params_t`).
  3. **Interface Lock:** The user is prevented from leaving TEST MODE (`WROĆ` button is disabled) until the buoy station transmits the exact matching response packet back over USB.
* **Serial Transmission:**

```cpp
typedef struct __attribute__((packed)) {
    uint8_t ID;      // 0x04 (Test Mode Identifier)
    uint8_t size;    // 13 (Total struct size in bytes)
    uint8_t dir;     // Default: 0
    uint16_t rpm;    // Target speed (0-240 RPM)
    float rounds;    // Total motor revolutions
    uint32_t time;   // Time underwater in milliseconds
} test_mode_params_t;
```

---

## 📁 File Architecture

* **`marines_esp32s3lcd.ino`**: Main application code. Contains UI layout definitions, event callbacks, numeric keyboard handling, USB packet processing, and state machine transitions.
* **`lvgl_port.cpp` / `lvgl_port.h`**: Hardware abstraction layer. Handles display double-buffering, display flushing callbacks (`flush_cb`), touch driver input polling (`indev_read`), and FreeRTOS task concurrency locks (`lvgl_port_lock`).
* **`rgb_lcd_port.cpp` / `touch.cpp` / `gt911.cpp`**: Hardware-specific peripheral driver initializations for Waveshare ESP32-S3 RGB LCD screen.

---

## ⚙️ How to Flash & Run

1. Open `marines_esp32s3lcd.ino` in **Arduino IDE**.
2. Select target board: `ESP32S3 Dev Module`.
3. Configure Board Options:
   * **PSRAM:** Enabled (OPI PSRAM depending on board model)
   * **Flash Size:** 8MB or 16MB
   * **USB CDC On Boot:** Enabled / Disabled depending on serial monitoring preference.
4. Verify that **LVGL 8.4.x** is installed in the Library Manager.
5. Compile and upload to the board.
