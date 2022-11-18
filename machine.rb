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

        def flags_s_z f
            f.flag_s = @value.negative?
            f.flag_z = @value.zero?
        end

        def flags f
            f.flag_pv = @overflow
            f.flag_hc = @hc
            flags_s_z(f)
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
            @hc = ((prev_value.abs < MAX4 && @value.abs >= MAX4) || (prev_value.abs > MAX4 && @value.abs <= MAX4))
        end

        def add(num, f)
            self.store(@value + num)
            f.flag_n = false
            f.flag_c = @carry
            self.flags(f)
        end

        def sub(num, f)
            self.store(@value + num)
            f.flag_n = true
            f.flag_c = @carry
            self.flags(f)
        end

        def and(num, f)
            self.store(@value & num)
            self.flags_s_z(f)
            f.flag_hc = true
            f.flag_pv, f.flag_n, f.flag_c = false
        end

        def xor(num, f)
            self.store(@value ^ num)
            self.flags_s_z(f)
            f.flag_pv = @value.even?
            f.flag_hc, f.flag_n, f.flag_c = false
        end

        def or(num, f)
            self.store(@value | num)
            self.flags_s_z(f)
            f.flag_pv, f.flag_hc, f.flag_n, f.flag_c = false
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

        def store(num)
            v = num.to_s(2)
            @flag_c = (v[0] == '1')
            @flag_n = (v[1] == '1')
            @flag_pc = (v[2] == '1')
            @flag_hc = (v[4] == '1')
            @flag_z = (v[6] == '1')
            @flag_s = (v[8] == '1')
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
            @hc = ((prev_high.abs < MAX4 && @high.abs >= MAX4) || (prev_high.abs > MAX4 && @high.abs <= MAX4))
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
            loop do
                interrupt while execute(@memory[@pc].value)
                execute(0x00) while !interrupt
                execute(@memory[@pc].value)
            end
        end

        def interrupt
            return false
        end

        def execute opcode
            t = Time.now
            t_states = 4
            op_size = 1
            case opcode
            when 0x00 #NOP
            when 0x01 #LD BC,HHLL
                @bc.store(@memory[@pc + 2].value, @memory[@pc + 1].value)
                t_states = 10
                op_size = 3
            when 0x02 #LD (BC),A
                @memory[@bc.value].store(@a.value)
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
                @b.store(@memory[@pc + 1].value)
                t_states = 7
                op_size = 2
            when 0x07 #RLCA
                @a.rotate_left
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = @a.carry
            when 0x08 #EX AF,AF’
                v = @a.value
                @a.store(@a’.value)
                @a’.store(v)
                v = @f.value
                @f.store(@f’.value)
                @f’.store(v)
            when 0x09 #ADD HL,BC
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x0A #LD A,(BC)
                @a.store(@memory[@bc.value].value)
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
                @c.store(@memory[@pc + 1].value)
                t_states = 7
                op_size = 2
            when 0x0F #RRCA
                @a.rotate_right
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = !@a.carry
            when 0x10 #DJNZ NN
                @b.store(@b.value - 1)
                @pc += @memory[@pc + 1].value if @b.nonzero?
                t_states = 13 + 8
                op_size = 2
            when 0x11 #LD DE,HHLL
                @de.store(@memory[@pc + 2].value, @memory[@pc + 1].value)
                t_states = 10
                op_size = 3
            when 0x12 #LD (DE),A
                @memory[@de.value].store(@a.value)
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
                @d.store(@memory[@pc + 1].value)
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
                @e.store(@memory[@pc + 1].value)
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
                @memory[v + 1].store(@h.value)
                @memory[v].store(@l.value)
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
                @h.store(@memory[@pc + 1].value)
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
                @l.store(@memory[@pc + 1].value)
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
                @memory[Register16.new(@memory[@pc + 2], @memory[@pc + 1]).value].store(@a.value)
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
                @memory[@hl.value].store(@memory[@pc + 1].value)
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
                @a.store(@memory[Register16.new(@memory[@pc + 2], @memory[@pc + 1]).value].value)
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
                @a.store(@memory[@pc + 1].value)
                t_states = 7
                op_size = 2
            when 0x3F #CCF
                @f.flag_hc = @f.flag_c
                @f.flag_c = !@f.flag_c
                @f.flag_n = false
            when 0x40 #LD B,B
            when 0x41 #LD B,C
                @b.store(@c.value)
            when 0x42 #LD B,D
                @b.store(@d.value)
            when 0x43 #LD B,E
                @b.store(@e.value)
            when 0x44 #LD B,H
                @b.store(@h.value)
            when 0x45 #LD B,L
                @b.store(@l.value)
            when 0x46 #LD B,(HL)
                @b.store(@memory[@hl.value].value)
                t_states = 7
            when 0x47 #LD B,A
                @b.store(@a.value)
            when 0x48 #LD C,B
                @c.store(@b.value)
            when 0x49 #LD C,C
            when 0x4A #LD C,D
                @c.store(@d.value)
            when 0x4B #LD C,E
                @c.store(@e.value)
            when 0x4C #LD C,H
                @c.store(@h.value)
            when 0x4D #LD C,L
                @c.store(@l.value)
            when 0x4E #LD C,(HL)
                @c.store(@memory[@hl.value].value)
                t_states = 7
            when 0x4F #LD C,A
                @c.store(@a.value)
            when 0x50 #LD D,B
                @d.store(@b.value)
            when 0x51 #LD D,C
                @d.store(@c.value)
            when 0x52 #LD D,D
            when 0x53 #LD D,E
                @d.store(@e.value)
            when 0x54 #LD D,H
                @d.store(@h.value)
            when 0x55 #LD D,L
                @d.store(@l.value)
            when 0x56 #LD D,(HL)
                @d.store(@memory[@hl.value].value)
                t_states = 7
            when 0x57 #LD D,A
                @d.store(@a.value)
            when 0x58 #LD E,B
                @e.store(@b.value)
            when 0x59 #LD E,C
                @e.store(@c.value)
            when 0x5A #LD E,D
                @e.store(@d.value)
            when 0x5B #LD E,E
            when 0x5C #LD E,H
                @e.store(@h.value)
            when 0x5D #LD E,L
                @e.store(@l.value)
            when 0x5E #LD E,(HL)
                @e.store(@memory[@hl.value].value)
                t_states = 7
            when 0x5F #LD E,A
                @e.store(@a.value)
            when 0x60 #LD H,B
                @h.store(@b.value)
            when 0x61 #LD H,C
                @h.store(@c.value)
            when 0x62 #LD H,D
                @h.store(@d.value)
            when 0x63 #LD H,E
                @h.store(@e.value)
            when 0x64 #LD H,H
            when 0x65 #LD H,L
                @h.store(@l.value)
            when 0x66 #LD H,(HL)
                @h.store(@memory[@hl.value].value)
                t_states = 7
            when 0x67 #LD H,A
                @h.store(@a.value)
            when 0x68 #LD L,B
                @l.store(@b.value)
            when 0x69 #LD L,C
                @l.store(@c.value)
            when 0x6A #LD L,D
                @l.store(@d.value)
            when 0x6B #LD L,E
                @l.store(@e.value)
            when 0x6C #LD L,H
                @l.store(@h.value)
            when 0x6D #LD L,L
            when 0x6E #LD L,(HL)
                @l.store(@memory[@hl.value].value)
                t_states = 7
            when 0x6F #LD L,A
                @l.store(@a.value)
            when 0x70 #LD (HL),B
                @memory[@hl.value].store(@b.value)
                t_states = 7
            when 0x71 #LD (HL),C
                @memory[@hl.value].store(@c.value)
                t_states = 7
            when 0x72 #LD (HL),D
                @memory[@hl.value].store(@d.value)
                t_states = 7
            when 0x73 #LD (HL),E
                @memory[@hl.value].store(@e.value)
                t_states = 7
            when 0x74 #LD (HL),H
                @memory[@hl.value].store(@h.value)
                t_states = 7
            when 0x75 #LD (HL),L
                @memory[@hl.value].store(@l.value)
                t_states = 7
            when 0x76 #HALT
                return false
            when 0x77 #LD (HL),A
                @memory[@hl.value].value = @a.value
                t_states = 7
            when 0x78 #LD A,B
                @a.store(@b.value)
            when 0x79 #LD A,C
                @a.store(@c.value)
            when 0x7A #LD A,D
                @a.store(@d.value)
            when 0x7B #LD A,E
                @a.store(@e.value)
            when 0x7C #LD A,H
                @a.store(@h.value)
            when 0x7D #LD A,L
                @a.store(@l.value)
            when 0x7E #LD A,(HL)
                @a.value = @memory[@hl.value].value
                t_states = 7
            when 0x7F #LD A,A
            when 0x80 #ADD A,B
                @a.add(@b.value, @f)
            when 0x81 #ADD A,C
                @a.add(@c.value, @f)
            when 0x82 #ADD A,D
                @a.add(@d.value, @f)
            when 0x83 #ADD A,E
                @a.add(@e.value, @f)
            when 0x84 #ADD A,H
                @a.add(@h.value, @f)
            when 0x85 #ADD A,L
                @a.add(@l.value, @f)
            when 0x86 #ADD A,(HL)
                @a.add(@memory[@hl.value].value, @f)
                t_states = 7
            when 0x87 #ADD A,A
                @a.add(@a.value, @f)
            when 0x88 #ADC A,B
                @a.add(@b.value + (@f.carry ? 1 : 0), @f)
            when 0x89 #ADC A,C
                @a.add(@c.value + (@f.carry ? 1 : 0), @f)
            when 0x8A #ADC A,D
                @a.add(@d.value + (@f.carry ? 1 : 0), @f)
            when 0x8B #ADC A,E
                @a.add(@e.value + (@f.carry ? 1 : 0), @f)
            when 0x8C #ADC A,H
                @a.add(@h.value + (@f.carry ? 1 : 0), @f)
            when 0x8D #ADC A,L
                @a.add(@l.value + (@f.carry ? 1 : 0), @f)
            when 0x8E #ADC A,(HL)
                @a.add(@memory[@hl.value].value + (@f.carry ? 1 : 0), @f)
                t_states = 7
            when 0x8F #ADC A,A
                @a.add(@a.value + (@f.carry ? 1 : 0), @f)
            when 0x90 #SUB A,B
                @a.sub(@b.value, @f)
            when 0x91 #SUB A,C
                @a.sub(@c.value, @f)
            when 0x92 #SUB A,D
                @a.sub(@d.value, @f)
            when 0x93 #SUB A,E
                @a.sub(@e.value, @f)
            when 0x94 #SUB A,H
                @a.sub(@h.value, @f)
            when 0x95 #SUB A,L
                @a.sub(@l.value, @f)
            when 0x96 #SUB A,(HL)
                @a.sub(@memory[@hl.value].value, @f)
                t_states = 7
            when 0x97 #SUB A,A
                @a.sub(@a.value, @f)
            when 0x98 #SBC A,B
                @a.sub(@a.value + (@f.carry ? 1 : 0), @f)
            when 0x99 #SBC A,C
                @a.sub(@c.value + (@f.carry ? 1 : 0), @f)
            when 0x9A #SBC A,D
                @a.sub(@d.value + (@f.carry ? 1 : 0), @f)
            when 0x9B #SBC A,E
                @a.sub(@e.value + (@f.carry ? 1 : 0), @f)
            when 0x9C #SBC A,H
                @a.sub(@h.value + (@f.carry ? 1 : 0), @f)
            when 0x9D #SBC A,L
                @a.sub(@l.value + (@f.carry ? 1 : 0), @f)
            when 0x9E #SBC A,(HL)
                @a.sub(@memory[@hl.value].value + (@f.carry ? 1 : 0), @f)
                t_states = 7
            when 0x9F #SBC A,A
                @a.sub(@a.value + (@f.carry ? 1 : 0), @f)
            when 0xA0 #AND A,B
                @a.and(@b.value, @f)
            when 0xA1 #AND A,C
                @a.and(@c.value, @f)
            when 0xA2 #AND A,D
                @a.and(@d.value, @f)
            when 0xA3 #AND A,E
                @a.and(@e.value, @f)
            when 0xA4 #AND A,H
                @a.and(@h.value, @f)
            when 0xA5 #AND L
                @a.and(@l.value, @f)
            when 0xA6 #AND (HL)
                @a.and(@memory[@hl.value].value, @f)
                t_states = 7
            when 0xA7 #AND A
                @a.and(@a.value, @f)
            when 0xA8 #XOR A,B
                @a.xor(@b.value, @f)
            when 0xA9 #XOR A,C
                @a.xor(@c.value, @f)
            when 0xAA #XOR A,D
                @a.xor(@d.value, @f)
            when 0xAB #XOR A,E
                @a.xor(@e.value, @f)
            when 0xAC #XOR A,H
                @a.xor(@h.value, @f)
            when 0xAD #XOR A,L
                @a.xor(@l.value, @f)
            when 0xAE #XOR A,(HL)
                @a.xor(@memory[@hl.value].value, @f)
                t_states = 7
            when 0xAF #XOR A,A
                @a.xor(@a.value, @f)
            when 0xB0 #OR A,B
                @a.or(@b.value, @f)
            when 0xB1 #OR A,C
                @a.or(@c.value, @f)
            when 0xB2 #OR A,D
                @a.or(@d.value, @f)
            when 0xB3 #OR A,E
                @a.or(@e.value, @f)
            when 0xB4 #OR A,H
                @a.or(@h.value, @f)
            when 0xB5 #OR A,L
                @a.or(@l.value, @f)
            when 0xB6 #OR A,(HL)
                @a.or(@memory[@hl.value].value, @f)
                t_states = 7
            when 0xB7 #OR A,A
                @a.or(@a.value, @f)
            when 0xB8 #CP A,B
                @f.flag_z = (@a.value == @b.value)
            when 0xB9 #CP A,C
                @f.flag_z = (@a.value == @c.value)
            when 0xBA #CP A,D
                @f.flag_z = (@a.value == @d.value)
            when 0xBB #CP A,E
                @f.flag_z = (@a.value == @e.value)
            when 0xBC #CP A,H
                @f.flag_z = (@a.value == @h.value)
            when 0xBD #CP A,L
                @f.flag_z = (@a.value == @l.value)
            when 0xBE #CP A,(HL)
                @f.flag_z = (@a.value == @memory[@hl.value].value)
            when 0xBF #CP A,A
                @f.flag_z = true
            else
                fail
            end
            @pc += op_size
            sleep (t + t_states * @state_duration - Time.now) / 1000.0
            return true
        end
    end

end

#TODO: how to set carry (for example on ADD A,A) ??
z80 = Z80::Z80.new
#z80.run
