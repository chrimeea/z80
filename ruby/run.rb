# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

# class Debugger
#     def initialize z80
#         @z80 = z80
#         @k1, @k2 = 0, 0
#     end

#     def debug
#         @k1 += 1 if @z80.pc.byte_value == 0x0296
#         # if @k2 > 0 && @k2 < 1000
#         #     puts @z80
#         #     reg = Z80::Register16.new
#         #     p 16.times.map { |v|
#         #         reg.store_byte_value(0x5C30 + v)
#         #         @z80.memory.read8(reg).to_s
#         #     }
#         #     @k2 += 1
#         # end
#         if @k1 == 17
#             puts 'p'
#             @z80.keyboard.key_press('p', false)
#             # @k2 = 1
#         elsif @k1 == 25
#             puts 'out'
#             @z80.keyboard.key_press('p', true)
#         elsif @k1 == 41
#             puts '1'
#             @z80.keyboard.key_press('1', false)
#         elsif @k1 == 49
#             puts 'out'
#             @z80.keyboard.key_press('1', true)
#         elsif @k1 == 57
#             puts 'ret'
#             @z80.keyboard.key_press('Return', false)
#         elsif @k1 == 65
#             puts 'out'
#             @z80.keyboard.key_press('Return', true)
#         end
#     end
# end

z80 = Z80::Z80.new
# z80.debugger = Debugger.new(z80)
z80.memory.load_rom('../roms/hc90.rom')
Z80::Hardware.new.boot z80

#TODO: border, UART, sound, tape

