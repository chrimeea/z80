# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

class Debugger
    def initialize
        @z80 = Z80::Z80.new
        @z80.memory.load_rom('../roms/hc90.rom')
        @ula = Z80::ULA.new
        @k1, @k2, @k3 = -8, 0, 0
        @reg1 = Z80::Register16.new
        @reg1.store_byte_value(0x5C00)
        @reg2 = Z80::Register16.new
        @reg2.store_byte_value(0x5C01)
        @reg3 = Z80::Register16.new
        @reg3.store_byte_value(0x5C02)
        @reg4 = Z80::Register16.new
        @reg4.store_byte_value(0x5C03)
        @reg5 = Z80::Register16.new
        @reg5.store_byte_value(0x5C04)
    end

    def main
        loop do
            @z80.run_one
            @ula.draw_screen_once
            self.debug
        end
    end

    def debug
        # if @z80.pc.byte_value == 0x1234 && @k1 == 0
        #     puts '1234'
        #     @k1 = 1
        # elsif @z80.pc.byte_value == 0x0E5C && @k2 == 0
        #     puts '0E5C'
        #     @k2 = 1
        # elsif @z80.pc.byte_value == 0x0039
        #     puts '0038'
        # end
        # @z80.iff1 = false
        # reg16 = Z80::Register16.new
        # reg16.store_byte_value(0x5C01)
        # reg8 = @z80.memory.read8(reg16)
        # if reg8.byte_value != @v
        #     @v = reg8.byte_value
        #     puts @z80
        #     reg = Z80::Register16.new
        #     p 16.times.map { |v|
        #         reg.store_byte_value(0x5C00 + v)
        #         @z80.memory.read8(reg).to_s
        #     }
        # end
        @k1 += 1 if @z80.pc.byte_value == 0x0296
        if @k1 >= 8 && @k2 < 11 &&
            @z80.pc.byte_value == 0x0296 &&
            (@k2 == 2 || @k2 == 6 || @k2 == 10 ||
            (@z80.memory.read8(@reg1).byte_value == 0xFF &&
            @z80.memory.read8(@reg2).byte_value == 0x00 &&
            @z80.memory.read8(@reg3).byte_value == 0x00 &&
            @z80.memory.read8(@reg4).byte_value == 0x00 &&
            @z80.memory.read8(@reg5).byte_value == 0xFF))
            @k2 += 1
            puts @k1
            @k1 = 0
        end
        # if @k2 >= 9 && @k3 == 0 && @z80.pc.byte_value == 0x0296
        #     @k3 = 1
        # end
        # if @k3 > 0 && @k3 < 200
        #     puts @z80
        #     reg = Z80::Register16.new
        #     p 16.times.map { |v|
        #         reg.store_byte_value(0x5C00 + v)
        #         @z80.memory.read8(reg).to_s
        #     }
        #     @k3 += 1
        # end
        if @k2 == 1
            puts 'p'
            @z80.keyboard.key_press('p', false)
            @k2 += 1
        elsif @k2 == 3
            puts 'out'
            @z80.keyboard.key_press('p', true)
            @k2 += 1
        elsif @k2 == 5
            puts '1'
            @z80.keyboard.key_press('1', false)
            @k2 += 1
        elsif @k2 == 7
            puts 'out'
            @z80.keyboard.key_press('1', true)
            @k2 += 1
        elsif @k2 == 9
            puts 'ret'
            @z80.keyboard.key_press('Return', false)
            @k2 += 1
        elsif @k2 == 11
            puts 'out'
            @z80.keyboard.key_press('Return', true)
            @k2 += 1
        end
    end
end

# z80.debugger = Debugger.new
Z80::Hardware.new.boot('../roms/hc90.rom')

#TODO: border, UART, sound, tape
#TODO: let the debugger run first z80 then draw_screen and synchronize them
