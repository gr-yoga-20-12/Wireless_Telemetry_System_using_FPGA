# FPGA Wireless Telemetry & Alarm System 🚨

## Overview 🧠

This project transforms an **EDGE Zynq-7000 SoC FPGA board** into an active Internet of Things (IoT) environmental monitoring system. 

It reads live analog data from onboard temperature and light sensors using the Zynq's internal XADC, evaluates the data against safety thresholds, and triggers a physical hardware buzzer if conditions become critical. Simultaneously, it streams a live, formatted telemetry feed to a mobile application over Wi-Fi using TCP/IP and JSON.

## Key Features ⚙️

* **Real-Time Analog Sensing:** Reads physical room temperature via an LM35 sensor and ambient light via an LDR.
* **Hardware Alarm System:** Automatically triggers an onboard physical buzzer when the temperature exceeds threshold value(38.0°C).
* **Wireless Telemetry Pipeline:** Uses the onboard ESP32 module to open a TCP socket to a mobile phone, streaming live data continuously.
* **Robust Software Polling:** Bypasses complex hardware sequencers in favor of a rock-solid C-based polling loop for guaranteed, freeze-proof sensor data capture.
* **Hardware Calibration:** Implements custom C-level math to counteract the EDGE board's protective voltage-divider circuits, ensuring true Celsius and brightness percentage readings.

## Hardware Setup 🛠 💡

To replicate this project on the EDGE Zynq board, you must configure the physical hardware jumpers correctly to connect the analog sensors to the FPGA pins.

1. **Temperature Sensor (LM35):** Place a jumper wire connecting the `TEMP` pin to the `2nd pin (AD2)` on the J15 ADC Header.
2. **Light Sensor (LDR):** Place a jumper wire connecting the `LDR` pin to the `10th pin (AD10)` on the J15 ADC Header.
3. **Wi-Fi Module:** Place a jumper at `E` and the center pin of `J8` to provide power to the onboard ESP32 module.

## System Architecture 💻 

### 1. Vivado Block Design (Hardware Layer)

The custom FPGA hardware design requires three main IP blocks:
* **XADC Wizard:** Configured to read external analog channels `VAUX2` (for Temp) and `VAUX10` (for LDR).
* **AXI UART Lite:** Configured to communicate serial AT commands to the onboard ESP32 Wi-Fi module.
* **AXI GPIO (Single Channel):** A 1-bit digital output wired to the onboard Buzzer.

### 2. Control Logic (Software Layer)

The Vitis C application runs a continuous loop that evaluates the environment:
* Calculates accurate Celsius by reading the raw 12-bit ADC data, fixing the Zynq's 16-bit left-justified alignment, and multiplying by `3.3x` to reverse the board's voltage protection.
* Converts the raw LDR voltage into a clean `0% to 100%` brightness scale.
* Evaluates the safety threshold:
  * If **Temp < 38.0°C**: Status is `"NORMAL"`, Buzzer is **OFF**.
  * If **Temp >= 38.0°C**: Status is `"CRITICAL"`, Buzzer is **ON**.

## The Telemetry Pipeline 📡 

Data travels from the FPGA to the mobile application`(third-party app)` in three distinct stages:
1. **Board to ESP8266 (UART):** Standard serial communication sending AT commands and raw text.
2. **ESP8266 to Phone (TCP/IPv4):** A direct, continuous TCP socket connection over a local Wi-Fi hotspot, ensuring reliable packet delivery.
3. **Data Format (JSON):** A lightweight JSON payload, making it incredibly easy for downstream mobile applications or web dashboards to parse the variables. 

# 📂 Project Structure

```
FPGA Telemetry System/
│
├──Vivado/    <- Download this files and save in one folder
     └──HDL_Project.cache
     └──HDL_Project.gen
     └──HDL_Project.hw
     └──HDL_Project.runs
     └──HDL_Project.srcs
     └──HDL_Project.xpr
     └──vivado.jou
│
├──Vitis/    <- Download this filesa and save in another folder
     └──component_design_1
     └──platform_design_1
     └──comp-info.json
├──HDL_Demonstration_Video.mp4
├──IP_Block_Diagram.pdf
│
└──Readme.md
```


## Sample Live Output stream: 📤

* {"Temp":35.1,"LDR":4,"Status":"NORMAL"}
* {"Temp":38.5,"LDR":5,"Status":"CRITICAL"}

## How to Run the Project 🚀

1. **Configure Hardware:** Ensure all physical jumpers (TEMP, LDR, Wi-Fi) are placed on the EDGE board.
2. **Generate Bitstream:** In Vivado, synthesize the block design containing the XADC, AXI UART, and AXI GPIO, and export the hardware (.xsa file).
3. **Update Vitis Code:** Open Vitis, update your platform with the new .xsa, and open the C code.
4. **Set IP Address:** In the C code, update the #define PHONE_IP "192.168.X.X" macro to match the current IP address of your mobile phone on the hotspot network.
5. **Start TCP Server:** Open your mobile TCP server application and set it to listen on port 8080.
6. **Deploy:** Clean and build the Application in Vitis. Press PS-RST on the FPGA board, and click Run.
