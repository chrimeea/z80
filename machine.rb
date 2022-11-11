# frozen_string_literal: true

module Z80

    MAX0 = 0x1
    MAX1 = 0x2
    MAX2 = 0x4
    MAX4 = 0x10
    MAX6 = 0x40
    MAX7 = 0x80
    MAX8 = 0x100
    MAX15 = 0x8000

    class Register8
        attr_reader :value, :overflow, :hc, :carry
        attr_writer :value

        def initialize
            @value = 0
            @overflow, @hc, @carry = false
        end

        def as_unsigned
            if @value.negative?
                MAX8 - @value
            else
                @value
            end
        end

        def shift_left
            if @value.negative?
                @carry = true
                v = self.as_unsigned << 1
            else
                @carry = false
                v = @value << 1
            end
            if v >= MAX7
                v = MAX8 - v
            end
            @value = v
        end

        def rotate_left
            self.shift_left
            @value += 1 if @carry
        end

        def rotate_left_trough_carry c
            v, @carry = c
            self.shift_left
            @value += 1 if v
        end

        def shift_right
            @carry = @value.odd?
            if @value.negative?
                v = self.as_unsigned >> 1
            else
                v = @value >> 1
            end
            @value = v
        end

        def rotate_right
            self.shift_right
            @value -= MAX7 if @carry
        end

        def rotate_right_trough_carry c
            v, @carry = c
            self.shift_right
            @value -= MAX7 if v
        end

        def flags f
            f.flag_pv = @overflow
            f.flag_s = @value.negative?
            f.flag_z = @value.zero?
            f.flag_hc = @hc
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
            @hc = (prev_value.abs < MAX4 && @value.abs >= MAX4) || (prev_value.abs > MAX4 && @value.abs <= MAX4)
        end
    end

    class Flag8 < Register8
        attr_accessor :flag_c, :flag_n, :flag_pv, :flag_hc, :flag_z, :flag_s

        def initialize
            @flag_c, @flag_n, @flag_pv, @flag_hc, @flag_z, @flag_s = false
        end

        def value
            v = 0
            v += MAX0 if @flag_c
            v += MAX1 if @flag_n
            v += MAX2 if @flag_pv
            v += MAX4 if @flag_hc
            v += MAX6 if @flag_z
            v -= MAX8 if @flag_s
        end
    end

    class Register16
        attr_reader :high, :low, :overflow, :hc

        def initialize h = Register8.new, l = Register8.new
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

        def flags_math f
            f.flag_hc = @hc
            f.flag_c = @overflow
        end
    end

    class Z80
        def initialize
            @a, @b, @c, @d, @e, @h, @l = [Register8.new] * 8
            @a’, @b’, @c’, @d’, @e’, @h’, @l’ = [Register8.new] * 8
            @f, @f’ = [Flag8.new] * 2
            @bc = Register16.new(@b, @c)
            @de = Register16.new(@d, @e)
            @hl = Register16.new(@h, @l)
            @af = Register16.new(@a, @f)
            @sp = Register16.new
            @pc = 0
            @i = 0
            @x = @y = 0
            @memory = Array.new(49152) { Register8.new }
            @state_duration = 1
        end

        def run
            t = Time.now
            t_states = 4
            op_size = 1
            case @memory[@pc].value
            when 0x00 #NOP
            when 0x01 #LD BC,HHLL
                @bc.store(@memory[@pc + 2].value, @memory[@pc + 1].value)
                t_states = 10
                op_size = 3
            when 0x02 #LD (BC),A
                @memory[@bc.value].value = @a.value
                t_states = 7
            when 0x03 #INC BC
                @bc.store(@bc.value + 1)
                t_states = 6
            when 0x04 #INC B
                @b.store(@b.value + 1)
                @f.flag_n = false
                @b.flags(@f)
            when 0x05 #DEC B
                @b.store(@b.value - 1)
                @f.flag_n = true
                @b.flags(@f)
            when 0x06 #LD B,NN
                @b.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x07 #RLCA
                @a.rotate_left
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = @a.carry
            when 0x08 #EX AF,AF’
                @a.value, @f.value, @a’.value= @a’.value, @f’.value, @a.value
                @f, @f’ = @f’, @f
            when 0x09 #ADD HL,BC
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x0A #LD A,(BC)
                @a.value = @memory[@bc.value].value
                t_states = 7
            when 0x0B #DEC BC
                @bc.store(@bc.value - 1)
                t_states = 6
            when 0x0C #INC C
                @c.store(@c.value + 1)
                @f.flag_n = false
                @c.flags(@f)
            when 0x0D #DEC C
                @c.store(@c.value - 1)
                @f.flag_n = false
                @c.flags(@f)
            when 0x0E #LD C,NN
                @c.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x0F #RRCA
                @a.rotate_right
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = !@a.carry
            when 0x10 #DJNZ NN
                @b.store(@b.value - 1)
                if @b.nonzero?
                    @pc += @memory[@pc + 1].value
                end
                t_states = 13 + 8
                op_size = 2
            when 0x11 #LD DE,HHLL
                @de.store(@memory[@pc + 2].value, @memory[@pc + 1].value)
                t_states = 10
                op_size = 3
            when 0x12 #LD (DE),A
                @memory[@de.value].value = @a.value
                t_states = 7
            when 0x13 #INC DE
                @de.store(@de.value + 1)
                t_states = 6
            when 0x14 #INC D
                @d.store(@d.value + 1)
                @f.flag_n = false
                @d.flags(@f)
            when 0x15 #DEC D
                @d.store(@d.value - 1)
                @f.flag_n = true
                @d.flags(@f)
            when 0x16 #LD D,NN
                @d.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x17 #RLA
                @a.rotate_left_trough_carry (@f.value & @FLAG_C).nonzero?
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = !@a.carry
            when 0x18 #JR NN
                @pc += @memory[@pc + 1].value
                t_states = 12
                op_size = 2
            when 0x19 #ADD HL,DE
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x1A #LD A,(DE)
                @a.value = @memory[@de.value].value
                t_states = 7
            when 0x1B #DEC DE
                @de.store(@de.value - 1)
                t_states = 6
            when 0x1C #INC E
                @e.store(@e.value + 1)
                @f.flag_n = false
                @e.flags(@f)
            when 0x1D #DEC E
                @e.store(@e.value - 1)
                @f.flag_n = true
                @e.flags(@f)
            when 0x1E #LD E,NN
                @e.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x1F #RRA
                @a.rotate_right_trough_carry (@f & @FLAG_C).nonzero?
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = ~@a.carry
            when 0x20 #JR NZ,NN
                @pc += @memory[@pc + 1].value if (@f.value & @FLAG_Z).zero?
                t_states = 12 + 7
                op_size = 2
            when 0x21 #LD HL,HHLL
                @hl.store(@memory[@pc + 2].value, @memory[@pc + 1].value)
                t_states = 10
                op_size = 3
            when 0x22 #LD (HHLL),HL
                v = Register16.new(@memory[@pc + 2], @memory[@pc + 1]).value
                @memory[v + 1].value, @memory[v].value = @h, @l
                t_states = 16
                op_size = 3
            when 0x23 #INC HL
                @hl.store(@hl.value + 1)
                t_states = 6
            when 0x24 #INC H
                @h.store(@h.value + 1)
                @f.flag_n = false
                @h.flags(@f)
            when 0x25 #DEC H
                @h.store(@h.value - 1)
                @f.flag_n = true
                @h.flags(@f)
            when 0x26 #LD H,NN
                @h.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x27 #DAA
                q, r = @a.as_unsigned.divmod MAX4
                c = (@f.value & @FLAG_C).nonzero?
                h = (@f.value & @FLAG_HC).nonzero?
                if c == false && h == false && q == 0x90 && r == 0x09
                    v = 0x00
                    @f.flag_c = false
                elsif c == false && h == false && q ==0x08 && r == 0xAF
                    v = 0x06
                    @f.flag_c = false
                elsif c == false && h == true && q == 0x09 && r == 0x03
                    v = 0x06
                    @f.flag_c = false
                elsif c == false && h == false && q == 0xAF && r == 0x09
                    v = 0x60
                    @f.flag_c = true
                elsif c == false && h == false && q == 0x9F && r == 0xAF
                    v = 0x66
                    @f.flag_c = true
                elsif c == false && h == true && q == 0xAF && r == 0x03
                    v = 0x66
                    @f.flag_c = true
                elsif c == true && h == false && q == 0x02 && r == 0x09
                    v = 0x60
                    @f.flag_c = true
                elsif c == true && h == false && q == 0x02 && r == 0xAF
                    v = 0x66
                    @f.flag_c = true
                elsif c == false && h == true && q == 0x03 && r == 0x03
                    v = 0x66
                    @f.flag_c = true
                elsif c == false && h == false && q == 0x09 && r == 0x09
                    v = 0x00
                    @f.flag_c = false
                elsif c == false && h == true && q == 0x08 && r == 0x6F
                    v = 0xFA
                    @f.flag_c = false
                elsif c == true && h == false && q == 0x7F && r == 0x09
                    v = 0xA0
                    @f.flag_c = true
                elsif c == true && h == true && q == 0x67 && r == 0x6F
                    v = 0x9A
                    @f.flag_c = true
                end
                @f.flag_pv = @a.value.even?
                @f.flag_z = (@a.value == 1)
                @f.flag_s = @a.value.negative?
            when 0x28 #JR Z,NN
                @pc += @memory[@pc + 1].value if (@f.value & @FLAG_Z).nonzero?
                t_states = 12 + 7
                op_size = 2
            when 0x29 #ADD HL,HL
                @hl.store(@hl.value + @hl.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x2A #LD HL,(HHLL)
                v = Register16.new(@memory[@pc + 2], @memory[@pc + 1]).value
                @hl.store(@memory[v + 1].value, @memory[v].value)
                t_states = 16
                op_size = 3
            when 0x2B #DEC HL
                @hl.store(@hl.value - 1)
                t_states = 6
            when 0x2C #INC L
                @l.store(@l.value + 1)
                @f.flag_n = false
                @l.flags(@f)
            when 0x2D #DEC L
                @l.store(@l.value - 1)
                @f.flag_n = true
                @l.flags(@f)
            when 0x2E #LD L,NN
                @l.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x2F #CPL
                @a.store(~@a.as_unsigned)
                @f.flag_n, @f.flag_hc = true
            when 0x30 #JR NC,NN
                @pc += @memory[@pc + 1].value if (@f.value & @FLAG_C).zero?
                t_states = 12 + 7
                op_size = 2
            when 0x31 #LD SP,HHLL
                @sp.store(@memory[@pc + 2].value, @memory[@pc + 1].value)
                t_states = 10
                op_size = 3
            when 0x32 #LD (HHLL),A
                @memory[Register16.new(@memory[@pc + 2], @memory[@pc + 1]).value].value = @a.value
                t_states = 16
                op_size = 3
            when 0x33 #INC SP
                @sp.store(@sp.value + 1)
                t_states = 6
            when 0x34 #INC (HL)
                m = @memory[@hl.value]
                m.store(m.value + 1)
                @f.flag_n = true
                m.flags(@f)
                t_states = 11
            when 0x35 #DEC (HL)
                m = @memory[@hl.value]
                m.store(m.value - 1)
                @f.flag_n = true
                m.flags(@f)
                t_states = 11
            when 0x36 #LD (HL),NN
                @memory[@hl.value].value = @memory[@pc + 1].value
                t_states = 10
                op_size = 2
            when 0x37 #SCF
                @f.flag_c = true
                @f.flag_n, @f.flag_hc = false
            when 0x38 #JR C,NN
                @pc += @memory[@pc + 1].value if (@f.value & @FLAG_C).nonzero?
                t_states = 12 + 7
                op_size = 2
            when 0x39 #ADD HL,SP
                @hl.store(@hl.value + @sp.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x3A #LD A,(HHLL)
                @a.value = @memory[Register16.new(@memory[@pc + 2], @memory[@pc + 1]).value].value
                t_states = 13
                op_size = 3
            when 0x3B #DEC SP
                @sp.store(@sp.value - 1)
                t_states = 6
            when 0x3C #INC A
                @a.store(@a.value + 1)
                @f.flag_n = false
                @a.flags(@f)
            when  0x3D #DEC A
                @a.store(@a.value - 1)
                @f.flag_n = false
                @a.flags(@f)
            when 0x3E #LD A,NN
                @a.value = @memory[@pc + 1].value
                t_states = 7
                op_size = 2
            when 0x3F #CCF
                @f.flag_hc = @f.flag_c
                @f.flag_c = !@f.flag_c
                @f.flag_n = false
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
