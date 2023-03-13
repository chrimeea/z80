# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

# class Debugger
#     def initialize z80
#         @z80 = z80
#         @k1 = 0
#     end

#     def debug
#         @k1 += 1 if @z80.pc.byte_value == 0x0296
#         if @k1 == 17
#             puts 'p'
#             @z80.keyboard.key_press('p', false)
#         elsif @k1 == 25
#             puts '1'
#             @z80.keyboard.key_press('p', true)
#             @z80.keyboard.key_press('1', false)
#         elsif @k1 == 33
#             puts 'ret'
#             @z80.keyboard.key_press('1', true)
#             @z80.keyboard.key_press('Return', false)
#         elsif @k1 == 41
#             puts 'fin'
#             @z80.keyboard.key_press('Return', true)
#         end
#     end
# end

z80 = Z80::Z80.new
# z80.debugger = Debugger.new(z80)
z80.memory.load_rom('../roms/hc90.rom')
Z80::Hardware.new.boot z80

#TODO: border, UART, sound, tape

