# frozen_string_literal: true

require 'test/unit'
require_relative 'machine'

module Z80
    class Z80Test < Test::Unit::TestCase
        def test_execute
            memory = Memory.new
            memory.load([0x0A])
            z80 = Z80.new memory
            reg = Register8.new
            reg.store(0x01)
            z80.execute reg
            puts z80        
        end
    end
end
