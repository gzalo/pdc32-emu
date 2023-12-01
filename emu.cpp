#include <iostream>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
using namespace std;

#include "instructions.h"
#include "registers.h"
#include "vga.h"

constexpr uint32_t programLen = 65536; // 64 KiB
constexpr uint32_t cacheLen = 131072; // 128 KiB
constexpr uint32_t dramLen = 33554432; // 32 MiB

// 4Mhz (baseclock) / 4 (clocks per instruction) / 60hz (display fps)
constexpr uint32_t instructions_per_display_update = 16666;

uint32_t dataLiteral = 0; // Data from instructions

uint32_t cacheAddr = 0;
uint32_t cacheData = 0;
uint32_t cache[cacheLen];

uint32_t dramAddr = 0;
uint32_t dramData = 0;
uint32_t dram[dramLen];

uint32_t program[programLen];
uint16_t programCounter = 0;
uint16_t returnAddress = 0;

bool carryIn = false;
uint32_t aluFlags = 0;
uint32_t a = 0;
uint32_t b = 0;

uint8_t uart_data_bits = 8;

#include "comparisons.h"

bus_register busRegister = REG_LITERAL;

uint32_t bus() {
    switch (busRegister) {
        case REG_DRIVE_SERIAL:
        case REG_RTC:
        case REG_UNUSED:
        case REG_KBD:
        case REG_UART:
        case REG_DRAM_DATA:
            return dram[dramAddr % dramLen];
        case REG_DRAM_ADDR:
            return dramAddr;
        case REG_CACHE_DATA:
            return cache[cacheAddr % cacheLen];
        case REG_SHIFT_LEFT_A:
            return a << 1;
        case REG_A_AND_B:
            return a & b;
        case REG_SHIFT_RIGHT_A:
            return a >> 1;
        case REG_A_XOR_B:
            return a ^ b;
        case REG_A_OR_B:
            return a | b;
        case REG_A_PLUS_B:
            return (carryIn ? 1 : 0) + a + b;
        case REG_STATE:
        case REG_LITERAL:
            return dataLiteral;
    }
    cerr << "MISSING IMPLEMENTATION FOR REGISTER: " << busRegister << endl;
    exit(1);
}

