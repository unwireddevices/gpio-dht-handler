# gpio-dht-handler
DHT11/DHT22 temperature and humidity sensor protocol handler for AR9331

DHT series sensor use a single wire communication protocol similar to well-knows 1-Wire protocol, but without synchronization pulses from host. After intitial start sequence initiated by host, sensor automatically sends 5 bytes of data, where "0" is coded with 26-28us pulse and "1" is coded with 70us pulse. With such timings, protocol can not be handled in userspace or without GPIO IRQ support and precise timers.

*gpio-dht-handler* uses raising edge GPIO interrupts and udelay to distinguish between long and short pulses.

*gpio-dht-handler* kernel module provides support for DHT11/DHT22 protocol for OpenWRT 14.07 "Barrier Breaker" with 3.10 Linux kernel, running on AR9331 SoC. Module is not portable as it uses AR9331's IRQ abilities. Module requires <a href="https://github.com/GBert/openwrt-misc/tree/master/gpio-test/src/patches-3.14">728-MIPS-ath79-add-gpio-irq.patch</a> kernel patch to be applied.

# Usage

*echo "&lt;gpio&gt; [PID]" &gt; /sys/kernel/debug/irq-dht*

*gpio* — GPIO number where DHT sensor is connected.

*PID* — process ID to send the signal containing DHT data to. If no PID is given, data will be sent to kernel log in readable form. Signal sends data as a single 32-bit integer, high word (16 bits) is humdity, low word is temperature in Celsius, multiplied by 10. "0" means error.
