// bu0836 device manager
//
// Copyright (C) 2010  Melchior FRANZ  <melchior.franz@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
#ifndef _BU0836A_HXX_
#define _BU0836A_HXX_

#include <cstdlib>
#include <iostream>
#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <vector>

#include "hid.hxx"



namespace bu0836 {

enum capabilities {
	INVERT = 0x1,
	ZOOM = 0x2,
	ENCODER1 = 0x4, // 1:1 encoder
	ENCODER2 = 0x8, // 1:2 and 1:4 encoder
};



struct usb_hid_descriptor {
	uint8_t  bLength;		// 9
	uint8_t  bDescriptorType;	// 33 -> LIBUSB_DT_HID
	uint16_t bcdHID;		// 1.10
	uint8_t  bCountryCode;		// 0 (unsupported)
	uint8_t  bNumDescriptors;	// 1
	struct {
		uint8_t bDescriptorType;	// 34 -> LIBUSB_DT_REPORT
		uint8_t wDescriptorLength1;	// 103
		uint8_t wDescriptorLength2;	// 103
	} descriptors
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
	[]
#else
	[0]
#endif
	;

	inline int wDescriptorLength(int n) const {
		return libusb_le16_to_cpu(descriptors[n].wDescriptorLength2 << 8
				| descriptors[n].wDescriptorLength1);
	}
    char *usb_strerror(int error_code);

    int LIBUSB_CALL libusb_init(libusb_context **ctx);
    void LIBUSB_CALL libusb_set_debug(libusb_context *ctx, int level);
    ssize_t LIBUSB_CALL libusb_get_device_list(libusb_context *ctx,
                                               libusb_device ***list);
    void LIBUSB_CALL libusb_exit(libusb_context *ctx);
};



class controller {
public:
	controller(libusb_device_handle *handle, libusb_device *device, libusb_device_descriptor desc,
			int capabilities);
	~controller();
	int claim();
	int get_eeprom();
	int set_eeprom(unsigned int from, unsigned int to);
	int save_image_file(const char *);
	int load_image_file(const char *);
	int show_input_reports();
	int capabilities() const { return _capabilities; }
	int active_axes() const { return _active_axes; }
	bool is_dirty() const { return _dirty; }

	const std::string &bus_address() const { return _bus_address; }
	const std::string &id() const { return _id; }
	const std::string &manufacturer() const { return _manufacturer; }
	const std::string &product() const { return _product; }
	const std::string &serial() const { return _serial; }
	const std::string &release() const { return _release; }
	const std::string &jsid() const { return _jsid; }
	const unsigned char *eeprom() const { return reinterpret_cast<const unsigned char *>(&_eeprom); }

	void set_autodiscovery(bool b) { _eeprom.autodiscovery = b ? 1 : 0, _dirty = true; }
	bool get_autodiscovery() const { return _eeprom.autodiscovery != 0; }

	void set_shutoff(int axis, bool value) {
		uint8_t mask = 1 << axis;
		_eeprom.shutoff = value ? _eeprom.shutoff | mask : _eeprom.shutoff & ~mask;
		_dirty = true;
	}
	bool get_shutoff(int axis) const { return (_eeprom.shutoff & (1 << axis)) != 0; }

	void set_invert(int axis, bool value) {
		uint8_t mask = 1 << axis;
		_eeprom.invert = value ? _eeprom.invert | mask : _eeprom.invert & ~mask;
		_dirty = true;
	}
	bool get_invert(int axis) const { return (_eeprom.invert & (1 << axis)) != 0; }

	void set_zoom(int axis, unsigned char value) { _eeprom.zoom[axis & 7] = value, _dirty = true; }
	int get_zoom(int axis) const { return _eeprom.zoom[axis & 7]; }

	void set_pulse_width(int n) { _eeprom.pulse = n < 1 ? 1 : n > 11 ? 11 : n, _dirty = true; }
	int get_pulse_width() const { return _eeprom.pulse; }

	void set_encoder_mode(int b, int mode);
	int get_encoder_mode(int b) const;

	int sync() {
		if (!_dirty)
			return 0;
		int ret = set_eeprom(0x0b, 0x1a);
		if (!ret)
			_dirty = false;
		return ret;
	}

private:
	int parse_hid(void);
	void print_input(hid::hid_main_item *, const unsigned char *data);
	int get_active_axes(hid::hid_main_item *);

	hid::hid _hid;

	std::string _bus_address;
	std::string _id;
	std::string _manufacturer;
	std::string _product;
	std::string _serial;
	std::string _release;
	std::string _jsid;

	libusb_device_handle *_handle;
	libusb_device *_device;
	libusb_device_descriptor _desc;
	int _active_axes;
	int _capabilities;
	usb_hid_descriptor *_hid_descriptor;
	bool _claimed;
	bool _kernel_detached;
	bool _dirty;

	struct {
		uint8_t ___a[11];      // 0x00
		uint8_t invert;        // 0x0b
		uint8_t autodiscovery; // 0x0c
		uint8_t shutoff;       // 0x0d
		uint8_t zoom[8];       // 0x0e-0x15
		uint8_t rotenc0[2];    // 0x16
		uint8_t rotenc1[2];    // 0x18
		uint8_t pulse;         // 0x1a
		uint8_t ___b[229];     // 0x1b
	} _eeprom;

	static const int _INTERFACE = 0;
};



class manager {
public:
	manager(int debug_level = 3);
	~manager();
	int select(const std::string &which);
	controller *selected() const { return _selected; }
	size_t size() const { return _devices.size(); }
	bool empty() const { return _devices.empty(); }
	controller &operator[](unsigned int index) { return *_devices[index]; }


private:
	std::vector<controller *> _devices;
	controller *_selected;

	libusb_context *_CONTEXT;
};

} // namespace bu0836

#endif
