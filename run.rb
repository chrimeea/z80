# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

# Z80::Hardware.new.boot
z80 = Z80::Z80.new
z80.state_duration = 0
z80.memory.load_rom('./roms/hc90.rom')
# t = Time.now
32.times do
    z80.execute z80.fetch_opcode
    z80.execute z80.fetch_opcode until z80.pc.byte_value == 0x0E5E
end
z80.execute z80.fetch_opcode until z80.pc.byte_value == 0x0B94
# puts Time.now - t
# z80.pc.store_byte_value(0x38)
100.times do
    puts z80.pc
    reg = z80.fetch_opcode
    puts reg
    z80.execute reg
    puts z80
end

#TODO: keyboard, UART, sound, tape, video attributes
#TODO: 3 5 flags not set for LDDR, ADD HL,DE, SET, RES, BIT

