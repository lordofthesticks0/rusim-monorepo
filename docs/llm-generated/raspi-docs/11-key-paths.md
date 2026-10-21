# 11. Key paths quick reference

```
/home/invitro/water-control-system/          # live project (PM2)
  web_control.py                             # Flask backend
  templates/index.html                       # kiosk UI (CarPlay redesign)
  water_control_arduino/water_control_arduino.ino  # master firmware (water level bypassed)
  logs/2026-10-19.csv                        # latest log (current schema)
/home/invitro/Desktop/water-control-system/  # simpler UI (unused); water level enabled
/home/invitro/%USERPROFILE%\Desktop/...     # third copy (same git status)
/home/invitro/captive_portal.sh
/home/invitro/shutdown_button.py
/home/invitro/test_serial.py
/home/invitro/analog_test/analog_test.ino
/home/invitro/test_relays/test_relays.ino
/home/invitro/rpi-clone/
/home/invitro/Arduino/libraries/            # DallasTemperature, OneWire
/etc/systemd/system/pm2-invitro.service
/etc/lightdm/lightdm.conf
~/.config/openbox/autostart
~/.pm2/dump.pm2
~/.pm2/logs/water-ui-out.log
~/.pm2/logs/water-ui-error.log
/media/usb                                  # USB mount point (empty)
/boot/firmware                              # FAT boot partition
```

## Documentation destination

This documentation is to be placed on the Pi at `~/Documentation` (i.e. `/home/invitro/Documentation`).
