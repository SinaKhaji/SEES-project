# Acoustic Leak Localisation with Commodity Hardware

**Software and Engineering for Embedded Systems**

Mohammadmahdi (Sina) Khaji · Ivanna Morales Roman

## Introduction

Leakages in water pipelines represent a critical challenge, and acoustic methods have emerged as the dominant approach for addressing them. For engineering applications, localisation is as critical as detection: utilities need to know precisely where to excavate, not merely that a leak exists somewhere in the network.

This project adopts a simple and accessible approach, modelled on the experimental setup and cross-correlation technique proposed by Lah et al. A single uniform pipe section is instrumented with two accelerometers that stream data to an ESP32 microcontroller.

**Objective:** *to assess whether this minimal, low-cost configuration can reliably localise a leak, and to characterise its feasibility and practical limitations.*

## Background

- **Cross-correlation:** the time shift that produces the highest correlation peak between the two sensor signals corresponds to the propagation delay between the sensors.
- **Leak position:** `x = (d + v · Δt) / 2`, where `d` is the sensor separation, `v` the acoustic wave speed (≈ 565 m/s for water-filled PVC) and `Δt` the measured delay.
- **Buffer method (delay correction):** a no-leak baseline and an empirical buffer time are subtracted to remove the systematic offset caused by background noise: `Δt = t_actual − t_no-leak − t_buffer`

## System overview

| Component | Role |
|---|---|
| Adafruit Circuit Playground Bluefruit ×2 | BLE sensor nodes with onboard LIS3DH accelerometer (nRF52840) |
| ESP32-S3 | Central receiver: data acquisition and synchronised capture of both sensor streams |
| 3×AAA battery holders | Independent power for each sensor node |
| 2 m PVC pipe section | Test medium, sensors 1.5 m apart |
| Zip ties and metal backing plate | Secure sensors to the pipe surface |

## Repository structure

```
src/
├── BLE_MCU/main/        # Receiver node (ESP32-S3)
│   ├── main.ino
│   └── BLEConnector.h
└── BLE_ACC/main/        # Sensor node (Circuit Playground Bluefruit)
    └── main.ino
```

## Communication mechanism

**Receiver node (ESP32-S3)**
- Dual BLE central device that connects to both sensor nodes at the same time
- Two FreeRTOS tasks, each pinned to its own core with `xTaskCreatePinnedToCore()`
- A mutex (`bleMutex`) protects shared BLE resources from concurrent access
- MTU increased to 517 bytes, so each notification packet carries multiple samples
- After a 5-minute session, it disconnects and enters deep sleep, then wakes up automatically

**Sensor node (Circuit Playground Bluefruit)**
- Samples x-, y- and z-axis acceleration every 200 ms (5 Hz)
- Static buffer of 10 000 samples (16 bytes each, ≈ 156 kB, about 61% of the SRAM)
- Unique BLE name derived from the MCU ID (`<id>_ACC`), with automatic re-advertising on disconnect
- Flushes the buffer over BLE UART while connected, then clears it

## Experimental procedure

- **No-leak condition:** 5 recordings of 5 minutes each, with water flowing normally through the pipe
- **Leak condition:** 5 recordings of 5 minutes each with a small controlled leak, using the same sensor placement and sampling configuration
- Cross-correlation was done offline in Python with `numpy.correlate`

## Results

| Quantity | Value |
|---|---|
| t_no-leak | −8.210 s |
| t_actual | 39.045 s |
| t_buffer | 47.257 s |
| Corrected Δt | ≈ −0.00163 s |
| **Estimated leak position** | **34.3 cm from sensor 1** |
| True leak position | 34.3 cm from sensor 1 |

**Key takeaway:** the accelerometers capture a clear difference between leak and no-leak conditions. With the buffer method, the estimated leak position matches the true drilled location. However, the buffer method needs a known leak position for calibration.

## Limitations

- On the Circuit Playground Bluefruit, the accelerometer and the BLE radio cannot operate at the same time: the BLE stack takes over the I²C bus during transmission.
- On the ESP32-S3, the shared BLE stack prevents full use of both cores.
- Both platforms are therefore limited to about **5 Hz**, while small leak signatures reach **1–10 kHz** (Choi et al. used 4 kHz).
- Background noise from the pump and flow setup gives a low signal-to-noise ratio.

## Future work

- A dedicated acquisition board that can sample at a high rate and broadcast at the same time
- Passive noise isolation of the test pipe
- A machine learning model that estimates the buffer time from the signals, so leaks can be localised without knowing their position in advance

## References

- Choi, J., Shin, J., Song, C., Han, S., & Park, D. I. (2017). Leak detection and location of water pipes using vibration sensors and modified ML prefilter. *Sensors, 17*(9), 2104. https://doi.org/10.3390/s17092104
- Fan, H., Tariq, S., & Zayed, T. (2022). Acoustic leak detection approaches for water pipelines. *Automation in Construction, 138*, 104226. https://doi.org/10.1016/j.autcon.2022.104226
- Lah, A. A. A., Dzyauddin, R. A., Md Yusoff, N., Mohd Ismail, M. I., Ahmad Salleh, N. A., & Ahmad, R. (2019). Leakage localisation using cross correlation for wireless sensor networks.
