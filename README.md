<img width="498" height="498" alt="CpuProcessorGIF" src="https://github.com/user-attachments/assets/b0c8c3ae-b35f-4a33-b70e-3546bc7d79a4" />

# SKEE3233-group-project

## SMART DOOR SYSTEM

This project is smart door automatic intelligent system using black pill STM32F401 microcontroller.The system is monitor rain ,object present and the manual button input then have 16x2LCD display, buzzer , 4 LED, and last sevro motor to open and close the door.

## Main features

- Rain detection using rain sensor module
- Object or person detection using IR obstacle avoidance sensor
- Manual doorbell input using push button interrupt
- LCD status display using 4-bit parallel mode
- Servo motor door control using PWM
- LED indicators for different system states
- Active buzzer for warning and doorbell sound
- State-machine control using switch-case structure
- External interrupt implementation using EXTI1
- SysTick delay implementation
- Register-level GPIO configuration using STM32 CMSIS

## Hardware Components💿
| No. | Component | Quantity | Purpose |
|---:|---|---:|---|
| 1 | STM32F401 Black Pill | 1 | Main microcontroller board |
| 2 | ST-Link V2 | 1 | Upload/debug program from Keil uVision |
| 3 | Rain sensor plate + LM393 module | 1 | Detect rain or water |
| 4 | IR obstacle avoidance sensor | 1 | Detect object or person near the door |
| 5 | Push button | 1 | Manual doorbell trigger |
| 6 | 16x2 LCD display | 1 | Display system status |
| 7 | Active buzzer module | 1 | Sound alarm/doorbell |
| 8 | Servo motor | 1 | Simulate door opening and closing |
| 9 | LED | 4 | Visual state indicators |
| 10 | 240Ω resistors | 4 | LED protection |
| 11 | Potentiometer 203 / 20kΩ | 1 | LCD contrast adjustment |
| 12 | Breadboard | 1 | Circuit prototyping |
| 13 | Jumper wires | Many | Electrical jumper connection |
| 14 | External 5V supply | 1 | External 5V supply for servo motor |
| 15 | USB cable | 1 | Power/programming connection |