void handleInstruction(const uint32_t instruction) {
    const uint8_t type = (instruction & 0xFF0000) >> 16;
    const uint16_t data = instruction & 0xFFFF;
    dataLiteral = (dataLiteral & 0xFFFF0000) | data;

    if(type == A0_SET_DRAM_DATA) {
        dramData = bus();
    } else if(type == A1_SET_CARRY_IN) {
        carryIn = bus() & 8; // Bit 3 is used for carry in
    } else if(type == A2_RETURN) {
        programCounter = returnAddress;
    } else if(type == A3_JLEQ) {
        if(lessOrEqualThan()) programCounter = data;
    } else if(type == A4_JGEQ) {
        if(greaterOrEqualThan()) programCounter = data;
    } else if(type == A5_JMP) {
        programCounter = data;
    } else if(type == A6_INC_DRAM_ADDR) {
        dramAddr++;
    } else if(type == A7_SET_DRAM_ADDR) {
        dramAddr = bus();
    } else if(type == A8_JNE) {
        if(notEqualThan()) programCounter = data;
    } else if(type == A9_SET_A) {
        a = bus();
    } else if(type == A10_SET_HIGH) {
        dataLiteral = (dataLiteral & 0xFFFF) | data << 16;
    } else if(type == A11_WRITE_DRAM) {
        dram[dramAddr % dramLen] = dramData;
    } else if(type == A12_SET_BUS) {
        busRegister = (bus_register)(data & 15);
    } else if(type == A13_CALL) {
        returnAddress = programCounter;
        programCounter = bus();
    } else if(type == A14_SET_B) {
        b = bus();
    } else if(type == A15_SET_ALU) {
        aluFlags = bus();
    } else if(type == B0_TIMER_SPEAKER_OFV) {
        // TODO: implement
    } else if(type == B1_UART_OFV) {
        auto speed = 24000000 / bus();
        cout << "UART OFV: speed=" << speed << " bps (* might need more math)" << endl;
    } else if(type == B2_UART_CONFIG) {
        auto word = bus();
        bool parity_odd = (word >> 16) & 0b1;
        uint8_t databits = 6 + ((word >> 17) & 0b11);
        bool parity_enabled = (word >> 19) & 0b1;
        uint8_t stopbits = 1 + (word >> 20);

        uart_data_bits = databits;

        cout << "UART CONFIG:"
            << " databits=" << unsigned(databits)
            << " parity=" << (parity_enabled ? (parity_odd ? "ODD" : "EVEN") : "OFF")
            << " stopbits=" << unsigned(stopbits)
            << endl;
    } else if(type == B3_UART_TX) {
        uint16_t mask = (1 << (uart_data_bits + 1)) - 1; // lower bits
        uint16_t word = bus() & mask;
        char data[5]; // in case uart_data_bits gets corrupted to something invalid, the bus might be 4 hexa chars + NUL
                      //
        if (uart_data_bits <= 8) {
            data[0] = (char) bus() & 0xFF;
            data[1] = '\0';
        } else {
            snprintf(data, 4, "%x", word);
        }

        cout << "UART TX: (" << unsigned(uart_data_bits) << " bits) " << (char *) data << endl;
    } else if(type == B5_KBD_TX) {
        // TODO: implement
    } else if(type == B6_DATA_ADDR_RTC) {
        // TODO: implement
    } else if(type == B7_OUT_DEBUG_COMMAND_RTC) {
        cout << "OUT PARALLEL " << bus() << endl;
    } else if(type == B8_JL) {
        if(lessThan()) programCounter = data;
    } else if(type == B9_TIMER_SPEAKER_FUNCTION) {
        // TODO: implement
    } else if(type == B10_JG) {
        if(greaterThan()) programCounter = data;
    } else if(type == B11_JE) {
        if(equalThan()) programCounter = data;
    } else if(type == B12_SET_CACHE_DATA) {
        cacheData = bus();
    } else if(type == B13_SET_CACHE_ADDR) {
        cacheAddr = bus();
    } else if(type == B14_ON_OFF_ATX) {
        // TODO: implement
    } else if(type == B15_WRITE_CACHE) {
        cache[cacheAddr % cacheLen] = cacheData;
    } else if(type == C0_TIMER) {
        // TODO: implement
    } else if(type == C1_TIME) {
        // TODO: implement
    } else if(type == C2_DRIVE_SERIAL_DATA) {
        // TODO: implement
    } else if(type == C3_DRIVE_SERIAL_ADDR) {
        // TODO: implement
    } else if(type == C4_DRIVE_SERIAL_FUNCTION) {
        // TODO: implement
    } else if(type == C7_VGA_TEXT_COLOR) {
	uint8_t fg = (bus() >> 16) & 0xff;
	uint8_t bg = (bus() >> 24) & 0xff;
	vga_C7_text_color(fg, bg);
    } else if(type == C8_VGA_WRITE_VRAM) {
        // TODO: implement
    } else if(type == C9_VGA_FUNCTION) {
        // TODO: implement
    } else if(type == C10_VGA_TEXT_BLINK) {
        // TODO: implement
    } else if(type == C11_VGA_PIXEL_COLOR) {
        // TODO: implement
    } else if(type == C12_VGA_TEXT_WRITE) {
	uint8_t c = bus() & 0xff;
	vga_C12_text_write(c);
    } else if(type == C13_VGA_TEXT_CHAR) {
        // TODO: implement
    } else if(type == C14_VGA_PIXEL_POS) {
        // TODO: implement
    } else if(type == C15_VGA_TEXT_POS) {
	uint8_t row = (bus() >> 7) & 0x1f;
	uint8_t col = bus() & 0x7f;
	vga_C15_text_position(row, col);
    } else {
        cerr << "UNKNOWN TYPE " << hex << (int)type << endl;
        exit(1);
    }
}

void loadProgram(const char* filename) {
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL) {
        cerr << "COULD NOT OPEN PROGRAM FILE " << filename << endl;
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    const size_t fileLen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    for (int i = 0; i < fileLen/3; i++) {
        uint8_t bytes[3];
        fread(bytes, 1, 3, fp);

        const uint32_t word = (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
        program[i] = word;
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        bool help = false;

        help |= strcmp(argv[1], "-h") == 0;
        help |= strcmp(argv[1], "-H") == 0;
        help |= strcmp(argv[1], "--help") == 0;
        help |= strcmp(argv[1], "/?") == 0;

        if (help) {
            cout << "usage: " << argv[0] << " [-h] [program.bin]" << endl;
            return 0;
        }
        loadProgram(argv[1]);
    } else {
        loadProgram("program.bin");
    }

    bool quit = false;
    int display_counter = 0;

    display_init();
    while(!quit) {
        const uint32_t instruction = program[programCounter++];
        handleInstruction(instruction);
	if (display_counter++ == instructions_per_display_update) {
	    display_update();
	    display_counter = 0;
	}
    }
    display_teardown();
}
