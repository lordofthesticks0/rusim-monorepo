# CALIBRATION NOTES

## 1. Temperature sensor
> no calibrations needed; uses a [library](https://github.com/adafruit/DHT-sensor-library) with consistent outputs

## 2. CO2 sensor
- [ ] tested (1/4)
- [ ] calibrated

There's a noticable lag that needs adressing to based on the comparative reading session with a sndway single gas sensor alongside a limited maximum reading of 4647
current issues: time constraint; monitoring takes 2 hours to do, calibration chamber lacks seal and stability; calibration requires a used wireless earphone box to hold up the sndway device.

## 3. Pressure sensor
- [x] tested
- [x] calibrated

Last test proved sensor to be responsive and accurate but needs further calibration to translate readings into actual kpa values between 918pa and 1836pa (10cm and 20cm stable).
current issues: leakage in manometer makes it difficult for presusre to stay constant in the section where the pressure sensor is installed, tool to create pressure is a comedically useless baloon pump that doesn't hold air nor suck air upon retraction.

### UPDATE 09/09/2026
All sensors checked. Uncalibrated, but verified that the code works.

### UPDATE 10/09/2026
Recalibration requires a recalculation of `CALIBRATION_FACTOR`. For example, if a sensor currently reports 50,000 Pa at a known 981 Pa reference:

```
newFactor = oldFactor × measuredPressure / targetPressure
newFactor = 2280 × 50000 / 981
newFactor ≈ 116208
```

All `CALIBRATION_FACTOR`s are stored in a hard-coded array.

```cpp
const int SENSORID = 3; // Select the sensor calibration: 0, 1, 2, or 3.
const float CALIBRATION_FACTORS[4] = {
  581040.0f, // Sensor 0
  70802.7743f, // Sensor 1
  82656.645503906f, // Sensor 2
  60223.70030581f  // Sensor 3
};
```

Something something frugal science. Did it by blowing the pipe directly. Used a manometer for calibration.

## 4. pH sensor
- [x] tested
- [x] calibrated

Calibration steps: each sensor had the potentiometer turned to a baseline of 2.5v while the probe was submerged in 6.18 pH, then it's cleaned and tested on the acidic and basic water mixtures (cleaned on each test) to note down the voltage on each one for a 3-point reference.
No major calibration issue, only issue being the - noticably difficult to fix - 0.3pH tolerance.

## 5. CH4 sensor
- [x] tested
- [ ] calibrated

All sensors were warmed constantly over 3 days to remove any oxidation over the copper plating
Calibration seems difficult due to the limited time constraint from using a real rumen sample, sensor seems to be reading 7x lower than the benchmark Mestek brand methane/combustible gas detector.

### UPDATE 08/09/2026
The sensor itself is installed in series with a load resistor. For the small modules it's 1KΩ, but we can't seem to figure out for the larger module. We assume it's exactly the same. The sensor's resistance is variable, depending on the current air. In the code it is stated as `rs`. 

To convert `rs` to `ppm` (parts per million), we use the following logic:

```cpp
float rsToPpm(float rs, int sensor_idx) {
  if (rs <= 0) return -1.0;
  float log_rs = log10(rs);
  float log_ppm = (log_rs - c_intercept[sensor_idx]) / m_slope[sensor_idx];
  return pow(10.0, log_ppm);
}
```
