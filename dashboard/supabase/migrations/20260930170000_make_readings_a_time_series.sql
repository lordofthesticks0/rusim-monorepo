-- Turns device_data.readings into a time series.
--
-- Two changes:
--
-- 1. experiment_id is dropped from the primary key, leaving
--    (device_id, bottle_id, timestamp). Previously the key included
--    experiment_id, which allowed exactly one row per bottle per experiment and
--    made repeated sampling impossible. A device now records many samples for
--    the same bottle within one experiment, distinguished by timestamp.
--
--    experiment_id itself is unchanged: it remains a NOT NULL column used to
--    group samples into experiments, with 0 reserved for the testing
--    experiment. Only its role in the key is removed.
--
-- 2. The five measurement columns become nullable so that a device can report
--    that a sensor produced no value, rather than being forced to invent one.
--    A row is now present-but-empty rather than absent when a reading fails.

alter table device_data.readings
  alter column ph drop not null,
  alter column pressure drop not null,
  alter column temp drop not null,
  alter column co2 drop not null,
  alter column ch4 drop not null;

alter table device_data.readings
  drop constraint readings_pkey;

alter table device_data.readings
  add constraint readings_pkey
  primary key (device_id, bottle_id, timestamp);

comment on column device_data.readings.ph is
  'Measured pH, or null when the sensor produced no reading.';
comment on column device_data.readings.pressure is
  'Measured pressure in kPa, or null when the sensor produced no reading.';
comment on column device_data.readings.temp is
  'Measured temperature in degrees Celsius, or null when the sensor produced no reading.';
comment on column device_data.readings.co2 is
  'Measured CO2 in ppm, or null when the sensor produced no reading.';
comment on column device_data.readings.ch4 is
  'Measured methane in ppm, or null when the sensor produced no reading.';
comment on column device_data.readings.experiment_id is
  'Experiment grouping. 0 is the testing experiment; 1 and above are regular experiments. Not part of the primary key, so one experiment spans many samples per bottle.';