# 2. Boot & kiosk session model

The Pi boots straight into a graphical kiosk. There is no login screen.

## Boot sequence

1. **systemd** brings up the base system, then enabled services including `lightdm.service` and `pm2-invitro.service`.
2. **LightDM** (`/etc/lightdm/lightdm.conf`) is configured with:
   - `autologin-user=invitro`
   - `autologin-session=openbox`
   - `xserver-command=X -s 0 -dpms -nocursor` (no screensaver, no DPMS, no cursor)
3. **openbox** session starts as `invitro`; `~/.config/openbox/autostart` waits 10 s then launches Chromium in fullscreen kiosk mode pointed at `http://127.0.0.1:5000` (incognito).
4. **PM2** is restored by `pm2-invitro.service` (`ExecStart=pm2 resurrect`), which relaunches the saved `water-ui` process.

## openbox autostart (`~/.config/openbox/autostart`)

```sh
sleep 10

# Kiosk browser. Single logical line on purpose: a stray space after a "\"
# line continuation silently truncates the command and drops the URL.
/usr/bin/chromium --kiosk --disable-infobars --noerrdialogs --incognito --disable-pinch --force-device-scale-factor=1.0 --disable-features=AsyncDns http://127.0.0.1:5000 &
```

The command is intentionally kept on one logical line: a trailing space after a
`\` line continuation escapes the space instead of the newline, so Chromium
starts with no URL and shows only an incognito new tab.

Also started in the session: `unclutter -idle 1 -root` (hides cursor after 1 s idle) and `autotouch` (touchscreen calibration).

## PM2 service (`/etc/systemd/system/pm2-invitro.service`)

```ini
[Unit]
Description=PM2 process manager
After=network.target

[Service]
Type=forking
User=invitro
Environment=PM2_HOME=/home/invitro/.pm2
ExecStart=/usr/local/lib/node_modules/pm2/bin/pm2 resurrect
ExecReload=/usr/local/lib/node_modules/pm2/bin/pm2 reload all
ExecStop=/usr/local/lib/node_modules/pm2/bin/pm2 kill
Restart=on-failure
```

The saved process list (`~/.pm2/dump.pm2`) defines one process:

| Field | Value |
|---|---|
| name | `water-ui` |
| script | `/home/invitro/water-control-system/web_control.py` |
| cwd | `/home/invitro/water-control-system` |
| interpreter | `python3` |
| mode | fork |
| autorestart | true |
| env | `PYTHONUNBUFFERED=1` |

Logs: `~/.pm2/logs/water-ui-out.log`, `water-ui-error.log`.

## Chromium kiosk

Chromium (v155) runs as the `invitro` user with GPU rasterization on the Pi's VideoCore, loads the local Flask app, and is the only visible UI. If the PM2 process dies, the kiosk shows an error page until PM2 resurrects it. The app page title is `System Monitor`.

## Notes / caveats

- Autologin means anyone with physical access gets a desktop session.
- `~/.bashrc` ends by invoking `~/captive_portal.sh` on every interactive login shell (see §9). This runs the captive-portal login flow on each shell and can block/prompt.
- `shutdown_button.py` (GPIO 17, 2 s hold) is **not** started by the documented autostart; it must be launched manually or via another mechanism.
- `unclutter` is started with `-idle 1` (not 0.1 as in older docs).
