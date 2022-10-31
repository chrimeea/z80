# frozen_string_literal: true

class Z80
    def initialize
        @MAX4 = 0x10
        @MAX7 = 0x80
        @MAX8 = 0x100
        @FLAG_C = 0x1
        @FLAG_N = 0x2
        @FLAG_PV = 0x4
        @FLAG_HC = 0x10
        @FLAG_Z = 0x40
        @FLAG_S = 0x80
        @a = @b = @c = @d = @e = @f = @h = @l = 0
        @a’ = @b’ = @c’ = @d’ = @e’ = @f’ = @h’ = @l’ = 0
        @sp = 0
        @pc = 0
        @i = 0
        @x = @y = 0
        @memory = Array.new(49152, 0)
        @state_duration = 1
    end

    def pair h, l
        l = @MAX7 - (l + 1) if l.negative?
        h * @MAX8 + l
    end

    def run
        t = Time.now
        t_states = 4
        op_size = 1
        case @memory[@pc]
        when 0x00 #NOP
        when 0x01 #LD BC,HHLL
            @b, @c = @memory[@pc + 2], @memory[@pc + 1]
            t_states = 10
            op_size = 3
        when 0x02 #LD (BC),A
            @memory[pair @b, @c] = @a
            t_states = 7
        when 0x03 #INC BC
            @c += 1
            if @c == @MAX7
                @b, @c = (@b + 1) & (@MAX7 - 1), -1
            end
            t_states = 6
        when 0x04 #INC B
            @b += 1
            @f ^= @FLAG_N
            if @b == @MAX7
                @b = -1
                @f |= @FLAG_PV | @FLAG_S
            else
                @f ^= @FLAG_PV | @FLAG_S
                if @b == @MAX4
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
        when 0x05 #DEC B
            @b -= 1
            @f |= @FLAG_N
            if @b - 1 == -@MAX7
                @b = 0
                @f |= @FLAG_PV
                @f ^= @FLAG_S
            else
                @f ^= @FLAG_PV
                @f |= @FLAG_S
                if @b - 1 == -@MAX4
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
        when 0x06 #LD B,NN
            @b = @memory[@pc + 1]
            t_states = 7
            op_size = 2
        when 0x07 #RLCA
            @a *= 2
            @f ^= @FLAG_N | @FLAG_HC
            if @a.negative?
                @f |= @FLAG_C
            else
                @f ^= @FLAG_C
            end
        when 0x08 #EX AF,AF’
            @a, @f, @a’, @f’ = @a’, @f’, @a, @f
        else
            fail
        end
        @pc += op_size
        sleep (t + t_states * @state_duration - Time.now) / 1000.0
    end
end

z80 = Z80.new
z80.run
