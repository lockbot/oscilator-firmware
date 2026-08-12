Here is the updated **`0-RasPiSetup.md`** including:

* `bind-interfaces`
* Clear `wpa_supplicant` Wi-Fi configuration guidance

---

# SOFIA RasPi Setup

## 1. Usage (Preconfigured Device)

This Raspberry Pi is configured to:

* Provide **DHCP over Ethernet (eth0)**
* Allow **SSH access via Ethernet**
* Use **Wi-Fi (wlan0)** independently for internet connectivity

---

## Network Configuration

### eth0

* Static IP: **192.168.10.2**
* DHCP range: **192.168.10.50 – 192.168.10.100**
* Used for direct laptop connection

### wlan0

* Standard Wi-Fi client
* Used only for internet access
* Not bridged to Ethernet

---

## Connecting via Ethernet

1. Connect PC directly via Ethernet cable.
2. Ensure PC network adapter is set to **DHCP (automatic)**.
3. SSH into the Pi:

```bash
ssh sofiapi@192.168.10.2
```

---

## Configuring Wi-Fi

You must configure Wi-Fi manually if not preconfigured.

### Option 1 — Using raspi-config

```bash
sudo raspi-config
```

Navigate to:

```
System Options → Wireless LAN
```

Reboot after configuration:

```bash
sudo reboot
```

---

### Option 2 — Manual wpa_supplicant configuration

Edit:

```bash
sudo nano /etc/wpa_supplicant/wpa_supplicant.conf
```

Ensure it contains:

```conf
country=BR
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1

network={
    ssid="YOUR_WIFI_NAME"
    psk="YOUR_WIFI_PASSWORD"
}
```

Apply changes:

```bash
sudo wpa_cli -i wlan0 reconfigure
```

If that fails:

```bash
sudo reboot
```

---

# 2. Clean Installation (Fresh Setup)

## Flash OS

Using Raspberry Pi Imager:

* OS: Raspberry Pi OS Lite (64-bit recommended)
* Enable SSH
* Set username: `sofiapi`
* Set password
* (Optional) Preconfigure Wi-Fi

Flash and boot.

---

## Configure Static Ethernet IP

Edit:

```bash
sudo nano /etc/dhcpcd.conf
```

Add:

```conf
interface eth0
static ip_address=192.168.10.2/24
```

Reboot.

---

## Install and Configure DHCP Server

```bash
sudo apt update
sudo apt install dnsmasq
```

Backup default config:

```bash
sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.orig
```

Create new config:

```bash
sudo nano /etc/dnsmasq.conf
```

Add:

```conf
interface=eth0
bind-interfaces
dhcp-range=192.168.10.50,192.168.10.100,12h
```

Restart service:

```bash
sudo systemctl restart dnsmasq
```

Enable on boot:

```bash
sudo systemctl enable dnsmasq
```

---

## Final Result

* Direct Ethernet connection without router
* Automatic IP assignment (192.168.10.50–100)
* SSH at:

```
192.168.10.2
```

* Wi-Fi operates independently for internet access
