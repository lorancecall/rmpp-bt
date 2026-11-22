# rmpp-bt

<img src="docs/rmpp-bt.gif" alt="rmpp-bt demo" width="405">

*Settings › Bluetooth: turn Bluetooth on or off, pair, connect or forget keyboards.*

Bluetooth settings page and quick-settings toggle for the reMarkable Paper Pro, without xovi.

- **Settings › Bluetooth**: turn Bluetooth on or off, search, pair, connect and forget devices
- **Quick settings**: a Bluetooth toggle
- **Keyboards**: Bluetooth keyboards type in xochitl; per keyboard, "Type Folio mode" uses the Type Folio layout
- Bluetooth stays on across reboots when you leave it on

Tested on OS 3.29 (Qt 6.10).

> Part of the **rmpp add-ons**, built on [rmpp-packages](https://github.com/lorancecall/rmpp-packages): it is
> installed by rmpp setup, loaded into xochitl at every boot by rmpp-preload (no xovi), and kept up to date with
> Vellum. How that differs from xovi: [how it works](https://github.com/lorancecall/rmpp-packages/blob/main/docs/how-it-works.md).

## Install

Install **rmpp setup** (desktop app or a one-line script) from
[rmpp-packages](https://github.com/lorancecall/rmpp-packages#install) and tick **rmpp-bt** (selected by default).
It needs developer mode; the rmpp-packages page explains it.
After a reMarkable OS update, press **Restore** in rmpp setup.

Other ways (Vellum on the command line, building and installing it yourself) and removal:
[docs/install.md](docs/install.md).

## Build

See [docs/install.md](docs/install.md#build-and-install-by-hand).

## License

GPL-3.0-only.
