/*
 * Copyright (c) 2017-2018 Group of Computer Architecture, University of Bremen <riscv@systemc-verification.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * gpio.cpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#include "gpiocommon.hpp"

#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

using namespace std;

void hexPrint(unsigned char* buf, size_t size) {
	for (uint16_t i = 0; i < size; i++) {
		printf("%2X ", buf[i]);
	}
	cout << endl;
}

void bitPrint(unsigned char* buf, size_t size) {
	for (uint16_t byte = 0; byte < size; byte++) {
		for (int8_t bit = 7; bit >= 0; bit--) {
			printf("%c", buf[byte] & (1 << bit) ? '1' : '0');
		}
		printf(" ");
	}
	printf("\n");
}

void GpioCommon::printRequest(Request* req) {
	switch (req->op) {
		case GET_BANK:
			cout << "GET BANK";
			break;
		case SET_BIT:
			cout << "SET BIT ";
			cout << to_string(req->setBit.pos) << " to ";
			switch (req->setBit.val) {
				case 0:
					cout << "LOW";
					break;
				case 1:
					cout << "HIGH";
					break;
				case 2:
					cout << "UNSET";
					break;
				default:
					cout << "INVALID";
			}
			break;
		default:
			cout << "INVALID";
	}
	cout << endl;
};

GpioCommon::GpioCommon() {
	state = 0;
}
