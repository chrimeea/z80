# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

module Z80
    class Register8Test < Test::Unit::TestCase
        def test_store
            reg = Register8.new
            reg.store(10)
            assert_equal(10, reg.value)
            reg.store(-99)
            assert_equal(-99, reg.value)
            reg.store(127)
            reg.store(128)
            assert_equal(-128, reg.value)
            assert_equal(128, reg.byte_value)
            assert_true(reg.overflow)
            assert_true(reg.negative?)
            assert_true(reg.hc)
            assert_false(reg.carry)
        end
    end

    class Z80Test < Test::Unit::TestCase
        def test_execute
            z80 = Z80.new
            z80.memory.load([0x0A])
            reg = Register8.new
            reg.store(0x01)
            z80.execute reg
            puts z80        
        end
    end
end
