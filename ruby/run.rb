# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

# class Debugger
#     def initialize z80
#         @z80 = z80
#         @k1, @k2 = 0, 0
#     end

    # def debug
        # @k1 += 1 if @z80.pc.byte_value == 0x0296
        # if @k1 > 16 && @k2 < 24 && @k2 < 100
            # @z80.keyboard.key_press('z', false) if @k2.zero?
            # puts @z80
            # @k2 += 1
        # end
    # end
# end

z80 = Z80::Z80.new
# z80.debugger = Debugger.new(z80)
z80.memory.load_rom('../roms/hc90.rom')
Z80::Hardware.new.boot z80

#TODO: border, UART, sound, tape
#TODO: 3 5 flags not set for LDDR, ADD HL,DE, SET, RES, BIT

