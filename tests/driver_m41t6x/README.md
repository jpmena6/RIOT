# About

This is a manual test application for the m41t62 Real Time Clock chip.

# Usage

The chip is initialized (arbitrarily) to 2016-07-31 12:58:56.00.
The current time will be read out and printed to STDOUT once per second.
An alarm will be set for 2016-07-31 13:02:34.00 (218 seconds after initialization).
When the alarm is triggered, the text " ===== M41T6x Alarm" will be printed.
