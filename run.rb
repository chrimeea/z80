# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

# Z80::Hardware.new.boot
z80 = Z80::Z80.new
z80.state_duration = 0
z80.memory.load_rom('./roms/hc90.rom')
196624.times do
    z80.execute z80.fetch_opcode
end
20.times do
    puts z80.pc
    reg = z80.fetch_opcode
    puts reg
    z80.execute reg
    puts z80
end

#TODO: UART, sound, tape, video attributes

