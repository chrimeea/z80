# frozen_string_literal: true

class Z80
    def initialize
        @MAX5 = 0x10
        @MAX7 = 0x80
        @MAX8 = 0x100
        @FLAG_C = 0x1
        @FLAG_N = 0x2
        @FLAG_PV = 0x4
        @FLAG_HC = 0x10
        @FLAG_Z = 0x40
        @FLAG_S = 0x80
        @a = @b = @c = @d = @e = @f = @h = @l = 0
        @a2 = @b2 = @c2 = @d2 = @e2 = @f2 = @h2 = @l2 = 0
        @sp = 0
        @pc = 0
        @i = 0
        @x = @y = 0
        @memory = Array.new(49152, 0)
        @state_duration = 1
    end

    def pair h, l
        #todo: how to read sign bit from l and / or c ?
        h * @MAX8 + l
    end

    def run
        t = Time.now
        op_size = 1
        case @memory[@pc]
        when 0x00
            t_states = 4
        when 0x01
            @b, @c = @memory[@pc + 1], @memory[@pc]
            t_states = 10
            op_size = 3
        when 0x02
            @memory[pair @b, @c] = @a
            t_states = 7
        when 0x03
            @c += 1
            if @c == @MAX7
                @b, @c = (@b + 1) & (@MAX7 - 1), -1
            end
            t_states = 6
        when 0x04
            @b += 1
            @f ^= @FLAG_N
            if (@b == @MAX7)
                @b = -1
                @f |= @FLAG_PV | @FLAG_S
            else
                @f ^= @FLAG_PV | @FLAG_S
                if @b = @MAX5
                    @f |= @FLAG_HC
                else
                    @f ^= @FLAG_HC
                    if (@b.zero?)
                        @f |= @FLAG_Z
                    else
                        @f ^= @FLAG_Z
                    end
                end
            end
            t_states = 4
        else
            fail
        end
        @pc += op_size
        sleep (t + t_states * @state_duration - Time.now) / 1000.0
    end
end

z80 = Z80.new
z80.run
