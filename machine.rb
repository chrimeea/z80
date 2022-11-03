# frozen_string_literal: true

module Z80

    MAX4 = 0x10
    MAX7 = 0x80
    MAX8 = 0x100
    MAX15 = 0x8000

    class Register8
        attr_reader :value, :overflow, :hc
        attr_writer :value

        def initialize
            @value = 0
            @overflow, @hc = false
        end

        def flags(f)
            if @overflow
                f |= @FLAG_PV
            else
                f ^= @FLAG_PV
            end
            if (@value.negative?)
                f |= @FLAG_S
            else
                f ^= @FLAG_S
            end
            if (@value.zero?)
                f |= @FLAG_Z
            else
                f ^= @FLAG_Z
            end
            if @hc
                f |= @FLAG_HC
            else
                f ^= @FLAG_HC
            end
        end

        def store(num)
            prev_value = @value
            if num >= MAX7
                @value = MAX7 - num
                @overflow = true
            elsif num < -MAX7
                @value = -MAX7 - num
                @overflow = true
            else
                @value = num
                @overflow = false
            end
            if (prev_value.abs < MAX4 && @value.abs >= MAX4) || (prev_value.abs > MAX4 && @value.abs <= MAX4)
                @hc = true
            else
                @hc = false
            end
        end
    end

    class Register16
        attr_reader :high, :low, :overflow, :hc

        def initialize h, l
            @high, @low = h, l
            @overflow, @hc = false
        end

        def value
            @high.value * MAX8 + @low.value
        end

        def store(h, l)
            @high.value, @low.value = h, l
        end

        def store(num)
            prev_high = @high
            if num >= MAX15
                num = MAX15 - num
                @overflow = true
            elsif num < -MAX15
                num = -MAX15 - num
                @overflow = true
            else
                @overflow = false
            end
            q, r = num.divmod MAX8
            @high.value, @low.value = q, r
            if (prev_high.abs < MAX4 && @high.abs >= MAX4) || (prev_high.abs > MAX4 && @high.abs <= MAX4)
                @hc = true
            else
                @hc = false
            end
        end
    end

    class Z80
        def initialize
            @FLAG_C = 0x1
            @FLAG_N = 0x2
            @FLAG_PV = 0x4
            @FLAG_HC = 0x10
            @FLAG_Z = 0x40
            @FLAG_S = 0x80
            @a, @b, @c, @d, @e, @f, @h, @l = [Register8.new] * 8
            @a’, @b’, @c’, @d’, @e’, @f’, @h’, @l’ = [Register8.new] * 8
            @bc, @de, @hl = Register16.new(@b, @c), Register16.new(@d, @e), Register16.new(@h, @l)
            @sp = 0
            @pc = 0
            @i = 0
            @x = @y = 0
            @memory = Array.new(49152, 0)
            @state_duration = 1
        end

        def run
            t = Time.now
            t_states = 4
            op_size = 1
            case @memory[@pc]
            when 0x00 #NOP
            when 0x01 #LD BC,HHLL
                @bc.store(@memory[@pc + 2], @memory[@pc + 1])
                t_states = 10
                op_size = 3
            when 0x02 #LD (BC),A
                @memory[@bc.value] = @a.value
                t_states = 7
            when 0x03 #INC BC
                @bc.store(@bc.value + 1)
                t_states = 6
            when 0x04 #INC B
                @b.store(@b.value + 1)
                @f ^= @FLAG_N
                @b.flags(@f)
            when 0x05 #DEC B
                @b.store(@b.value - 1)
                @f |= @FLAG_N
                @b.flags(@f)
            when 0x06 #LD B,NN
                @b.value = @memory[@pc + 1]
                t_states = 7
                op_size = 2
            when 0x07 #RLCA
                #TODO: fix
                @a *= 2
                @f ^= @FLAG_N | @FLAG_HC
                if @a.negative?
                    @f |= @FLAG_C
                else
                    @f ^= @FLAG_C
                end
            when 0x08 #EX AF,AF’
                @a, @f, @a’, @f’ = @a’, @f’, @a, @f
            when 0x09 #ADD HL,BC
                @hl.store(@hl.value + @bc.value)
                @f ^= @FLAG_N
                if @hl.hc
                    @f |= @FLAG_HC
                else
                    @f ^= @FLAG_HC
                end
                if @hl.overflow
                    @f |= @FLAG_C
                else
                    @f ^= @FLAG_C
                end
                t_states = 11
            when 0x0A #LD A,(BC)
                @a.value = @memory[@bc.value]
                t_states = 7
            when 0x0B #DEC BC
                @bc.store(@bc.value - 1)
                t_states = 6
            else
                fail
            end
            @pc += op_size
            sleep (t + t_states * @state_duration - Time.now) / 1000.0
        end
    end

end

z80 = Z80::Z80.new
z80.run
