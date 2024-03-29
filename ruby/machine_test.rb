# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

module Z80
    class Register8Test < Test::Unit::TestCase
        def test_increase
            reg = Register8.new
            reg.increase
            assert_equal(1, reg.two_complement)
            reg.store(127)
            reg.increase
            assert_equal(-128, reg.two_complement)
            assert_equal(128, reg.byte_value)
            assert_true(reg.overflow)
            assert_true(reg.negative?)
            assert_true(reg.hc)
            assert_false(reg.carry)
            reg.store(-1)
            reg.increase
            assert_true(reg.hc)
            assert_false(reg.overflow)
            assert_true(reg.carry)
        end

        def test_add
            reg = Register8.new
            reg.store(-128)
            alt_reg = Register8.new
            alt_reg.store(-128)
            reg.add(alt_reg)
            assert_equal(0, reg.two_complement)
            assert_equal(0, reg.byte_value)
            assert_true(reg.overflow)
            assert_false(reg.negative?)
            assert_false(reg.hc)
            assert_true(reg.carry)
        end

        def test_store
            reg = Register8.new
            reg.store(10)
            assert_equal(10, reg.two_complement)
            reg.store(-99)
            assert_equal(-99, reg.two_complement)
            reg.store(129)
            assert_equal(-127, reg.two_complement)
            assert_equal(129, reg.byte_value)
            reg.store(255)
            assert_equal(-1, reg.two_complement)
            assert_equal(255, reg.byte_value)
            reg.store(-128)
            assert_equal(-128, reg.two_complement)
            assert_equal(128, reg.byte_value)
            reg.store(-129)
            assert_equal(127, reg.two_complement)
            assert_equal(127, reg.byte_value)
        end

        def test_exchange
            reg = Register8.new
            reg.store_byte_value(1)
            alt = Register8.new
            alt.store_byte_value(2)
            reg.exchange(alt)
            assert_equal(2, reg.byte_value)
            assert_equal(1, alt.byte_value)
        end

        def test_negate
            reg = Register8.new
            reg.negate
            assert_equal(0xFF, reg.byte_value)
            reg.negate
            assert_equal(0, reg.byte_value)
        end

        def test_set_bit
            reg = Register8.new
            reg.set_bit(4)
            assert_equal(0x10, reg.byte_value)
            assert_true(reg.bit?(4))
            reg.set_bit(4, false)
            assert_equal(0, reg.byte_value)
            assert_false(reg.bit?(4))
        end

        def test_shift_left
            reg = Register8.new
            reg.store_byte_value(0xFE)
            reg.shift_left
            assert_equal(0xFC, reg.byte_value)
        end

        def test_to_4_bit_pair
            reg = Register8.new
            reg.store_byte_value(0x10)
            reg_high, reg_low = reg.to_4_bit_pair
            assert_equal(0x00, reg_low)
            assert_equal(0x01, reg_high)
        end

        def test_store_4_bit_pair
            reg = Register8.new
            reg.store_4_bit_pair(0x01, 0x00)
            assert_equal(0x10, reg.byte_value)
        end
    end

    class Register16Test < Test::Unit::TestCase
        def test_store
            reg = Register16.new
            reg.store(1)
            assert_equal(1, reg.two_complement)
            assert_equal(0, reg.high.two_complement)
            assert_equal(1, reg.low.two_complement)
            reg.store(25638)
            assert_equal(25638, reg.two_complement)
            assert_equal(100, reg.high.two_complement)
            assert_equal(38, reg.low.two_complement)
            reg.store(-1)
            assert_equal(-1, reg.two_complement)
            assert_equal(255, reg.high.byte_value)
            assert_equal(255, reg.low.byte_value)
            reg.store(-19053)
            assert_equal(-19053, reg.two_complement)
            assert_equal(181, reg.high.byte_value)
            assert_equal(147, reg.low.byte_value)
            reg.store(0x0100)
            assert_equal(0x0100, reg.two_complement)
            assert_equal(0x01, reg.high.byte_value)
            assert_equal(0x00, reg.low.byte_value)
            reg.decrease
            assert_equal(0xFF, reg.two_complement)
            assert_equal(0x00, reg.high.byte_value)
            assert_equal(0xFF, reg.low.byte_value)
        end

        def test_add
            reg = Register16.new
            reg.store(0x4000)
            alt = Register16.new
            alt.store(0xFFFF)
            reg.add(alt)
            assert_equal(0x3FFF, reg.byte_value)
            assert_true(reg.carry)
            assert_false(reg.hc)
            reg.store(0x57FF)
            alt.store(0x0701)
            reg.add(alt)
            assert_equal(0x5F00, reg.byte_value)
            assert_false(reg.carry)
            assert_false(reg.hc)
        end

        def test_exchange
            reg = Register16.new
            reg.store_byte_value(0xABCD)
            alt = Register16.new
            alt.store_byte_value(0x1234)
            reg.exchange(alt)
            assert_equal(0x1234, reg.byte_value)
            assert_equal(0xABCD, alt.byte_value)
        end
    end

    class MemoryTest < Test::Unit::TestCase
        def test_read16
            memory = Memory.new(MAX16)
            data = Array.new(MAX16, 0)
            data[0] = 0x01
            data[1] = 0x02
            data[0xFFFE] = 0x03
            data[0xFFFF] = 0x04
            memory.load(data)
            reg = Register16.new
            alt = memory.read16(reg)
            assert_equal(0x01, alt.low.byte_value)
            assert_equal(0x02, alt.high.byte_value)
            reg.store_byte_value(0xFFFE)
            alt = memory.read16(reg)
            assert_equal(0x03, alt.low.byte_value)
            assert_equal(0x04, alt.high.byte_value)
            reg.store_byte_value(0xFFFF)
            alt = memory.read16(reg)
            assert_equal(0x04, alt.low.byte_value)
            assert_equal(0x01, alt.high.byte_value)
        end
    end

    class KeyboardTest < Test::Unit::TestCase
        def test_key_press
            k = Keyboard.new
            reg16 = Register16.new
            reg16.store_byte_value(0xDFFE)
            alt16 = Register16.new
            alt16.store_byte_value(0xFEFE)
            secalt16 = Register16.new
            secalt16.store_byte_value(0xDEFE)
            assert_equal(0x1F, k.read8(reg16).byte_value)
            assert_equal(0x1F, k.read8(alt16).byte_value)
            assert_equal(0x1F, k.read8(secalt16).byte_value)
            k.key_press('p', false)
            assert_equal(0x1E, k.read8(reg16).byte_value)
            assert_equal(0x1F, k.read8(alt16).byte_value)
            assert_equal(0x1E, k.read8(secalt16).byte_value)
            k.key_press('p', true)
            assert_equal(0x1F, k.read8(reg16).byte_value)
            assert_equal(0x1F, k.read8(alt16).byte_value)
            assert_equal(0x1F, k.read8(secalt16).byte_value)
        end
    end

    class Z80Test < Test::Unit::TestCase
        def test_execute_ld_bc_hhll
            z80 = Z80.new
            z80.memory.load([0x01, 0x0A, 0x02, 0x01, 0xFF, 0xFF])
            z80.execute z80.fetch_opcode
            assert_equal(0x020A, z80.bc.byte_value)
            z80.execute z80.fetch_opcode
            assert_equal(0xFFFF, z80.bc.byte_value)
        end

        def test_execute_inc_b
            z80 = Z80.new
            z80.af.store_byte_value(0)
            z80.memory.load([0x04])
            z80.execute z80.fetch_opcode
            assert_equal(0x01, z80.bc.high.byte_value)
            assert_equal(0x00, z80.af.low.byte_value)
        end

        def test_execute_inc_bc
            z80 = Z80.new
            z80.memory.load([0x03])
            z80.execute z80.fetch_opcode
            assert_equal(0x0001, z80.bc.byte_value)
        end

        def test_execute_xor_a_a
            z80 = Z80.new
            z80.memory.load([0xAF])
            z80.af.high.store_byte_value(0xFF)
            z80.execute z80.fetch_opcode
            assert_equal(0x0044, z80.af.byte_value)
        end

        def test_execute_and_a
            z80 = Z80.new
            z80.af.store_byte_value(0)
            z80.memory.load([0xE6, 0x55])
            z80.execute z80.fetch_opcode
            assert_equal(0x0054, z80.af.byte_value)
        end

        def test_cp_a_h
            z80 = Z80.new
            z80.memory.load([0xBC])
            z80.af.high.store_byte_value(0x3F)
            z80.hl.high.store_byte_value(0x3F)
            z80.execute z80.fetch_opcode
            assert_equal(0x6A, z80.af.low.byte_value)
        end

        def test_lddr
            z80 = Z80.new
            z80.memory.load([0xED, 0xB8])
            z80.af.store_byte_value(0x3F00)
            z80.bc.store_byte_value(0x00A8)
            z80.de.store_byte_value(0xFFFF)
            z80.hl.store_byte_value(0x3EAF)
            z80.execute z80.fetch_opcode until z80.pc.byte_value == 0x0002
            puts z80.af.low.to_s(2)
            assert_equal(0x3F00, z80.af.byte_value)
            assert_equal(0x00, z80.bc.byte_value)
            assert_equal(0xFF57, z80.de.byte_value)
            assert_equal(0x3E07, z80.hl.byte_value)
        end
    end
end
